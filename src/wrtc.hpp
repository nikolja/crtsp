#pragma once

#ifndef __WRTC_HPP
#define __WRTC_HPP

#include <map>
#include <regex>
#include <mutex>
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <variant>
#include <utility>
#include <iostream>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <unordered_map>

// gst
#define GST_USE_UNSTABLE_API
// webrtc
#include <gst/gst.h>
#include <gst/gstpad.h>
#include <gst/sdp/sdp.h>
#include <gst/webrtc/webrtc.h>
// json
#include <nlohmann/json.hpp>
// httplib
#include <httplib.h>

#include <log.hpp>
#include <gst.hpp>
#include <utils.hpp>

#ifdef _MSC_VER
#	pragma warning( push )
#endif // #ifdef _MSC_VER

namespace wrtc {

struct gparams_t {
    using value_t = std::variant<std::string, int>;
    using option_t = std::pair<std::string, value_t>;
    using options_t = std::vector<option_t>;

    gparams_t(std::initializer_list<option_t> init) : options(init) {}

    void set_option(std::string const& name, value_t const& value) {
        auto it = std::find_if( options.begin(), options.end(), [&name](const option_t& opt) { return opt.first == name; } );
        if (it != options.end()) {
            it->second = value;
        } else {
            options.emplace_back(name, value);
        }
    }

    std::optional<value_t> get_option(const std::string& name) {
        auto it = std::find_if( options.begin(), options.end(), [&name](const option_t& opt) { return opt.first == name; } );
        if (it != options.end()) {
            return it->second;
        } else {
            return std::nullopt;
        }
    }

    void clear() {
        options.clear();
    }

    void apply(GstElement* element) {
        if (!element)
            return;
        for (const auto& [name, value] : options) {
            if (std::holds_alternative<std::string>(value)) {
                g_object_set(element, name.c_str(), std::get<std::string>(value).c_str(), NULL);
            } else if (std::holds_alternative<int>(value)) {
                g_object_set(element, name.c_str(), std::get<int>(value), NULL);
            }
        }
    }

    options_t options;
};

struct webrtc_session : public std::enable_shared_from_this<webrtc_session> {

    using ptr = std::shared_ptr<webrtc_session>;
    using clock_tp = std::chrono::steady_clock;

    enum class state_t { 
        created, 
        waiting_for_ice, 
        ready, 
        disconnected
    };

    state_t state{ state_t::created };
    clock_tp::time_point last_activity;

    webrtc_session(std::string const& peer_id): peer_id(peer_id) {
        LOG_INFO_FMT( "[{}] webrtc_session create", peer_id );
        if (is_pipeline_shared() && reset_on_create) {
            bool const playing{ is_playing() };
            if (playing && state_switching) {
                state_ready();
            }
            bool const success{ reset() };
            if (!success) {
                LOG_ERROR_FMT( "[{}] failed to reset webrtc session", peer_id );
            }
            if (!success && playing && state_switching) {
                state_playing();
            }
        }
    }

    ~webrtc_session() {
        cleanup();
        cleanup_pipeline_cust();
    }

    bool state_ready() {
        if (!get_pipeline())
            return false;
        auto ret = gst_element_set_state(get_pipeline(), GST_STATE_READY);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOG_WARNING_FMT( "[{}] failed to set pipeline to READY state", peer_id );
            return false;
        }
        return true;
    }

    bool state_playing() {
        if (!get_pipeline())
            return false;
        auto ret = gst_element_set_state(get_pipeline(), GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOG_WARNING_FMT( "[{}] failed to set pipeline to PLAYING state", peer_id );
            return false;
        }
        return true;
    }

    bool state_null() {
        if (!get_pipeline())
            return false;
        auto ret = gst_element_set_state(get_pipeline(), GST_STATE_NULL);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOG_WARNING_FMT( "[{}] failed to set pipeline to NULL state", peer_id );
            return false;
        }
        return true;
    }

    bool is_playing() {
        if (!get_pipeline())
            return false;
        GstState current, pending;
        GstClockTime timeout = GST_SECOND; //0
        GstStateChangeReturn ret = gst_element_get_state(get_pipeline(), &current, &pending, timeout);
        if (!ret) {
            LOG_WARNING_FMT( "[{}] unable to query pipeline state", peer_id );
            return false;
        }
        return current == GST_STATE_PLAYING || (current > GST_STATE_NULL && pending == GST_STATE_PLAYING);
    }

    void cleanup(bool restore_playing = true, bool unlink_from_pipeline = true) {
        LOG_INFO_FMT( "[{}] cleanup({})", peer_id, restore_playing );
        bool const playing{ is_playing() };
        if (playing && state_switching && is_pipeline_shared()) {
            state_ready();
        }
        if (rtppay) {
            gst_element_set_state(rtppay, GST_STATE_NULL);
            GstElement* parent = GST_ELEMENT(gst_element_get_parent(rtppay)); // pipeline_shared
            if (parent && unlink_from_pipeline) {
                gst_bin_remove(GST_BIN(parent), rtppay);
                //gst_object_unref(parent);
            }
            gst_object_unref(rtppay);
            rtppay = nullptr;
        }
        if (ident) {
            gst_element_set_state(ident, GST_STATE_NULL);
            GstElement* parent = GST_ELEMENT(gst_element_get_parent(ident)); // pipeline_shared
            if (parent && unlink_from_pipeline) {
                gst_bin_remove(GST_BIN(parent), ident);
                //gst_object_unref(parent);
            }
            gst_object_unref(ident);
            ident = nullptr;
        }
        if (queue) {
            gst_element_set_state(queue, GST_STATE_NULL);
            GstElement* parent = GST_ELEMENT(gst_element_get_parent(queue)); // pipeline_shared
            if (parent && unlink_from_pipeline) {
                gst_bin_remove(GST_BIN(parent), queue);
                //gst_object_unref(parent);
            }
            gst_object_unref(queue);
            queue = nullptr;
        }
        if (teesrcpad) {
            GstElement* tee = gst_pad_get_parent_element(teesrcpad);
            //GstElement *tee = gst_bin_get_by_name(GST_BIN(pipeline_shared), tee_name.c_str());
            //GstElement *tee = pipeline_shared ? gst_bin_get_by_name(GST_BIN(pipeline_shared), tee_name.c_str()) : nullptr;
            if (tee) {
                gst_element_release_request_pad(tee, teesrcpad);
                gst_object_unref(tee);
            }
            gst_object_unref(teesrcpad);
            teesrcpad = nullptr;
        }
        if (webrtcbin) {
            gst_element_set_state(webrtcbin, GST_STATE_NULL);
            GstElement* parent = GST_ELEMENT(gst_element_get_parent(webrtcbin)); // pipeline_shared
            if (parent && unlink_from_pipeline) {
                gst_bin_remove(GST_BIN(parent), webrtcbin);
                //gst_object_unref(parent);
            }
            gst_object_unref(webrtcbin);
            webrtcbin = nullptr;
        }
        if (transceiver && webrtcbin_shared) {
            transceiver_to_session.erase(transceiver);
            transceiver = nullptr;
        }
        if (playing && restore_playing && state_switching && is_pipeline_shared()) {
            state_playing();
        }
    }
    
    bool reset() {
        LOG_INFO_FMT( "[{}] reset() call #{}", peer_id, ++reset_count );

        if (is_pipeline_cust() || is_pipeline_desc()) {
            if (reset_count > 1) 
                LOG_INFO_FMT( "[{}] reset() called more than once", peer_id );
            if (reset_count > 1 || !is_pipeline_cust())
                restart_pipeline_cust();
            if (!is_pipeline_cust()) {
                LOG_ERROR_FMT( "[{}] failed to reset pipeline", peer_id );
                return false;
            }
            if (!is_webrtcbin_linked()) {
                LOG_ERROR_FMT( "[{}] failed to link webrtcbin to pipeline", peer_id );
                return false;
            }
            last_activity = clock_tp::now();
            state = state_t::waiting_for_ice;
            
            LOG_INFO_FMT( "[{}] webrtcbin linked to pipeline", peer_id );
            
            return true;
        }
    
        if (!pipeline_shared) {
            LOG_ERROR_FMT( "[{}] pipeline is not initialized", peer_id );
            return false;
        }
    
        cleanup(false);
    
        GstElement* tee = gst_bin_get_by_name(GST_BIN(pipeline_shared), tee_name.c_str());
        if (!tee) {
            LOG_ERROR_FMT( "[{}] failed to get tee element from pipeline", peer_id );
            return false;
        }

        queue = gst_element_factory_make("queue", nullptr);
        rtppay = rtppay_shared ? nullptr : gst_element_factory_make(rtppay_elem.c_str(), nullptr);
        webrtcbin = webrtcbin_shared ? nullptr : gst_element_factory_make("webrtcbin", /*webrtcbin_name.c_str()*/nullptr);
    
        if (!queue || (!rtppay_shared && !rtppay) || (!webrtcbin && !webrtcbin_shared)) {
            LOG_ERROR_FMT( "[{}] failed to create branch elements", peer_id );
            return false;
        }
    
        queue_params.apply(queue); //g_object_set(queue, "leaky", 2, "max-size-buffers", 1, /*"max-size-bytes", 0, "max-size-time", 0, */NULL);
        rtppay_params.apply(rtppay); //if (rtppay) g_object_set(rtppay, "pt", rtppay_payload, NULL);
        if (webrtcbin) {
            webrtcbin_params.apply(webrtcbin);
            //g_object_set(webrtcbin, "stun-server", stun_server.c_str(), NULL);
            //g_object_set(webrtcbin, "bundle-policy", bundle_policy, NULL);
            if (rtppay)
                gst_bin_add_many(GST_BIN(pipeline_shared), queue, rtppay, webrtcbin, nullptr);
            else
                gst_bin_add_many(GST_BIN(pipeline_shared), queue, webrtcbin, nullptr);
            g_signal_connect(
                webrtcbin, "on-negotiation-needed",
                G_CALLBACK(+[](GstElement* bin, gpointer user_data) {
                    auto *self = static_cast<webrtc_session*>(user_data);
                    LOG_INFO_FMT( "[{}] on-negotiation-needed triggered for element {}", self->peer_id, GST_ELEMENT_NAME(bin) );
                }),
                this
            );
        } else {
            if (rtppay)
                gst_bin_add_many(GST_BIN(pipeline_shared), queue, rtppay, nullptr);
            else
                gst_bin_add_many(GST_BIN(pipeline_shared), queue, nullptr);
        }
    
        // identity (optional)
        if (identity_using) {
            ident = gst_element_factory_make("identity", nullptr);
            if (!ident) {
                LOG_ERROR_FMT( "[{}] failed to create identity element", peer_id );
                cleanup();
                return false;
            }
            identity_params.apply(ident); //g_object_set(ident, "sync", FALSE, "drop-allocation", TRUE, "signal-handoffs", TRUE, "silent", TRUE, NULL);
            gst_bin_add(GST_BIN(pipeline_shared), ident);
            if (!gst_element_sync_state_with_parent(ident)) {
                LOG_ERROR_FMT( "[{}] failed to sync state with parent for identity", peer_id );
                cleanup();
                return false;
            }
            if (!gst_element_link_many(queue, ident, rtppay ? rtppay : get_webrtcbin(), nullptr)) {
                LOG_ERROR_FMT( "[{}] failed to link queue to identity to rtppay", peer_id );
                cleanup();
                return false;
            }
        } else {
            if (!gst_element_link(queue, rtppay ? rtppay : get_webrtcbin())) {
                LOG_ERROR_FMT("[{}] failed to link queue to {}", peer_id, rtppay ? "rtppay" : "webrtcbin");
                cleanup();
                return false;
            }
        }
    
        if (!gst_element_sync_state_with_parent(queue) || (rtppay && !gst_element_sync_state_with_parent(rtppay))) {
            LOG_ERROR_FMT( "[{}] failed to sync queue or rtppay", peer_id );
            cleanup();
            return false;
        }

        if (webrtcbin && !gst_element_sync_state_with_parent(webrtcbin)) {
            LOG_ERROR_FMT( "[{}] failed to sync webrtcbin", peer_id );
            cleanup();
            return false;
        }
    
        // link tee to queue
        teesrcpad = gst_element_get_request_pad(tee, "src_%u");
        GstPad* queue_sink = gst_element_get_static_pad(queue, "sink");
        if (gst_pad_link(teesrcpad, queue_sink) != GST_PAD_LINK_OK) {
            LOG_ERROR_FMT( "[{}] failed to link tee to queue", peer_id );
            gst_object_unref(queue_sink);
            cleanup();
            return false;
        }
        gst_object_unref(queue_sink);

        // add-transceiver logic (shared mode only)
        if (transceiver_adding) {
            std::string caps_transceiver_str{ caps_transceiver() };
            GstCaps* caps = gst_caps_from_string(caps_transceiver_str.c_str());
            //GstCaps* caps = gst_caps_new_simple("application/x-rtp", "media", G_TYPE_STRING, "video", "encoding-name", G_TYPE_STRING, encoder_format.c_str(), "payload", G_TYPE_INT, rtppay_payload);
            if (!caps) {
                LOG_ERROR_FMT( "[{}] failed to create caps from string: {}", peer_id, caps_transceiver_str );
                cleanup();
                return false;
            }
            g_signal_emit_by_name(get_webrtcbin(), "add-transceiver", GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, caps, &transceiver);
            if (!transceiver) {
                LOG_ERROR_FMT( "[{}] failed to add transceiver", peer_id );
                cleanup();
                return false;
            }
            if (webrtcbin_shared)
                transceiver_to_session[transceiver] = shared_from_this();
            gst_caps_unref(caps);
        }
        GstPad* trans_sink = gst_element_get_request_pad(get_webrtcbin(), "sink_%u");
    
        // link rtppay to trans_sink
        if (!is_rtppay_shared()) {
            GstPad* pay_src = gst_element_get_static_pad(rtppay, "src");
            if (!pay_src || !trans_sink  || gst_pad_link(pay_src, trans_sink) != GST_PAD_LINK_OK) {
                LOG_ERROR_FMT( "[{}] failed to link rtppay to trans_sink", peer_id );
                if (pay_src) gst_object_unref(pay_src);
                if (trans_sink) gst_object_unref(trans_sink);
                cleanup();
                return false;
            }
            gst_object_unref(pay_src);
        }
        gst_object_unref(trans_sink);
    
        // connect ICE signal (only if using local webrtcbin)
        if (webrtcbin)
            g_signal_connect(webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate_static), this);
    
        last_activity = clock_tp::now();
        state = state_t::waiting_for_ice;
    
        GstState curr, pend;
        bool const playing{
            pipeline_shared && gst_element_get_state(pipeline_shared, &curr, &pend, 0) && 
            (curr == GST_STATE_PLAYING || (curr > GST_STATE_NULL && pend == GST_STATE_PLAYING))
        };
        auto ret = state_switching && !playing ? gst_element_set_state(pipeline_shared, GST_STATE_PLAYING) : GST_STATE_CHANGE_SUCCESS;
        if (ret == GST_STATE_CHANGE_FAILURE) {
            LOG_WARNING_FMT( "[{}] failed to set pipeline to PLAYING state", peer_id );
        }
    
        LOG_INFO_FMT( "[{}] webrtcbin{} reset and linked to pipeline", peer_id, webrtcbin_shared ? "_shared" : "" );
    
        return true;
    }

    void refresh_activity() {
        last_activity = clock_tp::now();
    }

    void set_remote_description(GstWebRTCSessionDescription *desc) {
        if (!get_webrtcbin()) return;

        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(get_webrtcbin(), "set-remote-description", desc, promise);
        gst_promise_wait(promise);
        gst_promise_unref(promise);

        LOG_INFO_FMT( "[{}] remote description set", peer_id );

        if (is_webrtcbin_shared()) {
            // transceiver reassociation
            GArray* transceivers = nullptr;
            g_signal_emit_by_name(webrtcbin_shared, "get-transceivers", &transceivers);
            if (transceivers) {
                LOG_INFO_FMT( "[{}] webrtcbin_shared has {} transceivers", peer_id, transceivers->len );
                for (guint i = 0; i < transceivers->len; ++i) {
                    GstWebRTCRTPTransceiver* xcv = g_array_index(transceivers, GstWebRTCRTPTransceiver*, i);
                    //if (!GST_IS_WEBRTC_RTP_TRANSCEIVER(xcv)) continue;
                    if (xcv) {
                        gchar* mid = nullptr;
                        GstWebRTCRTPTransceiverDirection direction;
                        g_object_get(xcv, "direction", &direction, "mid", &mid, NULL);
                        LOG_INFO_FMT( "[{}] checking transceiver[{}] mid={} dir={}", peer_id, i, mid ? mid : "NULL", direction_to_string(direction) );
                        g_free(mid);
                        // clean up any previous transceivers registered for this session
                        for (auto it = transceiver_to_session.begin(); it != transceiver_to_session.end();) {
                            if (auto sess = it->second.lock()) {
                                if (sess.get() == this) {
                                    it = transceiver_to_session.erase(it);
                                    continue;
                                }
                            }
                            ++it;
                        }
                        transceiver = xcv;
                        transceiver_to_session[xcv] = shared_from_this();
                        break;
                    }
                }
                g_array_unref(transceivers);
            }
        }

        // replay pending ICE candidates
        {
            std::lock_guard<std::mutex> lock(pending_mutex);
            for (auto const& c : pending_candidates) {
                LOG_INFO_FMT( "[{}] re-emitting delayed ICE candidate (post set-remote-description)", peer_id );
                g_signal_emit_by_name(get_webrtcbin(), "add-ice-candidate", c.mlineindex, c.candidate.c_str());
            }
            if (!webrtcbin_shared)
                pending_candidates.clear();
        }
    }

    bool apply_offer_sdp(std::string const& sdp) {
        GstSDPMessage *sdp_msg;
        if (gst_sdp_message_new_from_text(sdp.c_str(), &sdp_msg) != GST_SDP_OK) {
            LOG_ERROR_FMT("[{}] invalid SDP offer", peer_id);
            return false;
        }
        GstWebRTCSessionDescription *offer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_OFFER, sdp_msg);
        set_remote_description(offer);
        gst_webrtc_session_description_free(offer);
        return true;
    }
    
    bool set_remote_offer(const std::string& sdp) {
        last_activity = clock_tp::now();
        if (!get_webrtcbin() || !GST_IS_ELEMENT(get_webrtcbin())) {
            pending_offer_sdp = sdp;
            LOG_INFO_FMT( "[{}] storing remote offer for later (webrtcbin not ready)", peer_id );
            return true;
        }
        return apply_offer_sdp(sdp);
    }

    void replay_local_ice_candidates() {
        if (!get_webrtcbin()) return;
    
        std::lock_guard<std::mutex> lock(candidates_mutex);

        LOG_INFO_FMT( "[{}] ICE restart requested, replaying local ICE candidates", peer_id );
    
        for (const auto& c : candidates) {
            LOG_INFO_FMT("[{}] manually replaying ICE: mlineindex={}, candidate={}", peer_id, c["sdpMLineIndex"].get<int>(), c["candidate"].get<std::string>());
            g_signal_emit_by_name(get_webrtcbin(), "add-ice-candidate", c["sdpMLineIndex"].get<int>(), c["candidate"].get<std::string>().c_str());
        }
        for (const auto& c : pending_candidates) {
            LOG_INFO_FMT("[{}] manually replaying ICE: mlineindex={}, candidate={}", peer_id, c.mlineindex, c.candidate);
            g_signal_emit_by_name(get_webrtcbin(), "add-ice-candidate", c.mlineindex, c.candidate.c_str());
        }

        LOG_INFO_FMT( "[{}] replay_local_ice_candidates(): candidates.size()={}, pending_candidates.size()={}", peer_id, candidates.size(), pending_candidates.size() );
    }

    void force_encoder_sdp(std::string& sdp, std::string const& expected_codec, std::string expected_pt = "") {
        // example: a=rtpmap:96 VP8/90000
        std::regex rtpmap_regex(R"(a=rtpmap:(\d+)\s+([A-Za-z0-9]+)/(\d+))");
        std::smatch match;
        std::string result;
        std::string::const_iterator search_start = sdp.cbegin();
        std::size_t last_pos = 0;
        std::string last_pt;

        while (std::regex_search(search_start, sdp.cend(), match, rtpmap_regex)) {
            std::size_t match_pos = match.position(0) + std::distance(sdp.cbegin(), search_start);
            std::size_t match_len = match.length(0);

            std::string full_line = match[0];
            std::string pt = match[1];
            std::string codec = match[2];
            std::string rate = match[3];
            last_pt = pt;

            // append unchanged text before this match
            result.append(sdp, last_pos, match_pos - last_pos);

            if (codec != expected_codec || (!expected_pt.empty() && pt != expected_pt)) {
                std::string new_line = "a=rtpmap:" + pt + " " + expected_codec + "/" + rate;
                result.append(new_line);
                LOG_INFO_FMT("force_encoder_sdp(): replaced '{}' with '{}'", full_line, new_line);
            } else {
                result.append(full_line);
            }

            last_pos = match_pos + match_len;
            search_start = sdp.cbegin() + last_pos;
        }

        // append remaining text
        result.append(sdp, last_pos, std::string::npos);
        sdp.swap(result);

        if (!expected_pt.empty()) {
            // replace payload type in m=video
            std::regex mline_regex(R"(m=video\s+\d+\s+\S+\s+[\d ]+)");
            sdp = std::regex_replace(sdp, mline_regex, "m=video 9 UDP/TLS/RTP/SAVPF " + expected_pt);
            // replace payload lineswith a=
            std::regex fmtp_regex("a=fmtp:" + last_pt + R"(( [^\r\n]*)(\r?\n))");
            std::regex rtcp_fb_regex("a=rtcp-fb:" + last_pt + R"(( [^\r\n]*)(\r?\n))");
            std::regex artpmap_regex("a=rtpmap:" + last_pt + R"(( [^\r\n]*)(\r?\n))");
            sdp = std::regex_replace(sdp, fmtp_regex, "a=fmtp:" + expected_pt + "$1$2");
            sdp = std::regex_replace(sdp, rtcp_fb_regex, "a=rtcp-fb:" + expected_pt + "$1$2");
            sdp = std::regex_replace(sdp, artpmap_regex, "a=rtpmap:" + expected_pt + "$1$2");
            if (last_pt != expected_pt) {
                LOG_INFO_FMT("force_encoder_sdp(): replaced '{}' with '{}'", last_pt, expected_pt);
            }
        }
    }
    
    std::string create_answer_json() {
        if (!get_webrtcbin()) {
            LOG_ERROR_FMT( "[{}] webrtcbin is null in create_answer_json", peer_id );
            return {};
        }

        last_activity = clock_tp::now();
        bool const restart_ice = (webrtcbin_shared && reset_count > 1);
        LOG_INFO_FMT( "[{}] create_answer_json(): is_webrtcbin_shared()={} reset_count={}", peer_id, is_webrtcbin_shared(), reset_count );

        GstStructure* opts_struct = gst_structure_new_empty("GstWebRTCSessionDescriptionOptions");
        gst_structure_set(opts_struct, "iceRestart", G_TYPE_BOOLEAN, TRUE, nullptr);

        GstPromise *promise = gst_promise_new();
        g_signal_emit_by_name(get_webrtcbin(), "create-answer", opts_struct, promise);
        gst_promise_wait(promise);
        gst_structure_free(opts_struct);
    
        GstWebRTCSessionDescription *answer = nullptr;
        const GstStructure *reply = gst_promise_get_reply(promise);
        if (!gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL)) {
            LOG_ERROR_FMT( "[{}] failed to get answer from GstStructure", peer_id );
            gst_promise_unref(promise);
            return R"({"type": "answer", "sdp": "", "candidates": []})";
        }
        if (!answer) {
            LOG_ERROR_FMT( "[{}] create_answer_json(): failed to create answer", peer_id );
            return {};
        }
    
        GstPromise *local_promise = gst_promise_new();
        g_signal_emit_by_name(get_webrtcbin(), "set-local-description", answer, local_promise);
        gst_promise_wait(local_promise);
        gst_promise_unref(local_promise);
        gst_promise_unref(promise);
    
        LOG_INFO_FMT( "[{}] local description set", peer_id );

        // wait up to for ice (candidates) gathering to complete
        for (int waited = 0; waited < ice_wait_ms; waited += ice_step_ms) {
            GstPromise *stats_promise = gst_promise_new();
            g_signal_emit_by_name(get_webrtcbin(), "get-stats", nullptr, stats_promise);
            gst_promise_wait(stats_promise);
            const GstStructure *stats = gst_promise_get_reply(stats_promise);
            if (stats) {
                const gchar *ice_state = gst_structure_get_string(stats, "ice-gathering-state");
                if (ice_state && std::string(ice_state) == "complete") {
                    gst_promise_unref(stats_promise);
                    break;
                }
            }
            gst_promise_unref(stats_promise);
            std::this_thread::sleep_for(std::chrono::milliseconds(ice_step_ms));
        }

        std::string sdp_safe;
        gchar *sdp_str = gst_sdp_message_as_text(answer->sdp);
        if (sdp_str) {
            sdp_safe = sdp_str;
            force_encoder_sdp(sdp_safe, utils::str_upper(encoder_format), std::to_string(rtppay_payload));
            g_free(sdp_str);
            if (sdpdebug_using) {
                LOG_INFO_FMT( "[{}] generated SDP:\n{}\n", peer_id, sdp_safe );
            } else {
                LOG_INFO_FMT( "[{}] generated SDP", peer_id );
            }
        } else {
            LOG_ERROR_FMT( "[{}] gst_sdp_message_as_text returned NULL", peer_id );
            sdp_safe = "";
        }

        nlohmann::json response_json;
        response_json["type"] = "answer";
        response_json["sdp"] = sdp_safe;
    
        {
            std::lock_guard<std::mutex> lock(candidates_mutex);
            response_json["candidates"] = candidates;
            if (!webrtcbin_shared)
                candidates.clear();
            sdp_message = sdp_safe; // store the SDP message for later use
        }

        // ICE candidates handled in set_remote_description()

        gst_webrtc_session_description_free(answer);

        // replay ICE candidates from previous session
        if (restart_ice)
            replay_local_ice_candidates();
    
        return response_json.dump();
    }

    void handle_pending_offer_if_any() {
        if (pending_offer_sdp.has_value()) {
            LOG_INFO_FMT("[{}] applying pending offer inside reset", peer_id);
            LOG_INFO_FMT("[{}] pending_offer_sdp = {}", peer_id, *pending_offer_sdp);
            apply_offer_sdp(*pending_offer_sdp);
            pending_offer_sdp.reset();
        }
    }
    
    void add_ice_candidate(int sdpmlineindex, std::string const& candidate) {
        last_activity = clock_tp::now();
        if (state < state_t::waiting_for_ice || !get_webrtcbin() || !GST_IS_ELEMENT(get_webrtcbin())) {
            std::lock_guard<std::mutex> lock(pending_mutex);
            pending_candidates.push_back({
                static_cast<uint32_t>(sdpmlineindex),
                candidate
            });
            LOG_WARNING_FMT( "[{}] ICE candidate delayed (webrtcbin not ready yet)", peer_id );
            return;
        }

        // apply delayed SDP offer if it exists
        if (pending_offer_sdp.has_value()) {
            LOG_INFO_FMT( "[{}] applying delayed SDP before ICE candidate", peer_id );
            apply_offer_sdp(*pending_offer_sdp);
            pending_offer_sdp.reset();
        }

        g_signal_emit_by_name(get_webrtcbin(), "add-ice-candidate", sdpmlineindex, candidate.c_str());
        //LOG_INFO_FMT( "[{}] added ICE candidate", peer_id );
        state = state_t::ready;
    }

    void set_pipeline_desc(std::string const& desc) {
        pipeline_desc = desc;
    }
    std::string const get_pipeline_desc() const { return pipeline_desc; }

    void restart_pipeline_cust() {
        if (is_pipeline_desc())
            makeup_pipeline_cust(pipeline_desc);
        else
            LOG_ERROR_FMT( "[{}] pipeline description is empty", peer_id );
    }

    GstElement* makeup_pipeline_cust(std::string const& desc) {
        pipeline_desc = desc;
        cleanup_pipeline_cust();
        GstElement* pipeline = gst_parse_launch(pipeline_desc.c_str(), nullptr);
        if (!pipeline) {
            LOG_ERROR_FMT( "[{}] failed to create pipeline from description: {}", peer_id, pipeline_desc );
            return nullptr;
        }
        LOG_INFO_FMT( "[{}] created pipeline from description: {}", peer_id, pipeline_desc );
        set_pipeline_cust(pipeline);
        return pipeline;
    }

    void cleanup_pipeline_cust() {
        if (!pipeline_cust)
            return;
        gst_element_set_state(pipeline_cust, GST_STATE_NULL);
        gst_object_unref(pipeline_cust);
        pipeline_cust = nullptr;
        if (webrtcbin) {
            gst_object_unref(webrtcbin);
            webrtcbin = nullptr;
        }
        LOG_INFO_FMT( "[{}] cleaned up pipeline", peer_id );
    }

    void set_pipeline_cust(GstElement* pipeline) {
        cleanup_pipeline_cust();
        set_pipeline_shared(nullptr); // todo: reset shared pipeline ?
        pipeline_cust = pipeline;
        if (!pipeline_cust)
            return;
        webrtcbin = gst::element_by_name(pipeline_cust, webrtcbin_name);
        if (webrtcbin) {
            g_signal_connect(webrtcbin, "on-ice-candidate", G_CALLBACK(on_ice_candidate_static), this);
            g_signal_connect(webrtcbin, "on-negotiation-needed", G_CALLBACK(+[](GstElement* bin, gpointer user_data) {
                auto *self = static_cast<webrtc_session*>(user_data);
                LOG_INFO_FMT( "[{}] on-negotiation-needed triggered for element {}", self->peer_id, GST_ELEMENT_NAME(bin) );
            }), this);
        } else {
            LOG_ERROR_FMT( "[{}] failed to find webrtcbin in pipeline", peer_id );
        }
        GstState curr, pend;
        bool const playing{
            pipeline_cust && gst_element_get_state(pipeline_cust, &curr, &pend, 0) && 
            (curr == GST_STATE_PLAYING || (curr > GST_STATE_NULL && pend == GST_STATE_PLAYING))
        };
        auto ret = !playing ? gst_element_set_state(pipeline_cust, GST_STATE_PLAYING) : GST_STATE_CHANGE_SUCCESS;
        if (ret == GST_STATE_CHANGE_FAILURE)
            LOG_WARNING_FMT( "[{}] failed to set pipeline to PLAYING state", peer_id );
        else
            LOG_INFO_FMT( "[{}] pipeline_cust set to PLAYING", peer_id );
    }

    inline GstElement* get_pipeline_cust() { return pipeline_cust; }
    inline bool is_pipeline_desc() const { return !pipeline_desc.empty(); }
    inline bool is_pipeline_cust() const { return pipeline_cust != nullptr; }
    inline GstElement* get_pipeline() { return is_pipeline_cust() ? pipeline_cust : pipeline_shared; }

    static bool is_pipeline_shared() { return pipeline_shared != nullptr; }
    static void set_pipeline_shared(GstElement* pipeline) {
        pipeline_shared = pipeline;
        rtppay_shared = gst::element_by_name(pipeline, rtppay_name);
        webrtcbin_shared = gst::element_by_name(pipeline, webrtcbin_name);
        GstElement* tee = gst::element_by_name(pipeline, tee_name);
        if (!tee && rtppay_shared) {
            GstState current, pending;
            bool const playing{ 
                pipeline && gst_element_get_state(pipeline, &current, &pending, 0) && 
                (current == GST_STATE_PLAYING || (current > GST_STATE_NULL && pending == GST_STATE_PLAYING))
            };
            if (playing && state_switching) {
                gst_element_set_state(pipeline, GST_STATE_READY);
            }
            tee = gst_element_factory_make("tee", tee_name.c_str());
            if (tee) {
                gst_bin_add(GST_BIN(pipeline), tee);
                if (!gst_element_link(rtppay_shared, tee)) {
                    LOG_ERROR_FMT( "failed to link elements: {} -> {}", GST_ELEMENT_NAME(rtppay_shared), GST_ELEMENT_NAME(tee) );
                    return;
                }
                gst_element_sync_state_with_parent(tee);
            } else {
                LOG_ERROR_FMT( "failed to create tee element" );
                return;
            }
            if (playing && state_switching) {
                gst_element_set_state(pipeline, GST_STATE_PLAYING);
            }
        }
        LOG_INFO_FMT( "pipeline_shared set to {}", pipeline ? GST_ELEMENT_NAME(pipeline) : "nullptr" );
    }

    inline GstElement *get_webrtcbin() { return webrtcbin_shared ? webrtcbin_shared : webrtcbin; }
    static bool is_webrtcbin_shared() { return webrtcbin_shared && gst_element_get_parent(webrtcbin_shared) == GST_OBJECT(pipeline_shared); }
    inline bool is_webrtcbin_linked() const { return is_webrtcbin_shared() || (webrtcbin && gst_element_get_parent(webrtcbin) == GST_OBJECT(pipeline_cust ? pipeline_cust : pipeline_shared)); }

    inline GstElement *get_rtppay() { return rtppay_shared ? rtppay_shared : rtppay; }
    static bool is_rtppay_shared() { return rtppay_shared && gst_element_get_parent(rtppay_shared) == GST_OBJECT(pipeline_shared); }
    inline bool is_rtppay_linked() const { return is_rtppay_shared() || (rtppay && gst_element_get_parent(rtppay) == GST_OBJECT(pipeline_cust ? pipeline_cust : pipeline_shared)); }

    void handle_new_local_ice_candidate(guint mlineindex, gchar *candidate) {
        std::lock_guard<std::mutex> lock(candidates_mutex);
        candidates.push_back({
            {"candidate", candidate},
            {"sdpMLineIndex", mlineindex}
        });
        if (debugger_using)
            LOG_INFO_FMT( "[{}] stored ICE candidate: mlineindex={}", peer_id, mlineindex );
    }

    static void on_ice_candidate_static(GstElement*, guint mlineindex, gchar *candidate, gpointer user_data) {
        static_cast<webrtc_session*>(user_data)->handle_new_local_ice_candidate(mlineindex, candidate);
    }

    static void on_ice_candidate_for_shared(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer) {
        for (const auto& [xcv, weak_session] : transceiver_to_session) {
            if (gst_object_get_parent(GST_OBJECT(xcv)) == GST_OBJECT(webrtcbin)) {
                if (auto session = weak_session.lock()) {
                    session->handle_new_local_ice_candidate(mlineindex, candidate);
                }
            }
        }
        LOG_INFO_FMT( "on_ice_candidate_for_shared(): mlineindex={}, candidate={}", mlineindex, candidate );
    }

    static const char* direction_to_string(GstWebRTCRTPTransceiverDirection direction) {
        switch (direction) {
            case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY: return "sendonly";
            case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY: return "recvonly";
            case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV: return "sendrecv";
            case GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE: return "inactive";
            default: return "unknown";
        }
    }

    static std::string generate_uuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 8; ++i) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 4; ++i) ss << dis(gen);
        ss << "-4";
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-" << ((dis(gen) & 0x3) | 0x8);
        for (int i = 0; i < 3; ++i) ss << dis(gen);
        ss << "-";
        for (int i = 0; i < 12; ++i) ss << dis(gen);
        return ss.str();
    }

    static void cleanup_all() {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions.clear();
    }

    static void cleanup_shared(std::string const& active_peer = "") {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (it->first == active_peer || it->second->is_pipeline_cust()) {
                ++it;
                continue;
            }
            LOG_INFO_FMT( "cleaning up expired: {} (state: {})", it->first, static_cast<int>(it->second->state) );
            it = sessions.erase(it);
        }
    }

    static void cleanup_expired(std::string const& active_peer = "") {
        auto now = clock_tp::now();
        std::lock_guard<std::mutex> lock(sessions_mutex);
        for (auto it = sessions.begin(); it != sessions.end();) {
            if (it->first == active_peer/* || it->second->is_webrtcbin_linked()*/) {
                ++it;
                continue;
            }
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->last_activity);
            if (elapsed > session_timeout && !it->second->is_pipeline_cust() && (it->second->state == state_t::created || it->second->state == state_t::waiting_for_ice)) {
                LOG_INFO_FMT( "cleaning up expired: {} (state: {})", it->first, static_cast<int>(it->second->state) );
                it = sessions.erase(it);
            } else {
                ++it;
            }
        }
    }

    static ptr make_session(std::string const& peer_id) {
        auto session = std::make_shared<webrtc_session>(peer_id);
        if (!session->pipeline_init.empty())
            session->set_pipeline_desc(pipeline_init);
        return session;
    }

    static void offer_request(const httplib::Request &req, httplib::Response &res) {
        LOG_INFO_FMT( "received {} request", addr_offer );

        std::string peer_id;
        if (req.has_header(header_peer)) {
            peer_id = req.get_header_value(header_peer);
        } else {
            peer_id = generate_uuid();
            res.set_header(header_peer, peer_id);
        }

        LOG_INFO_FMT( "handling offer for peer_id={}", peer_id );

        ptr session;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            if (!multiple_peers) {
                sessions.clear();
            }
            if (!sessions.count(peer_id)) {
                sessions[peer_id] = on_make_session ? on_make_session(peer_id, req, res) : make_session(peer_id);
            }
            session = sessions[peer_id];
            if (state_switching && is_pipeline_shared()) session->state_ready();
        }

        auto offer_json = nlohmann::json::parse(req.body);
        if (!session->reset()) {
            {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                sessions.erase(peer_id);
            }
            res.status = 400;
            res.set_content("failed to reset WebRTC session", "text/plain");
            LOG_ERROR_FMT( "failed to reset WebRTC session peer_id={}", peer_id );
            return;
        }
        if (!session->set_remote_offer(offer_json["sdp"])) {
            res.status = 400;
            res.set_content("invalid SDP", "text/plain");
            LOG_ERROR_FMT( "invalid SDP peer_id={}", peer_id );
            {
                std::lock_guard<std::mutex> lock(sessions_mutex);
                sessions.erase(peer_id);
            }
            return;
        }

        std::string answer = session->create_answer_json();
        res.set_content(answer, "application/json");

        cleanup_expired(peer_id);

        LOG_INFO_FMT( "sent answer with ICE candidates peer_id={}", peer_id );
    }

    static void candidate_request(const httplib::Request &req, httplib::Response &res) {
        LOG_INFO_FMT( "received {} request", addr_candidate );

        if (!req.has_param("peer_id")) {
            res.status = 400;
            res.set_content("missing peer_id", "text/plain");
            LOG_ERROR_FMT( "missing peer_id" );
            return;
        }

        std::string peer_id = req.get_param_value("peer_id");
        LOG_INFO_FMT( "handling candidate for peer_id={}", peer_id );
        std::shared_ptr<webrtc_session> session;
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            if (!sessions.count(peer_id)) {
                res.status = 404;
                res.set_content("unknown peer_id", "text/plain");
                LOG_ERROR_FMT( "unknown peer_id={}", peer_id );
                return;
            }
            session = sessions[peer_id];
        }

        auto candidate_json = nlohmann::json::parse(req.body);
        session->add_ice_candidate(candidate_json["sdpMLineIndex"], candidate_json["candidate"]);
        res.set_content("ok", "text/plain");

        LOG_INFO_FMT( "added ICE candidate peer_id={}", peer_id );
    }

    static void load_content(const std::string& file, std::string& content) {
        if (file.empty()) {
            LOG_INFO_FMT("wrtc::webrtc_session: no content file specified, use default content");
            return;
        }

        // construct the path to the content file
        auto const& file_path{ file };
        if (!std::filesystem::exists(file_path)) {
            LOG_WARNING_FMT("wrtc::webrtc_session: content file {} doesn't exist", file_path);
            return;
        }

        // read the content file
        std::ifstream stream(file_path);
        if (!stream.is_open()) {
            LOG_WARNING_FMT("wrtc::webrtc_session: cannot open existing content file {} for reading", file_path);
            return;
        }

        std::stringstream buffer;
        buffer << stream.rdbuf();
        content = buffer.str();
    }

    static void code_request(const httplib::Request &req, httplib::Response &res) {
        load_content(content_file, content_code);
        res.set_content(content_code, "text/html");
    }

    static void status_request(const httplib::Request &req, httplib::Response &res) {
        // webrtc signaling server status
        using json = nlohmann::json;
        LOG_INFO_FMT( "received {} request", addr_stat );
        json root;
        // sessions
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            json peers = json::array();
            for (const auto& [peer_id, session] : sessions) {
                peers.push_back({
                    {"peer_id", peer_id},
                    {"state", static_cast<int>(session->state)},
                    {"playing", session->is_playing()},
                    {"reset_count", session->reset_count},
                    {"pending_offer", session->pending_offer_sdp.value_or("")},
                    {"sdp_message", session->sdp_message},
                    {"pipeline_desc", session->pipeline_desc}
                    //{"candidates", session->candidates}
                });
            }
            root["sessions"] = peers;
        }
        // main settings
        root["server"]["port"] = port;
        root["server"]["address"] = address;
        root["server"]["stun_server"] = stun_server;
        root["server"]["bundle_policy"] = bundle_policy;
        root["server"]["rtppay_payload"] = rtppay_payload;
        root["server"]["encoder_format"] = encoder_format;
        root["server"]["rtppay_elem"] = rtppay_elem;
        root["server"]["content_file"] = content_file;
        // flags
        root["flags"] = {
            {"identity_using", identity_using},
            {"debugger_using", debugger_using},
            {"sdpdebug_using", sdpdebug_using},
            {"multiple_peers", multiple_peers},
            {"reset_on_create", reset_on_create},
            {"state_switching", state_switching}
        };
        // ice timings
        root["ice"]["step_ms"] = ice_step_ms;
        root["ice"]["wait_ms"] = ice_wait_ms;
        // elements pipeline
        root["elements"] = {
            {"source_name", source_name},
            {"convert_name", convert_name},
            {"encoder_name", encoder_name},
            {"parser_name", parser_name},
            {"rtppay_name", rtppay_name},
            {"tee_name", tee_name},
            {"webrtcbin_name", webrtcbin_name}
        };
        // rest urls
        root["routes"] = {
            {"addr_code", addr_code},
            {"addr_stat", addr_stat},
            {"addr_api", addr_api},
            {"addr_offer", addr_offer},
            {"addr_candidate", addr_candidate},
            {"param_peer", param_peer},
            {"header_peer", header_peer}
        };
        // pipeline states
        root["pipeline"]["shared"] = is_pipeline_shared();
        root["pipeline"]["webrtcbin_shared"] = is_webrtcbin_shared();
        root["pipeline"]["rtppay_shared"] = is_rtppay_shared();
        root["pipeline"]["init"] = pipeline_init;
        res.set_content(root.dump(2), "application/json");
    }

    static void params_to_json(httplib::Params const& params, nlohmann::json &json) {
        for (auto& [key, val] : params) {
            try {
                json[key] = nlohmann::json::parse(val);
            } catch (...) {
                json[key] = val;
                LOG_WARNING_FMT( "failed to parse param {} value '{}', using as string", key, val );
            }
        }
    }

    static nlohmann::json params_to_json(httplib::Params const& params) {
        nlohmann::json json;
        params_to_json(params, json);
        return json;
    }

    static void command_request(const httplib::Request &req, httplib::Response &res) {
        try {
            auto json = req.method == "GET" ? params_to_json(req.params) : nlohmann::json::parse(req.body);
            if (!json.contains("command") || !json["command"].is_string()) {
                res.status = 400;
                res.set_content("missing or invalid 'command'", "text/plain");
                LOG_ERROR_FMT( "missing or invalid command" );
                return;
            }
            std::string cmd = json["command"];
            // check if command is in the map
            if (!cmds.count(cmd)) {
                // unknown command
                res.status = 404;
                res.set_content("unknown command: " + cmd, "text/plain");
                LOG_ERROR_FMT( "unknown command: {}", cmd );
                return;
            }
            auto& execute_func{ cmds[cmd] };
            if (execute_func)
                execute_func(json, res);
        } catch (std::exception &e) {
            res.status = 500;
            res.set_content(std::string("error parsing JSON: ") + e.what(), "text/plain");
            LOG_ERROR_FMT( "error parsing JSON: {}", e.what() );
        }
    }

    static void on_cmd_uuid(nlohmann::json& json, httplib::Response& res) {
        // handle the uuid command
        // http://<hostname>:<port>/api?command=uuid
        res.set_content(generate_uuid(), "text/plain");
    }

    static void on_cmd_close(nlohmann::json& json, httplib::Response& res) {
        // handle the disconnect command
        // http://<hostname>:<port>/api?command=disconnect&peer_id=...
        std::string const peer_id = json.value("peer_id", ""); //std::string const peer_id = json["peer_id"]
        if (peer_id.empty()) { //if (!json.contains("peer_id")) {
            res.status = 400;
            res.set_content("missing peer_id", "text/plain");
            LOG_ERROR_FMT( "missing peer_id" );
            return;
        }
        // peer disconnected handled
        LOG_INFO_FMT( "closing session for peer_id={}", peer_id );
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.erase(peer_id);
        }
        res.set_content("session closed", "text/plain");
    }

    static bool server_start() {
        server.Get(addr_code, code_request);
        server.Get(addr_stat, status_request);
        server.Post(addr_offer, offer_request);
        server.Post(addr_candidate, candidate_request);
        server.Get(addr_api, command_request);
        server.Post(addr_api, command_request);
        thread = std::thread(
            [&]() {
                LOG_INFO_FMT( "HTTP signaling server started on port {}", port );
                server.listen(address, port);
            }
        );
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!server.is_running()) {
            LOG_ERROR_FMT( "failed to start HTTP signaling server" );
            return false;
        }
        return true;
    }

    static void server_stop() {
        if (server.is_running()) {
            server.stop();
            thread.join();
            LOG_INFO_FMT( "HTTP signaling server stopped" );
        }
    }

    inline static std::thread thread;
    inline static httplib::Server server;
    inline static unsigned int port{ 8000 };
    inline static std::string address{ "0.0.0.0" };
    inline static int bundle_policy{ GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE };
    inline static std::string stun_server{ "stun://stun.l.google.com:19302" };

    inline static bool identity_using{ false };
    inline static bool debugger_using{ false };
    inline static bool sdpdebug_using{ false };
    inline static bool multiple_peers{ true };
    inline static bool reset_on_create{ false };
    inline static bool state_switching{ true };

    // wait up to for ice candidates
    inline static int ice_step_ms{ 50 };
    inline static int ice_wait_ms{ 500 };
    inline static int rtppay_payload{ 96 };
    //inline static bool rtppay_linked{ false };
    inline static std::string source_name{ "source" };
    inline static std::string convert_name{ "convert" };
    inline static std::string encoder_name{ "encoder" };
    inline static std::string parser_name{ "parser" };
    inline static std::string rtppay_name{ "pay" };
    inline static std::string tee_name{ "tee" };
    inline static std::string webrtcbin_name{ "webrtcbin" };
    inline static std::string encoder_format{ "VP8" };
    inline static std::string rtppay_elem{ "rtpvp8pay" };
    inline static gparams_t rtppay_params{ {"pt", rtppay_payload } };
    inline static gparams_t identity_params{ 
        {"sync", FALSE}, 
        {"drop-allocation", TRUE}, 
        {"signal-handoffs", TRUE}, 
        {"silent", TRUE} 
    };
    inline static gparams_t webrtcbin_params{ 
        {"stun-server", stun_server},
        {"bundle-policy", bundle_policy}//, {"latency", 0}
    };
    inline static gparams_t queue_params{ 
        {"leaky", 2}, {"max-size-buffers", 1}//, {"max-size-bytes", 0}, {"max-size-time", 0}
    };
    
    inline static bool transceiver_adding{ false };
    //inline static std::string encoder_caps{ "video/x-raw" }; // "video/x-vp8", "video/x-vp9"
    inline static std::string caps_transceiver() { return fmt::format("application/x-rtp,media=video,encoding-name={},payload={}", utils::str_upper(encoder_format), rtppay_payload); }

    inline static GstElement *pipeline_shared{ nullptr };
    inline static GstElement *webrtcbin_shared{ nullptr };
    inline static GstElement *rtppay_shared{ nullptr };

    inline static std::string addr_code{ "/" };
    inline static std::string addr_stat{ "/stat" };
    inline static std::string addr_api{ "/api" };
    inline static std::string addr_offer{ "/offer" };
    inline static std::string addr_candidate{ "/candidate" };
    inline static std::string param_peer{ "peer_id" };
    inline static std::string header_peer{ "X-Peer-ID" };
    inline static std::string content_file{ };
    #include "wrtc.inl"

    inline static const constexpr char* cmd_make_uuid{ "make_uuid" };
    inline static const constexpr char* cmd_disconnect{ "disconnect" };

    // commands api map for http server
    using cmd_t = std::function<void(nlohmann::json&, httplib::Response&)>;
    static inline std::unordered_map<std::string, cmd_t> cmds{
        { cmd_make_uuid, on_cmd_uuid },
        { cmd_disconnect, on_cmd_close }
    };

    static void push_command(std::string const& cmd, cmd_t func) {
        cmds[cmd] = func;
    }

    static void default_commands() {
        // register default commands
        cmds.clear();
        cmds[cmd_make_uuid] = on_cmd_uuid;
        cmds[cmd_disconnect] = on_cmd_close;
    }

    inline static std::mutex sessions_mutex;
    inline static std::chrono::seconds session_timeout{ 30 };
    inline static std::unordered_map<std::string, std::shared_ptr<webrtc_session>> sessions;
    // transceiver map for shared webrtcbin to route ICE
    static inline std::unordered_map<GstWebRTCRTPTransceiver*, std::weak_ptr<webrtc_session>> transceiver_to_session;

    static inline std::string pipeline_init{ };
    using make_func = std::function<ptr(
        std::string const&, const httplib::Request&, httplib::Response&
    )>;
    static inline make_func on_make_session{ nullptr };

private:
    struct ice_candidate {
        uint32_t mlineindex;
        std::string candidate;
    };

    int reset_count{ 0 };
    std::string peer_id{ };
    std::string sdp_message{ };
    std::string pipeline_desc{ };
    GstPad *teesrcpad{ nullptr };
    GstElement *queue{ nullptr };
    GstElement *ident{ nullptr };
    GstElement *rtppay{ nullptr };
    GstElement *webrtcbin{ nullptr };
    GstElement *pipeline_cust{ nullptr };
    std::vector<nlohmann::json> candidates;
    std::mutex candidates_mutex, pending_mutex;
    std::optional<std::string> pending_offer_sdp;
    std::vector<ice_candidate> pending_candidates;
    GstWebRTCRTPTransceiver* transceiver{ nullptr };
};

} // namespace wrtc

#ifdef _MSC_VER
#	pragma warning( pop )
#endif // #ifdef _MSC_VER

#endif // #ifndef __WRTC_HPP
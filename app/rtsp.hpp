#pragma once

#ifndef __RTSP_HPP
#define __RTSP_HPP

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <functional>
#include <string_view>

// cxxopts
#include <cxxopts.hpp>

// logging
#include <log.hpp>
#include <gst.hpp>
#include <opts.hpp>
#include <meta.hpp>
#include <json.hpp>
#include <utils.hpp>
#if (defined(WITH_HTTPLIB))
#include <wrtc.hpp>
#endif

namespace app {

constexpr const char* c_build_marker = "[alpha]";
inline std::string c_config_filename = "conf.json";

void logging() {
    #ifndef WITHOUT_SPDLOG
    applog::log::cfg().id = "rtsp";
    applog::log::cfg().file = "rtsp.log";
    applog::log();
    #endif // #ifndef WITHOUT_SPDLOG
}

void print_conf(std::string const& path) {
    try {
        if (!std::filesystem::exists(path))
            return;
        std::ifstream stream(path);
        if (!stream.is_open())
            return;
        std::string args_string;
        using nlm_json = nlohmann::json;
        nlm_json json = nlm_json::parse(stream);
        for (auto& [key, value] : json.items()) {
            std::string key_str{"--" + key};
            nlm_json value_json = static_cast<nlm_json>(value);
            std::string value_str = value_json.dump();
            std::erase_if(value_str, [](char quote) { return quote == '"'; });
            std::string arg{key_str + "=" + value_str + " "};
            args_string.append(arg);
        }
        stream.close();
        LOG_INFO_FMT("{}: {}", path, args_string);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("failed to parse JSON config from {}: {}", path, e.what());
    }
}

void print_info(int argc, const char* const argv[]) {
    std::string arguments;
    LOG_INFO_FMT( "build date: {} {} {}", __DATE__, __TIME__, app::c_build_marker);
    LOG_INFO_FMT( "{} --help to print help message", argv[0] );
    for (int i = 0; i < argc; ++i)
        arguments += std::string(argv[i]) + std::string(" ");
    LOG_INFO_FMT( "{}", arguments );
}

void print_pars(std::string const& info, opts::parser const& pars, bool just_result = true) {
    std::string arguments;
    auto const& map{ pars.to_map(just_result) };
    for (const auto& [key, value] : map)
        arguments += std::string("--") + key + std::string("=") + value + std::string(" ");
    LOG_INFO_FMT( "{}: {}", info, arguments );
}

struct rtsp_t {
    ~rtsp_t() {
        stop();
    }

    struct config_t {
        std::string source{ gst::def_camera_elem };
        std::string property{ gst::def_camera_prop };
        std::string mediatype{ gst::def_media_type };
        std::string framesize{ gst::def_camera_size };
        int framerate{ 30 };
        std::string format{ };
        bool vidconvert{ true };
        bool queueleaky{ false };
        std::string decode{ };
        std::string encoder{ gst::def_codec_key };
        std::string backend{ gst::def_backend_key };
        std::string encprop{ };
        int bitrate{ 1000 };
        std::string tuning{ gst::par_x264enc_tune };
        std::string preset{ gst::par_x264enc_speed_preset };
        int keyframes{ 10 };
        int payload{ 96 };
        int interval{ 1 };
        std::string rtspsink{ "0.0.0.0:8554/stream0" };
        bool rtspmcast{ true };
        int rtspmport{ 5600 };
        bool verbose{ true };
        #if (defined(WITH_HTTPLIB))
        int webrtctout{ 0 };
        int webrtcport{ (int)wrtc::webrtc_session::port };
        std::string webrtcstun{ wrtc::webrtc_session::stun_server };
        std::string webrtccont{ wrtc::webrtc_session::content_file };
        #endif

        inline auto const get_frame_size() const {
            return utils::str_to_size(framesize);
        }

        inline auto const get_frame_width() const {
            return get_frame_size().width;
        }

        inline auto const get_frame_height() const {
            return get_frame_size().height;
        }

        inline bool const extract_rtspsink(std::string& host, std::string& port, std::string& mount) const {
            std::stringstream stream( rtspsink );
            bool const is_host{ std::getline(stream, host, ':') };
            bool const is_port{ std::getline(stream, port, '/') };
            bool const is_mount{ std::getline(stream, mount) };
            return is_host && is_port && is_mount && !host.empty() && !port.empty() && !mount.empty();
        }

        inline bool const is_rtspsink_valid() const {
            std::string host, port, mount;
            return extract_rtspsink(host, port, mount) && !host.empty() && !port.empty() && !mount.empty();
        }

        inline std::string const get_rtspsink_host() const {
            std::string host, port, mount;
            if (extract_rtspsink(host, port, mount)) {
                return host;
            }
            return "";
        }

        inline std::string const get_rtspsink_port() const {
            std::string host, port, mount;
            if (extract_rtspsink(host, port, mount)) {
                return port;
            }
            return "";
        }

        inline std::string const get_rtspsink_mount() const {
            std::string host, port, mount;
            if (extract_rtspsink(host, port, mount)) {
                return mount;
            }
            return "";
        }

        void prepare() {
            // x264enc, x265enc
            gst::par_x264enc_bitrate = gst::par_x265enc_bitrate = std::to_string(bitrate); // kbits/sec
            gst::par_x264enc_tune = gst::par_x265enc_tune = tuning;
            gst::par_x264enc_speed_preset = gst::par_x265enc_speed_preset = preset;
            gst::par_x264enc_key_int_max = gst::par_x265enc_key_int_max = std::to_string(keyframes);
            // qsvh264enc, qsvh265enc
            // todo: tuning latency
            gst::par_qsv_h264enc_bitrate = gst::par_qsv_h265enc_bitrate = std::to_string(bitrate); // kbits/sec
            // v4l2h264enc, omxh264enc, omxh265enc
            // todo: tuning latency
            gst::par_v4l2_h264enc_video_bitrate = gst::par_omx_h264enc_bitrate = gst::par_omx_h265enc_bitrate = std::to_string(bitrate * 1000); // bits/sec
            // openh264enc
            // todo: tuning latency
            gst::par_open_h264enc_bitrate = std::to_string(bitrate * 1000); // bits/sec
            // vp8enc, vp9enc
            gst::par_vp8enc_target_bitrate = gst::par_vp9enc_target_bitrate = std::to_string(bitrate * 1000); // bits/sec
            // nvh264enc, nvh265enc
            gst::par_nv_h264enc_bitrate = gst::par_nv_h265enc_bitrate = std::to_string(bitrate); // kbit/sec
            gst::par_nv_h264enc_max_bitrate = gst::par_nv_h265enc_max_bitrate = std::to_string(bitrate); // kbit/sec
            // mfh264enc, mfh265enc
            gst::par_mf_h264enc_bitrate = gst::par_mf_h265enc_bitrate = std::to_string(bitrate); // kbit/sec
            gst::par_mf_h264enc_max_bitrate = gst::par_mf_h265enc_max_bitrate = std::to_string(bitrate); // kbit/sec
        }

        void setup(gst::encode_params_t& params) {
            prepare();
            params = gst::encode_params_t();
            params.setup(backend, encoder);
        }

        inline static const std::vector<std::string> descriptions {
            "video source (f.e. 'v4l2src', 'mfvideosrc', 'libcamerasrc', 'nvarguscamerasrc', ...)",
            "video properties (f.e. 'device=/dev/video0', 'device-index=0', 'camera-name=0', 'sensor-id=0', ...)",
            "video media type (f.e. 'video/x-raw', 'image/jpeg', ...)",
            "video resolution (supported: '240p','360p','480p','720p','960p','1080p' or 'W*H','WxH','W;H')",
            "video framerate (in frames per second)",
            "video format (f.e. 'UYVY','YUY2','YVYU','NV12','NV21','YV12','I420','BGRA','RGBA','GRAY8', ...)",
            "video convert using, avoid it if the camera already gives 'I420', 'NV12', ... format",
            "queue leaky using, to avoid frame accumulation if the encoder can't keep up",
            "video decoder if needed (f.e. 'jpegdec', 'avdec_mjpeg', 'qsvjpegdec', 'v4l2jpegdec', ...)",
            "codec for output stream (supported: 'h264','h265','vp8','vp9','mjpeg')",
            "backend for encoder (supported: 'gst-auto','gst-basic','gst-v4l2','gst-libav','gst-nv','gst-qsv','gst-open','gst-d3d11','gst-mf','gst-omx')",
            "codec properties (f.e. 'threads=4')",
            "encoder bitrate (in kbit/sec) (f.e. '1000')",
            "encoder tuning options (supported: 'stillimage','fastdecode','zerolatency')",
            "encoder preset name for speed/quality options (supported: 'ultrafast','superfast','veryfast','faster', ...)",
            "maximal distance between two key-frames ('0' = automatic)",
            "payload type of the output packets",
            "SPS/PPS insertion interval in seconds ('0' = disabled, '-1' = send with every IDR frame)",
            "rtsp stream server host:port/mount (f.e. '0.0.0.0:8554/stream0')",
            "rtsp stream multicast using",
            "rtsp stream multicast port",
            "verbose level using",
            #if (defined(WITH_HTTPLIB))
            "webrtc connection to source timeout (in ms)",
            "webrtc + http(web and api) port (http://<ip>:<port>, http://<ip>:<port>/log, http://<ip>:<port>/api)",
            "webrtc transport stun server (f.e. 'stun://stun.l.google.com:19302')",
            "webrtc content html/js file (f.e. 'client.html')",
            #endif
            //"load arguments from JSON",
            //"save arguments to JSON",
            //"save options to JSON",
            //"print help"
        };
    };

    std::string const pipeline() {
        // init gstreamer
        gst::initializer::get();
        // setup caps
        std::ostringstream sstream;
        auto insert_param = [](std::string_view src, std::string_view param) -> std::string {
            if (param.empty())
                return std::string(src);
            auto pos = src.find('!');
            if (pos != std::string_view::npos) {
                // insert before '!' with space
                return std::string(src.substr(0, pos)) + " " + std::string(param) + " " + std::string(src.substr(pos));
            } else {
                // just adding to end
                return std::string(src) + " " + std::string(param);
            }
        };
        bool const has_src_props{ !config.property.empty() };
        bool const has_cap_type{ !config.mediatype.empty() };
        bool const has_cap_width{ config.get_frame_width() > 0 };
        bool const has_cap_height{ config.get_frame_height() > 0 };
        bool const has_cap_frmrate{ config.framerate > 0 };
        bool const has_cap_format{ !config.format.empty() };
        bool const vidconvert_needed{ config.vidconvert };
        bool const queueleaky_needed{ config.queueleaky };
        bool const has_cap_decode{ !config.decode.empty() };
        std::string caps = has_cap_type ? config.mediatype : "";
        caps = has_cap_width ? caps + fmt::format("{}width={}", caps.empty() ? "" : ", ", config.get_frame_width()) : caps;
        caps = has_cap_height ? caps + fmt::format("{}height={}", caps.empty() ? "" : ", ", config.get_frame_height()) : caps;
        caps = has_cap_frmrate ? caps + fmt::format("{}framerate={}/1", caps.empty() ? "" : ", ", config.framerate) : caps;
        caps = has_cap_format ? caps + fmt::format("{}format={}", caps.empty() ? "" : ", ", config.format) : caps;
        bool const has_src_caps{ !caps.empty() };
        // setup encode
        config.setup(encode);
        // props adding
        std::string const encode_subpipe{ insert_param(encode.subpipe, config.encprop) };
        // setup rtppay
        //std::string rtppay = fmt::format("{} pt={} name={}", encode_params.rtppay, payload, "pay0");
        // making pipeline
        sstream 
            << config.source << (has_src_props ? " " + config.property : "") << " ! " // v4l2src ... ! (source)
            << (has_src_caps ? caps + " ! " : "")                                     // video/x-raw, ... ! (caps)
            << (has_cap_decode ? config.decode + " ! " : "" )                         // jpegdec ! (decode)
            << (vidconvert_needed ? fmt::format("{} ! ", encode.convert) : "")        // videoconvert ! (convert)
            << (queueleaky_needed ? "queue leaky=2 max-size-buffers=1 ! " : "")       // queue leaky=2 max-size-buffers=1 ! (queue)
            << encode_subpipe << " ! "                                                // x264enc ... (encode)
            << encode.rtppay << " pt=" << config.payload << " name=pay0";             // rtph264pay config-interval=1 ... (payload)
        return sstream.str();
    }

    #if (defined(WITH_HTTPLIB))
    bool webrtc() {
        // webrtc + http
        // stop previous sessions
        wrtc::webrtc_session::cleanup_all();
        // stop http server
        wrtc::webrtc_session::server_stop();
        // prepare webrtc
        std::string rtppay{ }, rtpdepay{ };
        wrtc::webrtc_session::port = config.webrtcport;
        wrtc::webrtc_session::stun_server = config.webrtcstun;
        wrtc::webrtc_session::content_file = config.webrtccont;
        wrtc::webrtc_session::rtppay_elem = encode.rtppay;
        wrtc::webrtc_session::encoder_format = utils::str_upper(encode.codeckey);
        if (wrtc::webrtc_session::encoder_format == "MJPEG")
            wrtc::webrtc_session::encoder_format = "JPEG";
        auto const decode_rtpdepay = utils::get_map_value(gst::map_rtpdepay_element_by_codec, encode.codec, fmt::format("invalid or unsupported rtp depayloader using '{}' codec", encode.codeckey));
        //wrtc::webrtc_session::rtppay_params.clear();
        if (encode.rtppay == "rtpvp8pay" || encode.rtppay == "rtpvp9pay") {
            wrtc::webrtc_session::rtppay_payload = 96;
            wrtc::webrtc_session::rtppay_params = { {"pt", 96 } };
            wrtc::webrtc_session::queue_params = { {"leaky", 2}, {"max-size-buffers", 1} };
            rtpdepay = fmt::format("{} name={}", decode_rtpdepay, "rtpdepay");
            rtppay = fmt::format("{} name={} pt={}", encode.rtppay, wrtc::webrtc_session::rtppay_name, wrtc::webrtc_session::rtppay_payload);
        } else if (encode.rtppay == "rtph264pay" || encode.rtppay == "rtph265pay") {
            wrtc::webrtc_session::rtppay_payload = 103;
            wrtc::webrtc_session::rtppay_params = { {"pt", 103 }, {"config-interval", 1} };
            wrtc::webrtc_session::queue_params = { {"leaky", 0} };
            rtpdepay = fmt::format("{} name={}", decode_rtpdepay, "rtpdepay");
            rtppay = fmt::format("{} name={} pt={} config-interval=1", encode.rtppay, wrtc::webrtc_session::rtppay_name, wrtc::webrtc_session::rtppay_payload);
        } else {
            wrtc::webrtc_session::rtppay_payload = 96;
            rtpdepay = fmt::format("{} name={}", decode_rtpdepay, "rtpdepay");
            rtppay = fmt::format("{} name={} pt={}", encode.rtppay, wrtc::webrtc_session::rtppay_name, wrtc::webrtc_session::rtppay_payload);
        }
        std::string const rtspsrc{ fmt::format("rtsp://127.0.0.1:{}/{}", config.get_rtspsink_port(), config.get_rtspsink_mount() ) };
        std::string const watchdog{ config.webrtctout ? fmt::format("! watchdog timeout={} ", config.webrtctout) : "" };
        std::string stunserv{ !config.webrtcstun.empty() ? fmt::format("stun-server={} ", config.webrtcstun) : "" };
        wrtc::webrtc_session::pipeline_init = fmt::format(
            "rtspsrc location={} latency=0 name={} {}! application/x-rtp, payload={} ! {} ! {} ! webrtcbin bundle-policy={} {}name={} ",
            rtspsrc, wrtc::webrtc_session::source_name, watchdog, config.payload, rtpdepay, rtppay, wrtc::webrtc_session::bundle_policy, stunserv, wrtc::webrtc_session::webrtcbin_name
        );
        if (!running) {
            running = true;
            // log page
            wrtc::webrtc_session::server.Get("/log", [this](const httplib::Request &req, httplib::Response &res) -> void {
                LOG_INFO_FMT( "received /log request" );
                std::string content;
                int const length{ 65536 * 2 }; //8192
                std::fstream filelog;
                filelog.open(applog::log::cfg().file, std::ios::in);
                if (filelog.is_open()) {
                    if(length > 0)
                        filelog.seekg(-length, std::ios_base::end);
                    std::stringstream buffer;
                    buffer << filelog.rdbuf();
                    content = buffer.str();
                    filelog.close();
                } else {
                    LOG_ERROR_FMT( "unable to open file: {}", std::string(applog::log::cfg().file) );
                }
                std::string const result{ utils::str_last_lines(content, 1024 * 8) };
                res.set_content(result, "text/plain");
            });
            // help page
            wrtc::webrtc_session::server.Get("/help", [this](const httplib::Request &req, httplib::Response &res) -> void {
                LOG_INFO_FMT( "received /help request" );
                std::string content_help;
                if (on_help) {
                    nlohmann::json content_json = on_help(config);
                    std::ostringstream out;
                    out << "<html><head><meta charset='UTF-8'><title>Options</title></head><body>";
                    out << "<table border='1' cellpadding='6' cellspacing='0'><tr>"
                        << "<th>Key</th><th>Type</th><th>Value</th><th>Default</th><th>Description</th></tr>";
                    for (const auto& opt : content_json) {
                        std::string const key = safe_string(opt, "key");
                        std::string const typ = safe_string(opt, "type");
                        std::string const val = safe_string(opt, "value");
                        std::string const def = safe_string(opt, "default");
                        std::string const des = safe_string(opt, "description");
                        out << fmt::format(
                            "<tr><td><code>{}</code></td><td>{}</td><td>{}</td><td>{}</td><td>{}</td></tr>",
                            key, typ, val, def, des
                        );
                    }
                    out << "</table></body></html>";
                    content_help = out.str();
                } else {
                    content_help = "<h1>http server help</h1>";
                    content_help += "<p>use <code>/api?command=config</code> to set current configuration</p>";
                    content_help += "<p>use <code>/log</code> to get log output</p>";
                }
                res.set_content(content_help, "text/html");
            });
            // config page
            wrtc::webrtc_session::server.Get("/config", [this](const httplib::Request &req, httplib::Response &res) -> void {
                LOG_INFO_FMT( "received /config request" );
                std::string content_config;
                if (on_help) {
                    nlohmann::json content_json = on_help(config);
                    std::ostringstream out;
                    out << "<!DOCTYPE html><html>";
                    out << "<head><meta charset='UTF-8'><title>Config Form</title>";
                    out << "<style>";
                    out << "input.modified {";
                    out << "    background-color: #e6f7ff;";
                    out << "}";
                    out << "input.edited {";
                    out << "    background-color: #fff3cd;";
                    out << "}";
                    out << "input.error {";
                    out << "    background-color: #f8d7da;";
                    out << "}";
                    out << ".toast {";
                    out << "    position: fixed;";
                    out << "    bottom: 24px;";
                    out << "    right: 24px;";
                    out << "    background-color: #28a745;";
                    out << "    color: white;";
                    out << "    padding: 12px 24px;";
                    out << "    border-radius: 6px;";
                    out << "    font-family: sans-serif;";
                    out << "    font-size: 14px;";
                    out << "    display: none;";
                    out << "    z-index: 10000;";
                    out << "    box-shadow: 0 0 8px rgba(0,0,0,0.3);";
                    out << "    transition: opacity 0.3s ease-in-out;";
                    out << "}";
                    out << ".toast.show {";
                    out << "    display: block;";
                    out << "    opacity: 1;";
                    out << "}";
                    out << ".toast.error {";
                    out << "    background-color: #dc3545;";
                    out << "}";
                    out << "</style>";
                    out << "</head><body>";
                    out << "<h1>Configuration Parameters</h1>";
                    out << "<form id='configForm'>\n";
                    out << "<table border='1' cellpadding='6' cellspacing='0'><tr><th>Key</th><th>Type</th><th>Value</th><th>Default</th><th>Description</th></tr>\n";
                    for (const auto& opt : content_json) {
                        std::string const key = safe_string(opt, "key");
                        std::string const typ = safe_string(opt, "type");
                        std::string const val = safe_string(opt, "value");
                        std::string const def = safe_string(opt, "default");
                        std::string const des = safe_string(opt, "description");
                        bool const is_diff = (val != def);
                        std::string const css_class = is_diff ? " class='modified'" : "";
                        out << "<tr>";
                        out << "<td><code>" << key << "</code></td>";
                        out << "<td>" << typ << "</td>";
                        //out << "<td><input name='" << key << "' id='f_" << key << "' value='" << val << "' data-original='" << val << "' /></td>";
                        out << "<td><input name='" << key << "' id='f_" << key << "' value='" << val << "' data-original='" << val << "' data-default='" << def << "'" << css_class << " /></td>";
                        out << "<td>" << def << "</td>";
                        out << "<td>" << des << "</td>";
                        out << "</tr>\n";
                    }
                    out << "</table><br>\n";
                    out << "<button type='submit'>Apply</button>\n";
                    out << "<button type='button' id='saveButton'>Save (" << (conf_path.empty() ? c_config_filename : conf_path) << ")</button>\n";
                    out << "</form>\n";
                    out << "<div id='toast' class='toast'></div>";
                    // javascript to handle form submission
                    out << R"(
                    <script>
                    function showToast(message, isError = false) {
                        const toast = document.getElementById('toast');
                        toast.textContent = message;
                        toast.className = 'toast' + (isError ? ' error' : '');
                        toast.classList.add('show');
                        setTimeout(() => {
                            toast.classList.remove('show');
                        }, 3000);
                    }
                    document.addEventListener("DOMContentLoaded", function () {
                        const inputs = document.querySelectorAll("input[name]");
                        inputs.forEach(input => {
                            input.addEventListener("input", () => {
                                input.classList.toggle("edited", input.value !== input.dataset.original);
                            });
                        });
                    });
                    document.getElementById('configForm').addEventListener('submit', function(e) {
                        e.preventDefault();
                        const inputs = this.querySelectorAll('input');
                        const config = { command: "config" }; //const params = new URLSearchParams();params.append('command', 'config');
                        inputs.forEach(input => {
                            if (input.value !== input.dataset.original) {
                                config[input.name] = input.value; //params.append(input.name, input.value);
                            }
                        });
                        if (Object.keys(config).length === 1) { //if (params.toString() === 'command=config') {
                            showToast("Nothing changed"); //alert("Nothing changed");
                            return;
                        }
                        //fetch('/api?' + params.toString(), {
                        //    method: 'GET'
                        //}).then(resp => {
                        fetch('/api', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify(config)
                        }).then(resp => {
                            if (!resp.ok) throw new Error("Failed");
                            return resp.text();
                        }).then(text => {
                            showToast("Config updated"); //alert("Config updated");
                            setTimeout(() => location.reload(), 3000);
                        }).catch(err => {
                            showToast("Error: " + err, true); //alert("Error: " + err);
                        });
                    });
                    document.getElementById('saveButton').addEventListener('click', function() {
                        const save = { command: "save" };
                        fetch('/api', {
                            method: 'POST',
                            headers: { 'Content-Type': 'application/json' },
                            body: JSON.stringify(save)
                        }).then(resp => {
                            if (!resp.ok) throw new Error("Failed");
                            return resp.text();
                        }).then(text => {
                            showToast("Config saved");
                        }).catch(err => {
                            showToast("Error: " + err, true);
                        });
                    });
                    </script>
                    )";
                    out << "</body></html>";
                    content_config = out.str();
                } else {
                    content_config = "<h1>http server config</h1>";
                    //content_config += "<p>use <code>/api?command=config</code> to set current configuration</p>";
                    content_config += "<p>use <code>/api</code> POST with <code>{\"command\":\"config\", ...}</code> to update configuration</p>";
                }
                res.set_content(content_config, "text/html");
            });
            // api commands
            wrtc::webrtc_session::push_command(
                "config",
                [this](nlohmann::json& json, httplib::Response& res) -> void {
                    LOG_INFO_FMT( "received /api?command=config request" );
                    if (on_config)
                        changed = on_config(json, config);
                    res.set_content("config command handled", "text/plain");
                }
            );
            wrtc::webrtc_session::push_command(
                "save",
                [this](nlohmann::json& json, httplib::Response& res) -> void {
                    LOG_INFO_FMT( "received /api?command=save request" );
                    if (json.contains("path")) {
                        conf_path = json["path"].get<std::string>();
                    }
                    conf_path = json.contains("path") ? json["path"].get<std::string>() : (conf_path.empty() ? c_config_filename : conf_path);
                    if (on_save && on_save(config, conf_path)) {
                        res.set_content(fmt::format("configuration saved to {}", conf_path), "text/plain");
                        LOG_INFO_FMT( "configuration saved to {}", conf_path );
                    } else {
                        res.set_content(fmt::format("failed to save configuration to {}", conf_path), "text/plain");
                        LOG_ERROR_FMT( "failed to save configuration to {}", conf_path );
                    }
                }
            );
            wrtc::webrtc_session::push_command(
                "args",
                [this](nlohmann::json& json, httplib::Response& res) -> void {
                    LOG_INFO_FMT( "received /api?command=args request" );
                    nlohmann::json content = on_args ? on_args(config, true) : nlohmann::json();
                    res.set_content(content.dump(4), "application/json");
                }
            );
        }
        // start http server
        bool const res{ wrtc::webrtc_session::port && wrtc::webrtc_session::server_start() };
        if (res) {
            LOG_INFO_FMT( "HTTP server started and ready at http://<ip>:{}", wrtc::webrtc_session::port );
        } else {
            LOG_ERROR_FMT( "HTTP server failed to start at http://<ip>:{}", wrtc::webrtc_session::port );
        }
        return res;
    }
    #endif

    bool open() {
        if (server.is_opened()) {
            LOG_WARNING( "RTSP server is already opened" );
            stop();
        }
        auto const pipe{ pipeline() };
        std::vector<gst::rtspsink_t::pipedesc_t> pipes;
        LOG_INFO_FMT( "RTSP server pipeline: {}", pipe );
        pipes.push_back({ pipe, config.get_rtspsink_host(), config.get_rtspsink_port(), config.get_rtspsink_mount() });
        bool const opened{ server.open(pipes, config.rtspmcast, config.rtspmport) };
        if (opened) {
            LOG_INFO_FMT( "RTSP stream ready at rtsp://<ip>:{}/{}", config.get_rtspsink_port(), config.get_rtspsink_mount() );
        } else {
            LOG_ERROR_FMT( "RTSP stream failed" );
        }
        #if (defined(WITH_HTTPLIB))
        webrtc();
        #endif
        return opened;
    }

    void stop() {
        #ifdef WITH_HTTPLIB
        wrtc::webrtc_session::cleanup_all();
        wrtc::webrtc_session::server_stop();
        #endif
        if (server.is_opened()) {
            server.close();
            LOG_INFO( "RTSP server stopped" );
        } else {
            LOG_WARNING( "RTSP server is not started" );
        }
    }

    void wait() {
        while (!finished) { //&& server.is_opened()
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(10ms);
            if (changed) {
                changed = false;
                LOG_INFO( "RTSP server configuration changed" );
                stop();
                std::this_thread::sleep_for(500ms);
                bool const opened{ open() };
                if (opened) {
                    LOG_INFO( "RTSP server is restarted" );
                } else {
                    LOG_ERROR( "RTSP server failed to restart" );
                }
                std::this_thread::sleep_for(500ms);
            }
            //finished = !server.is_opened() && !wrtc::webrtc_session::is_running();
        }
        stop();
        LOG_INFO( "RTSP server is finished" );
    }

    config_t config;
    bool changed{ false };
    bool running{ false };
    std::string conf_path;
    gst::rtspsink_t server;
    gst::encode_params_t encode;
    
    inline static bool finished{ false };
    static void handler_sigint(int signum) {
        finished = true;
    }

    std::function<nlohmann::json(config_t&)> on_help{ nullptr };
    std::function<nlohmann::json(config_t&, bool)> on_args{ nullptr };
    std::function<bool(config_t&, std::string const&)> on_save{ nullptr };
    std::function<bool(nlohmann::json const&, config_t&)> on_config{ nullptr };
};

} // end of namespace app

namespace meta {

template<> inline auto register_members<utils::size_r>() {
    return make_members(
        make_member("width", 0, &utils::size_r::width),
        make_member("height", 1, &utils::size_r::height)
    );
}

template<> inline auto register_members<app::rtsp_t::config_t>() {
    return make_members(
        make_member("source", 0, &app::rtsp_t::config_t::source),
        make_member("property", 1, &app::rtsp_t::config_t::property),
        make_member("mediatype", 2, &app::rtsp_t::config_t::mediatype),
        make_member("framesize", 3, &app::rtsp_t::config_t::framesize),
        make_member("framerate", 4, &app::rtsp_t::config_t::framerate),
        make_member("format", 5, &app::rtsp_t::config_t::format),
        make_member("vidconvert", 6, &app::rtsp_t::config_t::vidconvert),
        make_member("queueleaky", 7, &app::rtsp_t::config_t::queueleaky),
        make_member("decode", 8, &app::rtsp_t::config_t::decode),
        make_member("encoder", 9, &app::rtsp_t::config_t::encoder),
        make_member("backend", 10, &app::rtsp_t::config_t::backend),
        make_member("encprop", 11, &app::rtsp_t::config_t::encprop),
        make_member("bitrate", 12, &app::rtsp_t::config_t::bitrate),
        make_member("tuning", 13, &app::rtsp_t::config_t::tuning),
        make_member("preset", 14, &app::rtsp_t::config_t::preset),
        make_member("keyframes", 15, &app::rtsp_t::config_t::keyframes),
        make_member("payload", 16, &app::rtsp_t::config_t::payload),
        make_member("interval", 17, &app::rtsp_t::config_t::interval),
        make_member("rtspsink", 18, &app::rtsp_t::config_t::rtspsink),
        make_member("rtspmcast", 19, &app::rtsp_t::config_t::rtspmcast),
        make_member("rtspmport", 20, &app::rtsp_t::config_t::rtspmport),
        make_member("verbose", 21, &app::rtsp_t::config_t::verbose)
        #if (defined(WITH_HTTPLIB))
        ,
        make_member("webrtctout", 22, &app::rtsp_t::config_t::webrtctout),
        make_member("webrtcport", 23, &app::rtsp_t::config_t::webrtcport),
        make_member("webrtcstun", 24, &app::rtsp_t::config_t::webrtcstun),
        make_member("webrtccont", 25, &app::rtsp_t::config_t::webrtccont)
        #endif
    );
}

template <typename T, typename = std::enable_if_t<meta::is_registered<T>()>>
inline void add_options(T& obj, cxxopts::Options& options, std::vector<std::string> const& descr) {
    meta::do_for_all_members<T>(
        [&obj, &options, &descr](auto& member) {
            using MemberT = meta::get_member_type<decltype(member)>;
            if ( member.can_get_const_ref() ) {
                auto& ref{ member.get_ref(obj) };
                auto const& id{ member.get_id() };
                auto const& name{ member.get_name() };
                auto add_option = meta::overload {
                    [&id, &name, &options, &descr](auto& val) {                        
                        opts::add_option(options, name, descr[id], val);
                    },
                };
                if (id >= 0 && id < descr.size()) {
                    add_option(ref);
                }
            }
        }
    );
}

template <typename T, typename = std::enable_if_t<meta::is_registered<T>()>>
void do_parse(T& obj, cxxopts::ParseResult& parsed, bool default_value = true) {
    meta::do_for_all_members<T>(
        [&obj, &parsed, &default_value](auto& member) {
            using MemberT = meta::get_member_type<decltype(member)>;
            if ( member.can_get_const_ref() ) {
                auto& ref{ member.get_ref(obj) };
                auto const& id{ member.get_id() };
                auto const& name{ member.get_name() };
                auto parse_option = meta::overload {
                    [&id, &name, &parsed](int& val) { val = parsed[name].template as<int>(); return true; },
                    [&id, &name, &parsed](bool& val) { val = parsed[name].template as<bool>(); return true; },
                    [&id, &name, &parsed](float& val) { val = parsed[name].template as<float>(); return true; },
                    [&id, &name, &parsed](double& val) { val = parsed[name].template as<double>(); return true; },
                    [&id, &name, &parsed](std::string& val) { val = parsed[name].template as<std::string>(); return true; },
                    [&id, &name, &parsed](unsigned& val) { val = parsed[name].template as<unsigned>(); return true; },
                    [&id, &name, &parsed](auto&) { return false; },
                };
                // check if the option is present in the parsed result
                // and if the id is valid (>= 0) or if we should use default
                try {
                    if (id >= 0 && (default_value || parsed.count(name)))
                        parse_option(ref);
                } catch (const std::exception& e) {
                    LOG_ERROR_FMT("error parsing option '{}': {}", name, e.what());
                }
            }
        }
    );
}

template <typename T, typename = std::enable_if_t<meta::is_registered<T>()>>
nlohmann::json make_help(T& obj, cxxopts::Options const& opts) {
    nlohmann::json out = nlohmann::json::array();
    meta::do_for_all_members<T>(
        [&obj, &opts, &out](auto& member) {
            using MemberT = meta::get_member_type<decltype(member)>;
            if (!member.can_get_const_ref()) return;
            auto& ref = member.get_ref(obj);
            const auto& name = member.get_name();
            const auto& detail = opts::get_option_detail(opts, name);
            if (!detail) return;
            std::string const key = !detail->l.empty() ? detail->l[0] : detail->s;
            std::string const def = detail->has_default ? detail->default_value : "";
            std::string const typ = detail->is_boolean ? "bool" : (detail->is_container ? "list" : "string");
            std::string const des = detail->desc;
            nlohmann::json j;
            j["key"] = key;
            j["type"] = typ;
            j["description"] = des;
            j["default"] = def;
            if constexpr (std::is_same_v<MemberT, bool>) {
                j["value"] = ref;
            } else if constexpr (std::is_same_v<MemberT, int> || std::is_same_v<MemberT, unsigned>) {
                j["value"] = ref;
            } else if constexpr (std::is_same_v<MemberT, float> || std::is_same_v<MemberT, double>) {
                j["value"] = static_cast<double>(ref);
            } else if constexpr (std::is_same_v<MemberT, std::string>) {
                j["value"] = ref;
            } else {
                j["value"] = "<unsupported>";
            }
            out.push_back(j);
        }
    );
    return out;
}

} // end of namespace meta

#endif // #ifndef __RTSP_HPP
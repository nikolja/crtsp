#pragma once

#ifndef __GST_HPP
#define __GST_HPP

#include <map>
#include <array>
#include <string>
#include <sstream>
#include <utility>
#include <algorithm>

// gst
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/rtsp/gstrtsp.h>
#include <gst/rtsp-server/rtsp-server.h>

// logging
#include <log.hpp>
// utils
#include <utils.hpp>

namespace gst {

// safe_ptr

template<typename T> static inline void safe_ptr_addref(T* ptr) {
    if (ptr)
        g_object_ref_sink(ptr);
}

template<typename T> static inline void safe_ptr_release(T** p_ptr);

template<> inline void safe_ptr_release<GError>(GError** p_ptr) { g_clear_error(p_ptr); }
template<> inline void safe_ptr_release<GstElement>(GstElement** p_ptr) { if (p_ptr) { gst_object_unref(G_OBJECT(*p_ptr)); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstElementFactory>(GstElementFactory** p_ptr) { if (p_ptr) { gst_object_unref(G_OBJECT(*p_ptr)); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstPad>(GstPad** p_ptr) { if (p_ptr) { gst_object_unref(G_OBJECT(*p_ptr)); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstCaps>(GstCaps** p_ptr) { if (p_ptr) { gst_caps_unref(*p_ptr); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstBuffer>(GstBuffer** p_ptr) { if (p_ptr) { gst_buffer_unref(*p_ptr); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstSample>(GstSample** p_ptr) { if (p_ptr) { gst_sample_unref(*p_ptr); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstBus>(GstBus** p_ptr) { if (p_ptr) { gst_object_unref(G_OBJECT(*p_ptr)); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GstMessage>(GstMessage** p_ptr) { if (p_ptr) { gst_message_unref(*p_ptr); *p_ptr = nullptr; } }
template<> inline void safe_ptr_release<GMainLoop>(GMainLoop** p_ptr) { if (p_ptr) { g_main_loop_unref(*p_ptr); *p_ptr = nullptr; } }

template<> inline void safe_ptr_addref<char>(char* p_ptr);  // declaration only. not defined. should not be used
template<> inline void safe_ptr_release<char>(char** p_ptr) { if (p_ptr) { g_free(*p_ptr); *p_ptr = nullptr; } }

template <typename T>
struct safe_ptr {
    inline safe_ptr() noexcept : ptr(nullptr) { }
    inline safe_ptr(T* p) : ptr(p) { }
    inline ~safe_ptr() noexcept { release(); }
    inline void release() noexcept {
        if (ptr)
            safe_ptr_release<T>(&ptr);
    }

    // no const in gst C API
    inline operator T* () noexcept { return ptr; }
    inline operator /*const*/ T* () const noexcept { return (T*)ptr; }

    T* get() { return ptr; }
    /*const*/ T* get() const { return (T*)ptr; }

    const T* operator -> () const { return ptr; }
    inline operator bool () const noexcept { return ptr != nullptr; }
    inline bool operator ! () const noexcept { return ptr == nullptr; }

    T** get_ref() { return &ptr; }

    inline safe_ptr& reset(T* p) noexcept {
        release();
        if (p) {
            safe_ptr_addref<T>(p);
            ptr = p;
        }
        return *this;
    }

    inline safe_ptr& attach(T* p) noexcept {
        release(); ptr = p; return *this;
    }
    inline T* detach() noexcept { T* p = ptr; ptr = nullptr; return p; }

    inline void swap(safe_ptr& o) noexcept { std::swap(ptr, o.ptr); }
private:
    safe_ptr(const safe_ptr&);
    safe_ptr& operator=(const T*);
protected:
    T* ptr;
};

inline bool element_exists(std::string const& elem_name) {
    safe_ptr<GstElementFactory> factory;
    factory.attach(gst_element_factory_find(elem_name.c_str()));
    return (bool)factory;
}

GstElement* element_by_name(GstElement *pipeline, std::string const& elem_name) {
    return pipeline ? gst_bin_get_by_name(GST_BIN(pipeline), elem_name.c_str()) : nullptr;
}

GstElement* element_contains(GstElement *pipeline, std::string const& elem_name) {
    if (!pipeline)
        return nullptr;
    bool done{ false };
    GValue value = G_VALUE_INIT;
    GstIterator *it = gst_bin_iterate_elements(GST_BIN(pipeline));
    while (!done) {
        gst::safe_ptr<gchar> name;
        GstElement *element{ nullptr };
        switch(gst_iterator_next(it, &value)) {
            case GST_ITERATOR_OK:
                element = GST_ELEMENT(g_value_get_object(&value));
                name.attach(gst_element_get_name(element));
                if (name) {
                    if (strstr(name, elem_name.c_str()) != nullptr) {
                        name.release();
                        g_value_unset(&value);
                        gst_iterator_free(it);
                        return element;
                    }
                    name.release();
                }
                g_value_unset(&value);
                break;
            case GST_ITERATOR_RESYNC:
                gst_iterator_resync(it);
                break;
            case GST_ITERATOR_ERROR:
            case GST_ITERATOR_DONE:
                done = true;
                break;
        }
    }
    gst_iterator_free(it);
    return nullptr;
}

// initializes gstreamer once in the whole process

struct initializer {

    static initializer& get() {
        static initializer instance;
        if (instance.is_failed)
            LOG_ERROR_FMT( "gstreamer: can't initialize" );
        //instance_ptr = &instance;
        return instance;
    }

    static initializer* ptr() {
        static initializer *instance_ptr{ nullptr };
        if (!instance_ptr)
            instance_ptr = &get();
        return instance_ptr;
    }
    
    bool is_looped() const { return start_loop; }
    bool is_deinit() const { return call_deinit; }
    bool is_loaded() const { return !is_failed; }

    void set_looped(bool const val) {
        if (start_loop == val)
            return;
        if (start_loop) {
            g_main_loop_quit(loop);
            thread.join();
        }
        start_loop = val;
        if (start_loop) {
            loop.attach(g_main_loop_new(nullptr, false));
            thread = std::thread( [this](){ g_main_loop_run(loop); } );
        }
    }

    auto& get_loop() { return loop; }
    auto& get_thread() { return thread; }

    static inline bool deinit_def{ false };
    static inline bool looped_def{ true };

    enum verbose_lvl {
        v_off = 0,
        v_err = 1,
        v_wrn = 2,
        v_inf = 3
    };

    static inline uint8_t verbose{ verbose_lvl::v_wrn };

    static bool is_verbose_inf() { return verbose > verbose_lvl::v_wrn; }
    static bool is_verbose_wrn() { return verbose > verbose_lvl::v_err; }
    static bool is_verbose_err() { return verbose > verbose_lvl::v_off; }

    static inline std::vector<std::string> arguments{ "--gst-debug-level=1" };
    static inline guint major, minor, micro, nano;

private:
    bool is_failed;
    bool start_loop;
    bool call_deinit;
    std::thread thread;
    safe_ptr<GMainLoop> loop;

    initializer(): is_failed(false) {
        call_deinit = deinit_def;
        start_loop = looped_def;

        //int argc = 0;
        //char** argv = new char*[2];
        //argv[argc++] = "--gst-version";
        //argv[argc++] = "--gst-debug-level=1";
        //gst_init(&argc, &argv);
        //delete argv;

        int argc{ 0 };
        char** argv{ nullptr };
        if (arguments.size() > 0) {
            argv = new char*[ arguments.size() + 1 ];
            argv[argc++] = (char*)""; // first argument will be ignored
            for (auto const& arg : arguments) {
                argv[argc++] = (char*)arg.c_str();
            }
        }

        safe_ptr<GError> err;
        gboolean res{ gst_init_check(&argc, &argv, err.get_ref()) };
        if (argv)
            delete argv;
        if (!res) {
            LOG_WARNING_FMT( "gstreamer: can't initialize {}", (err ? err->message : "<unknown reason>") );
            is_failed = true;
            return;
        }

        gst_version(&major, &minor, &micro, &nano);
        LOG_INFO_FMT( "gstreamer: version {}.{}.{} {}", major, minor, micro, nano ); // nano == 1 ? "(cvs)" : nano == 2 ? "(prerelease)" : ""
        if (GST_VERSION_MAJOR != major) {
            LOG_WARNING_FMT( "gstreamer: incompatible version" );
            is_failed = true;
            return;
        }

        if (start_loop) {
            loop.attach(g_main_loop_new(nullptr, false));
            thread = std::thread( [this](){ g_main_loop_run(loop); } );
        }
    }

    ~initializer() {
        if (call_deinit) {
            gst_deinit();
        }
        if (start_loop) {
            g_main_loop_quit(loop);
            if (thread.joinable())
                thread.join();
        }
    }

protected:
    initializer(const initializer & r) = delete;
    initializer & operator = (const initializer & r) = delete;
};

// codecs

enum codec_id {
    h264,
    h265,
    mpeg2,
    mpeg4,
    mjpeg,
    vp8,
    vp9,
    bmp
};

inline const std::map<codec_id, std::string> codec_keys {
    { h264, "h264" },
    { h265, "h265" },
    { mpeg2, "mpeg2" },
    { mpeg4, "mpeg4" },
    { mjpeg, "mjpeg" },
    { vp8, "vp8" },
    { vp9, "vp9" },
    { bmp, "bmp" },
};

// rtppay

inline const std::map<codec_id, std::string> map_rtppay_element_by_codec {
    { h264, "rtph264pay" },
    { h265, "rtph265pay" },
    { mpeg2, "rtpmp2tpay" },
    { mpeg4, "rtpmp4vpay" },
    { mjpeg, "rtpjpegpay" },
    { vp8, "rtpvp8pay" },
    { vp9, "rtpvp9pay" }
};

// rtpdepay

inline const std::map<codec_id, std::string> map_rtpdepay_element_by_codec {
    { h264, "rtph264depay" },
    { h265, "rtph265depay" },
    { mpeg2, "rtpmp2tdepay" },
    { mpeg4, "rtpmp4vdepay" },
    { mjpeg, "rtpjpegdepay" },
    { vp8, "rtpvp8depay" },
    { vp9, "rtpvp9depay" }
};

// parser

inline const std::map<codec_id, std::string> map_parser_element_by_codec {
    { h264, "h264parse" },
    { h265, "h265parse" },
    { mpeg2, "mpegvideoparse" },
    { mpeg4, "mpeg4videoparse" },
    { mjpeg, "jpegparse" }
    //{ vp8, "" }, { vp9, "" }
};

// x264enc
inline std::string par_x264enc_bitrate = "1000";         // (in kbit/sec)
inline std::string par_x264enc_tune = "zerolatency";
inline std::string par_x264enc_speed_preset = "ultrafast";
inline std::string par_x264enc_key_int_max = "10";
// x265enc
inline std::string par_x265enc_bitrate = "1000";         // (in kbit/sec)
inline std::string par_x265enc_tune = "zerolatency";
inline std::string par_x265enc_speed_preset = "ultrafast";
inline std::string par_x265enc_key_int_max = "10";
// qsvh264enc
inline std::string par_qsv_h264enc_bitrate = "1000";      // (in kbit/sec)
inline std::string par_qsv_h264enc_low_latency = "true";  // default: "false"
inline std::string par_qsv_h264enc_rate_control = "";     // default: "2", "vbr"
inline std::string par_qsv_h264enc_target_usage = "";     // default: "4", "balanced"
// qsvh265enc
inline std::string par_qsv_h265enc_bitrate = "1000";      // (in kbit/sec)
inline std::string par_qsv_h265enc_low_latency = "true";  // default: "false"
inline std::string par_qsv_h265enc_rate_control = "";     // default: "2", "vbr"
inline std::string par_qsv_h265enc_target_usage = "";     // default: "4", "balanced"
// qsvjpegenc
inline std::string par_qsv_jpegenc_low_latency = "true";  // default: "false"
inline std::string par_qsv_jpegenc_quality = "";          // default: "85"
inline std::string par_qsv_jpegenc_target_usage = "";     // default: "4", "balanced"
// vp8enc
inline std::string par_vp8enc_deadline = "1";
inline std::string par_vp8enc_target_bitrate = "";        // default: "256000" (in bits/sec)
inline std::string par_vp8enc_keyframe_max_dist = "";     // default: "128"
inline std::string par_vp8enc_threads = "";               // default: "0"
// vp9enc
inline std::string par_vp9enc_deadline = "1";
inline std::string par_vp9enc_target_bitrate = "";        // default: "256000" (in bits/sec)
inline std::string par_vp9enc_keyframe_max_dist = "";     // default: "128"
inline std::string par_vp9enc_threads = "";               // default: "0"
// v4l2h264enc
inline std::string par_v4l2_h264enc_video_bitrate = "1000000";
inline std::string par_v4l2_h264enc_video_level = "(string)4";
// omxh264enc
inline std::string par_omx_h264enc_bitrate = "1000000";
inline std::string par_omx_h264enc_control_rate = "1";
// omxh265enc
inline std::string par_omx_h265enc_bitrate = "1000000";
inline std::string par_omx_h265enc_control_rate = "1";
// openh264enc
inline std::string par_open_h264enc_bitrate = "1000000";
inline std::string par_open_h264enc_complexity = "low";
inline std::string par_open_h264enc_enable_frame_skip = "true";
inline std::string par_open_h264enc_max_bitrate = "";     // default: "0"
inline std::string par_open_h264enc_multi_thread = "4";
inline std::string par_open_h264enc_rate_control= "bitrate";
// jpegenc
inline std::string par_jpegenc_quality = "";              // default: "85"
// nvh264enc
inline std::string par_nv_h264enc_bitrate = "";           // default: "0" (from NVENC preset) (in kbit/sec)
inline std::string par_nv_h264enc_max_bitrate = "";       // default: "0" (in kbit/sec)
inline std::string par_nv_h264enc_preset = "";            // default: "0", "default" ("3", "low-latency")
inline std::string par_nv_h264enc_zerolatency = "true";   // default: "false"
// nvh265enc
inline std::string par_nv_h265enc_bitrate = "";           // default: "0" (from NVENC preset) (in kbit/sec)
inline std::string par_nv_h265enc_max_bitrate = "";       // default: "0" (in kbit/sec)
inline std::string par_nv_h265enc_preset = "";            // default: "0", "default" ("3", "low-latency")
inline std::string par_nv_h265enc_zerolatency = "true";   // default: "false"
// mfh264enc
inline std::string par_mf_h264enc_bitrate = "";           // default: "2048" (in kbit/sec)
inline std::string par_mf_h264enc_low_latency = "true";   // default: "false"
inline std::string par_mf_h264enc_max_bitrate = "";       // default: "0" (in kbit/sec)
// mfh265enc
inline std::string par_mf_h265enc_bitrate = "";           // default: "2048" (in kbit/sec)
inline std::string par_mf_h265enc_low_latency = "true";   // default: "false"
inline std::string par_mf_h265enc_max_bitrate = "";       // default: "0" (in kbit/sec)

// backend

enum backend_id {
    gst_auto,
    gst_basic,
    gst_v4l2,
    gst_libav,
    gst_nv,
    gst_qsv,
    gst_open,
    gst_d3d11,
    gst_mf,
    gst_omx
};

inline const std::map<backend_id, std::string> backend_keys {
    { gst_auto, "gst-auto" },
    { gst_basic, "gst-basic" },
    { gst_v4l2, "gst-v4l2" },
    { gst_libav, "gst-libav" },
    { gst_nv, "gst-nv" },
    { gst_qsv, "gst-qsv" },
    { gst_open, "gst-open" },
    { gst_d3d11, "gst-d3d11" },
    { gst_mf, "gst-mf" },
    { gst_omx, "gst-omx" }
};

// basic encode/decode

inline const std::map<codec_id, std::string> map_basic_encode_element_by_codec() {
    return {
        { h264, "x264enc" + 
            (
                std::string(!par_x264enc_tune.empty() ? " tune=" + par_x264enc_tune : "") +
                std::string(!par_x264enc_speed_preset.empty() ? " speed-preset=" + par_x264enc_speed_preset : "") +
                std::string(!par_x264enc_bitrate.empty() ? " bitrate=" + par_x264enc_bitrate : "") +
                std::string(!par_x264enc_key_int_max.empty() ? " key-int-max=" + par_x264enc_key_int_max : "")
            )
        },
        { h265, "x265enc" +
            (
                std::string(!par_x265enc_tune.empty() ? " tune=" + par_x265enc_tune : "") +
                std::string(!par_x265enc_speed_preset.empty() ? " speed-preset=" + par_x265enc_speed_preset : "") +
                std::string(!par_x265enc_bitrate.empty() ? " bitrate=" + par_x265enc_bitrate : "") +
                std::string(!par_x265enc_key_int_max.empty() ? " key-int-max=" + par_x265enc_key_int_max : "")
            )
        },
        { mpeg2, "mpeg2enc" },
        { mjpeg, "jpegenc" + 
            (
                std::string(!par_jpegenc_quality.empty() ? " quality=" + par_jpegenc_quality : "")
            )
        },
        { vp8, "vp8enc" + 
            (
                std::string(!par_vp8enc_deadline.empty() ? " deadline=" + par_vp8enc_deadline : "") + 
                std::string(!par_vp8enc_target_bitrate.empty() ? " target-bitrate=" + par_vp8enc_target_bitrate : "") +
                std::string(!par_vp8enc_keyframe_max_dist.empty() ? " keyframe-max-dist=" + par_vp8enc_keyframe_max_dist : "") +
                std::string(!par_vp8enc_threads.empty() ? " threads=" + par_vp8enc_threads : "")
            )
        },
        { vp9, "vp9enc" + 
            (
                std::string(!par_vp9enc_deadline.empty() ? " deadline=" + par_vp9enc_deadline : "") + 
                std::string(!par_vp9enc_target_bitrate.empty() ? " target-bitrate=" + par_vp9enc_target_bitrate : "") +
                std::string(!par_vp9enc_keyframe_max_dist.empty() ? " keyframe-max-dist=" + par_vp9enc_keyframe_max_dist : "") +
                std::string(!par_vp9enc_threads.empty() ? " threads=" + par_vp9enc_threads : "")
            )
        },
        { bmp, "avenc_bmp" }
    };
}

inline const std::map<codec_id, std::string> map_basic_decode_element_by_codec {
    { h264, "h264parse ! avdec_h264" },
    { h265, "h265parse ! avdec_h265" },
    { mpeg2, "mpegvideoparse ! mpeg2dec" },
    { mjpeg, "jpegparse ! jpegdec" },
    { vp8, "vp8dec" },
    { vp9, "vp9dec" },
    { bmp, "avdec_bmp" }
};

// v4l2 encode/decode

inline const std::map<codec_id, std::string> map_v4l2_encode_element_by_codec() {
    return {
        { h264, "v4l2h264enc extra-controls=\"controls,repeat_sequence_header=1,video_bitrate=" + par_v4l2_h264enc_video_bitrate + "\" ! video/x-h264,level=" + par_v4l2_h264enc_video_level },
        { mjpeg, "v4l2jpegenc" }
    };
}

inline const std::map<codec_id, std::string> map_v4l2_decode_element_by_codec {
    { h264, "h264parse ! v4l2h264dec" },
    { mjpeg, "jpegparse ! v4l2jpegdec" }
};

// open encode/decode

inline const std::map<codec_id, std::string> map_open_encode_element_by_codec() {
    return {
        { h264, "openh264enc" + 
            (
                std::string(!par_open_h264enc_bitrate.empty() ? " bitrate=" + par_open_h264enc_bitrate : "") +
                std::string(!par_open_h264enc_complexity.empty() ? " complexity=" + par_open_h264enc_complexity : "") +
                std::string(!par_open_h264enc_enable_frame_skip.empty() ? " enable-frame-skip=" + par_open_h264enc_enable_frame_skip : "") +
                std::string(!par_open_h264enc_max_bitrate.empty() ? " max-bitrate=" + par_open_h264enc_max_bitrate : "") +
                std::string(!par_open_h264enc_multi_thread.empty() ? " multi-thread=" + par_open_h264enc_multi_thread : "") +
                std::string(!par_open_h264enc_rate_control.empty() ? " rate-control=" + par_open_h264enc_rate_control : "")
            )
        },
        { mjpeg, "openjpegenc" }
    };
}

inline const std::map<codec_id, std::string> map_open_decode_element_by_codec {
    { h264, "h264parse ! openh264dec" },
    { mjpeg, "jpegparse ! openjpegdec" }
};

// qsv encode/decode

inline const std::map<codec_id, std::string> map_qsv_encode_element_by_codec() {
    return {
        { h264, "qsvh264enc" + 
            (
                std::string(!par_qsv_h264enc_bitrate.empty() ? " bitrate=" + par_qsv_h264enc_bitrate : "") +
                std::string(!par_qsv_h264enc_low_latency.empty() ? " low-latency=" + par_qsv_h264enc_low_latency : "") +
                std::string(!par_qsv_h264enc_rate_control.empty() ? " rate-control=" + par_qsv_h264enc_rate_control : "") +
                std::string(!par_qsv_h264enc_target_usage.empty() ? " target-usage=" + par_qsv_h264enc_target_usage : "")
            ) 
        },
        { h265, "qsvh265enc" + 
            (
                std::string(!par_qsv_h265enc_bitrate.empty() ? " bitrate=" + par_qsv_h265enc_bitrate : "") +
                std::string(!par_qsv_h265enc_low_latency.empty() ? " low-latency=" + par_qsv_h265enc_low_latency : "") +
                std::string(!par_qsv_h265enc_rate_control.empty() ? " rate-control=" + par_qsv_h265enc_rate_control : "") +
                std::string(!par_qsv_h265enc_target_usage.empty() ? " target-usage=" + par_qsv_h265enc_target_usage : "")
            )
        },
        { mjpeg, "qsvjpegenc" +
            (
                std::string(!par_qsv_jpegenc_low_latency.empty() ? " low-latency=" + par_qsv_jpegenc_low_latency : "") +
                std::string(!par_qsv_jpegenc_quality.empty() ? " quality=" + par_qsv_jpegenc_quality : "") +
                std::string(!par_qsv_jpegenc_target_usage.empty() ? " target-usage=" + par_qsv_jpegenc_target_usage : "")
            )
        }
    };
}

inline const std::map<codec_id, std::string> map_qsv_decode_element_by_codec {
    { h264, "h264parse ! qsvh264dec" },
    { h265, "h265parse ! qsvh265dec" },
    { mjpeg, "jpegparse ! qsvjpegdec" }
};

// nv encode/decode

inline const std::map<codec_id, std::string> map_nv_encode_element_by_codec() {
    return {
        { h264, "nvh264enc" +
            (
                std::string(!par_nv_h264enc_bitrate.empty() ? " bitrate=" + par_nv_h264enc_bitrate : "") +
                std::string(!par_nv_h264enc_max_bitrate.empty() ? " max-bitrate=" + par_nv_h264enc_max_bitrate : "") +
                std::string(!par_nv_h264enc_preset.empty() ? " preset=" + par_nv_h264enc_preset : "") +
                std::string(!par_nv_h264enc_zerolatency.empty() ? " zerolatency=" + par_nv_h264enc_zerolatency : "")
            )
        },
        { h265, "nvh265enc" +
            (
                std::string(!par_nv_h265enc_bitrate.empty() ? " bitrate=" + par_nv_h265enc_bitrate : "") +
                std::string(!par_nv_h265enc_max_bitrate.empty() ? " max-bitrate=" + par_nv_h265enc_max_bitrate : "") +
                std::string(!par_nv_h265enc_preset.empty() ? " preset=" + par_nv_h265enc_preset : "") +
                std::string(!par_nv_h265enc_zerolatency.empty() ? " zerolatency=" + par_nv_h265enc_zerolatency : "")
            )
        }
    };
};

inline const std::map<codec_id, std::string> map_nv_decode_element_by_codec {
    { h264, "h264parse ! nvh264dec" },
    { h265, "h265parse ! nvh265dec" },
    { mpeg2, "mpegvideoparse ! nvmpeg2videodec" },
    { mpeg4, "mpeg4videoparse ! nvmpeg4videodec" },
    { mjpeg, "jpegparse ! nvjpegdec" },
    { vp8, "nvvp8dec" },
    { vp9, "nvvp9dec" }
};

// libav encode/decode

inline const std::map<codec_id, std::string> map_libav_encode_element_by_codec {
    { mpeg2, "avenc_mpeg2video" },
    { mpeg4, "avenc_mpeg4" },
    { mjpeg, "avenc_mjpeg" },
    { bmp, "avenc_bmp" }
};

inline const std::map<codec_id, std::string> map_libav_decode_element_by_codec {
    { h264, "h264parse ! avdec_h264" },
    { h265, "h265parse ! avdec_h265" },
    { mpeg2, "mpegvideoparse ! avdec_mpeg2video" },
    { mpeg4, "mpeg4videoparse ! avdec_mpeg4" },
    { mjpeg, "jpegparse ! avdec_mjpeg" },
    { vp8, "parsebin ! avdec_vp8" },
    { bmp, "avdec_bmp" }
};

// d3d11 encode/decode

inline const std::map<codec_id, std::string> map_d3d11_encode_element_by_codec {};

inline const std::map<codec_id, std::string> map_d3d11_decode_element_by_codec {
    { h264, "h264parse ! d3d11h264dec" },
    { h265, "h265parse ! d3d11h265dec" },
    { mpeg2, "mpegvideoparse ! d3d11mpeg2dec" }
};

// mediafoundation encode/decode

inline const std::map<codec_id, std::string> map_mf_encode_element_by_codec() {
    return {
        { h264, "mfh264enc" + 
            (
                std::string(!par_mf_h264enc_bitrate.empty() ? " bitrate=" + par_mf_h264enc_bitrate : "") +
                std::string(!par_mf_h264enc_low_latency.empty() ? " low-latency=" + par_mf_h264enc_low_latency : "") +
                std::string(!par_mf_h264enc_max_bitrate.empty() ? " max-bitrate=" + par_mf_h264enc_max_bitrate : "")
            )
        },
        { h265, "mfh265enc" +
            (
                std::string(!par_mf_h265enc_bitrate.empty() ? " bitrate=" + par_mf_h265enc_bitrate : "") +
                std::string(!par_mf_h265enc_low_latency.empty() ? " low-latency=" + par_mf_h265enc_low_latency : "") +
                std::string(!par_mf_h265enc_max_bitrate.empty() ? " max-bitrate=" + par_mf_h265enc_max_bitrate : "")
            )
        }
    };
};

inline const std::map<codec_id, std::string> map_mf_decode_element_by_codec {};

// omx encode/decode

inline const std::map<codec_id, std::string> map_omx_encode_element_by_codec() {
    return {
        { h264, "omxh264enc control-rate=" + par_omx_h264enc_control_rate + " bitrate=" + par_omx_h264enc_bitrate + " ! video/x-h264, stream-format=byte-stream" },
        { h265, "omxh265enc control-rate=" + par_omx_h265enc_control_rate + " bitrate=" + par_omx_h265enc_bitrate + " ! video/x-h265, stream-format=byte-stream" }
    };
}

inline const std::map<codec_id, std::string> map_omx_decode_element_by_codec {
    { h264, "h264parse ! omxh264dec ! nvvidconv ! video/x-raw,format=BGRx" },
    { h265, "h265parse ! omxh265dec ! nvvidconv ! video/x-raw,format=BGRx" }
};

// backend map with encode/decode

inline const std::map<backend_id, std::map<codec_id, std::string>> map_encode_element_by_codec() {
    return {
        { gst_basic, map_basic_encode_element_by_codec() },
        { gst_v4l2, map_v4l2_encode_element_by_codec() },
        { gst_libav, map_libav_encode_element_by_codec },
        { gst_nv, map_nv_encode_element_by_codec() },
        { gst_qsv, map_qsv_encode_element_by_codec() },
        { gst_open, map_open_encode_element_by_codec() },
        { gst_d3d11, map_d3d11_encode_element_by_codec },
        { gst_mf, map_mf_encode_element_by_codec() },
        { gst_omx, map_omx_encode_element_by_codec() }
    };
}

inline const std::map<backend_id, std::map<codec_id, std::string>> map_decode_element_by_codec() {
    return {
        { gst_basic, map_basic_decode_element_by_codec },
        { gst_v4l2, map_v4l2_decode_element_by_codec },
        { gst_libav, map_libav_decode_element_by_codec },
        { gst_nv, map_nv_decode_element_by_codec },
        { gst_qsv, map_qsv_decode_element_by_codec },
        { gst_open, map_open_decode_element_by_codec },
        { gst_d3d11, map_d3d11_decode_element_by_codec },
        { gst_mf, map_mf_decode_element_by_codec },
        { gst_omx, map_omx_decode_element_by_codec }
    };
}

// formats

enum format_id {
    bgr, rgb, gray8, gray16_le, gray16_be,
    bgra, rgba, bgrx, rgbx, uyvy, yuy2,
    yvyu, nv12, nv21, yv12, i420
};

inline const std::map<format_id, std::string> format_keys {
    { bgr, "BGR" }, { rgb, "RGB" }, { gray8, "GRAY8" }, { gray16_le, "GRAY16_LE" }, { gray16_be, "GRAY16_BE" },
    { bgra, "BGRA" }, { rgba, "RGBA" }, { bgrx, "BGRX" }, { rgbx, "RGBX" }, { uyvy, "UYVY" }, { yuy2, "YUY2" },
    { yvyu, "YVYU" }, { nv12, "NV12" }, { nv21, "NV21" }, { yv12, "YV12" }, { i420, "I420" }
};

inline const std::map<backend_id, std::string> map_format_by_backend {
    { gst_basic, "NV12" },
    { gst_v4l2, "NV12" },
    { gst_libav, "NV12" },
    { gst_nv, "NV12" },
    { gst_qsv, "NV12" },
    { gst_open, "I420" },
    { gst_d3d11, "NV12" },
    { gst_mf, "NV12" },
    { gst_omx, "NV12" }
};

// codec_info

struct codec_info {
    std::string name;
    backend_id backend;
    codec_id codec;
    format_id format;
};

// h265 priority decoders/encoders

inline const std::vector<codec_info> prior_decoders_h265 {
    { "omxh265dec", backend_id::gst_omx, codec_id::h265, format_id::nv12 },
    { "v4l2h265dec", backend_id::gst_v4l2, codec_id::h265, format_id::nv12 },
    { "qsvh265dec", backend_id::gst_qsv, codec_id::h265, format_id::nv12 },
    { "avdec_h265", backend_id::gst_libav, codec_id::h265, format_id::nv12 },
    { "d3d11h265dec", backend_id::gst_d3d11, codec_id::h265, format_id::nv12 },
    { "nvh265dec", backend_id::gst_nv, codec_id::h265, format_id::nv12 }
};

inline const std::vector<codec_info> prior_encoders_h265 {
    { "omxh265enc", backend_id::gst_omx, codec_id::h265, format_id::nv12 },
    { "v4l2h265enc", backend_id::gst_v4l2, codec_id::h265, format_id::nv12 },
    { "qsvh265enc", backend_id::gst_qsv, codec_id::h265, format_id::nv12 },
    { "mfh265enc", backend_id::gst_mf, codec_id::h265, format_id::nv12 },
    { "nvh265enc", backend_id::gst_nv, codec_id::h265, format_id::nv12 },
    { "x265enc", backend_id::gst_basic, codec_id::h265, format_id::nv12 }
};

// h264 priority decoders/encoders

inline const std::vector<codec_info> prior_decoders_h264 {
    { "omxh264dec", backend_id::gst_omx, codec_id::h264, format_id::nv12 },
    { "v4l2h264dec", backend_id::gst_v4l2, codec_id::h264, format_id::nv12 },
    { "qsvh264dec", backend_id::gst_qsv, codec_id::h264, format_id::nv12 },
    { "avdec_h264", backend_id::gst_libav, codec_id::h264, format_id::nv12 },
    { "d3d11h264dec", backend_id::gst_d3d11, codec_id::h264, format_id::nv12 },
    { "nvh264dec", backend_id::gst_nv, codec_id::h264, format_id::nv12 },
    { "openh264dec", backend_id::gst_open, codec_id::h264, format_id::i420 }
};

inline const std::vector<codec_info> prior_encoders_h264 {
    { "omxh264enc", backend_id::gst_omx, codec_id::h264, format_id::nv12 },
    { "v4l2h264enc", backend_id::gst_v4l2, codec_id::h264, format_id::nv12 },
    { "qsvh264enc", backend_id::gst_qsv, codec_id::h264, format_id::nv12 },
    { "x264enc", backend_id::gst_basic, codec_id::h264, format_id::nv12 },
    { "mfh264enc", backend_id::gst_mf, codec_id::h264, format_id::nv12 },
    { "nvh264enc", backend_id::gst_nv, codec_id::h264, format_id::nv12 },
    { "openh264enc", backend_id::gst_open, codec_id::h264, format_id::i420 }
};

// mjpeg priority decoders/encoders

inline const std::vector<codec_info> prior_decoders_mjpeg {
    { "v4l2jpegdec", backend_id::gst_v4l2, codec_id::mjpeg, format_id::nv12 },
    { "qsvjpegdec", backend_id::gst_qsv, codec_id::mjpeg, format_id::nv12 },
    { "jpegdec", backend_id::gst_basic, codec_id::mjpeg, format_id::nv12 },
    { "nvjpegdec", backend_id::gst_nv, codec_id::mjpeg, format_id::nv12 }
};

inline const std::vector<codec_info> prior_encoders_mjpeg {
    { "v4l2jpegenc", backend_id::gst_v4l2, codec_id::mjpeg, format_id::nv12 },
    { "qsvjpegenc", backend_id::gst_qsv, codec_id::mjpeg, format_id::nv12 },
    { "jpegenc", backend_id::gst_basic, codec_id::mjpeg, format_id::nv12 }
};

// bmp priority decoders/encoders

inline const std::vector<codec_info> prior_decoders_bmp {
    { "avdec_bmp", backend_id::gst_libav, codec_id::bmp, format_id::rgb }
};

inline const std::vector<codec_info> prior_encoders_bmp {
    { "avenc_bmp", backend_id::gst_libav, codec_id::bmp, format_id::rgb }
};

// vp8 priority decoders/encoders

inline const std::vector<codec_info> prior_decoders_vp8 {
    { "vp8dec", backend_id::gst_basic, codec_id::vp8, format_id::rgb },
    { "avdec_vp8", backend_id::gst_libav, codec_id::vp8, format_id::rgb }
};

inline const std::vector<codec_info> prior_encoders_vp8 {
    { "vp8enc", backend_id::gst_basic, codec_id::vp8, format_id::rgb },
    { "avenc_vp8", backend_id::gst_libav, codec_id::vp8, format_id::rgb }
};

// todo: mpeg2, mpeg4
inline const std::map<codec_id, std::vector<codec_info>> map_prior_decoders_by_codec {
    { codec_id::h264, prior_decoders_h264 },
    { codec_id::h265, prior_decoders_h265 },
    { codec_id::mjpeg, prior_decoders_mjpeg },
    { codec_id::bmp, prior_decoders_bmp },
    { codec_id::vp8, prior_decoders_vp8 }
};

// todo: mpeg2, mpeg4
inline const std::map<codec_id, std::vector<codec_info>> map_prior_encoders_by_codec {
    { codec_id::h264, prior_encoders_h264 },
    { codec_id::h265, prior_encoders_h265 },
    { codec_id::mjpeg, prior_encoders_mjpeg },
    { codec_id::bmp, prior_encoders_bmp },
    { codec_id::vp8, prior_encoders_vp8 }
};

// extraction

inline std::pair<std::string, std::string> get_extract_parser(std::string const& decoder) {
    std::string par_string, dec_string;
    std::stringstream dec_stream(decoder);
    bool const first{ std::getline(dec_stream, par_string, '!') };
    par_string = first ? utils::trim(par_string) : "";
    bool const second{ std::getline(dec_stream, dec_string, '!') };
    dec_string = second ? utils::trim(dec_string) : "";
    return std::make_pair(par_string, dec_string);
}

inline std::string get_extract_decoder(std::string const& decoder) {
    std::size_t delim_pos = decoder.rfind('!');
    if (delim_pos != std::string::npos) {
        return std::string( utils::trim(decoder.substr(delim_pos + 1)) );
    }
    return decoder;
}

inline std::string get_extract_encoder(std::string const& encoder) {
    std::size_t delim_pos = encoder.find(' ');
    if (delim_pos != std::string::npos) {
        return std::string( utils::trim(encoder.substr(0, delim_pos)) );
    }
    return encoder;
}

// videoconvert

inline std::string get_videoconvert(backend_id backend, int nthreads = 0) {
    if (backend == backend_id::gst_v4l2)
        return "v4l2convert";
    return (nthreads > 0) ? fmt::format("videoconvert n-threads={}", nthreads) : "videoconvert";
}

inline std::string get_videoconvert(std::string const& backend, int nthreads = 0) {
    return get_videoconvert(utils::get_map_key(backend_keys, backend), nthreads);
}

// helpers elements

inline codec_info get_existed_element(codec_id codec, std::map<codec_id, std::vector<codec_info>> const& elements_map) {
    initializer::get();
    auto elements = utils::get_map_value(elements_map, codec, "invalid or unsupported codec");
    for(auto const& elm : elements)
        if (element_exists(elm.name))
            return elm;
    return {};
}

// available decoder

inline backend_id get_available_priority_decoder_backend(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_decoders_by_codec) };
    return c_info.backend;
}

inline format_id get_available_priority_decoder_format(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_decoders_by_codec) };
    return c_info.format;
}

inline std::string get_available_priority_decoder_format_key(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_decoders_by_codec) };
    return utils::get_map_value(format_keys, c_info.format, "invalid or unsupported format");
}

// available encoder

inline backend_id get_available_priority_encoder_backend(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_encoders_by_codec) };
    return c_info.backend;
}

inline format_id get_available_priority_encoder_format(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_encoders_by_codec) };
    return c_info.format;
}

inline std::string get_available_priority_encoder_format_key(codec_id codec) {
    codec_info const c_info{ get_existed_element(codec, map_prior_encoders_by_codec) };
    return utils::get_map_value(format_keys, c_info.format, "invalid or unsupported format");
}

// gstreamer decoder

inline std::string get_decoder(backend_id backend, codec_id codec) {
    auto const& decode_element_by_codec_ref{ map_decode_element_by_codec() };
    auto const& it = decode_element_by_codec_ref.find(backend == gst_auto ? get_available_priority_decoder_backend(codec) : backend);
    if (it != decode_element_by_codec_ref.end()) {
        auto const& mc{ it->second };
        auto const& mc_it = mc.find(codec);
        return (mc_it != mc.end()) ? mc_it->second : "";
    }
    return "";
}

inline std::string get_decoder(std::string const& backend, std::string const& codec) {
    auto bid = utils::get_map_key(backend_keys, backend, fmt::format("invalid gstreamer '{}' backend", backend));
    auto cid = utils::get_map_key(codec_keys, codec,  fmt::format("invalid or unsupported codec '{}'", codec));
    return get_decoder(bid, cid);
}

// gstreamer encoder

inline std::string get_encoder(backend_id backend, codec_id codec) {
    auto const& gst_encode_element_by_codec_ref{ map_encode_element_by_codec() };
    auto const& it = gst_encode_element_by_codec_ref.find(backend == gst_auto ? get_available_priority_encoder_backend(codec) : backend);
    if (it != gst_encode_element_by_codec_ref.end()) {
        auto const& mc{ it->second };
        auto const& mc_it = mc.find(codec);
        return (mc_it != mc.end()) ? mc_it->second : "";
    }
    return "";
}

inline std::string get_encoder(std::string const& backend, std::string const& codec) {
    auto bid = utils::get_map_key(backend_keys, backend, fmt::format("invalid gstreamer '{}' backend", backend));
    auto cid = utils::get_map_key(codec_keys, codec, fmt::format("invalid or unsupported codec '{}'", codec));
    return get_encoder(bid, cid);
}

// priority encoder

inline codec_info get_available_priority_encoder(codec_id codec) {
    return get_existed_element(codec, map_prior_encoders_by_codec);
}

// priority decoder

inline codec_info get_available_priority_decoder(codec_id codec) {
    return get_existed_element(codec, map_prior_decoders_by_codec);
}

// rtsp server

struct rtspsink_t {

    using pipedesc_t = std::array<std::string, 4>;

    ~rtspsink_t() { 
        stop(); 
    }

    static void on_client_disconnected(GstRTSPServer* server, GstRTSPClient* client) {
        LOG_INFO( "rtsp::server::on_client_disconnected: client disconnected" );
    }

    static void on_client_connected(GstRTSPServer* server, GstRTSPClient* client) {
        if (!client || !server)
            return;
        const GstRTSPConnection* conn{ gst_rtsp_client_get_connection(client)};
        if (!conn)
            return;
        g_signal_connect(client, "closed", G_CALLBACK(on_client_disconnected), nullptr);
        const gchar* ip{ gst_rtsp_connection_get_ip(conn) };
        LOG_INFO_FMT( "rtsp::server::on_client_connected: new client connected: {}", ip ? ip : "unknown" );
    }

    static void on_multicast(GstRTSPMediaFactory* factory, GstRTSPMedia* media, gpointer user_data) {
        guint streams_max{ 10 };
        guint streams{gst_rtsp_media_n_streams(media)};
        int multicast_port = GPOINTER_TO_INT(user_data);
        if (streams == 0) {
            LOG_WARNING( "rtsp::server::multicast: no streams in media" );
            return;
        }
        if (streams >= streams_max) {
            LOG_WARNING( "rtsp::server::multicast: streams count is too much ({})", streams );
            return;
        }
        for (int i = 0; i < streams; i++) {
            // address pool for multicast
            GstRTSPAddressPool* pool = gst_rtsp_address_pool_new();
            // extract the stream from the media
            GstRTSPStream* stream = gst_rtsp_media_get_stream(media, i);
            gchar *min, *max;
            // make a new address pool
            min = g_strdup_printf("224.3.0.%d", (2 * i) + 1);
            max = g_strdup_printf("224.3.0.%d", (2 * i) + 2);
            guint16 min_port = multicast_port + (streams_max * i);
            guint16 max_port = multicast_port + streams_max + (streams_max * i);
            gst_rtsp_address_pool_add_range(pool, min, max, min_port, max_port, 1);
            LOG_INFO_FMT( "rtsp::server::multicast: multicast address pool {} - {} ({}:{})", min, max, min_port, max_port );
            g_free(min);
            g_free(max);
            gst_rtsp_stream_set_address_pool(stream, pool);
            g_object_unref(pool);
        }
    }

    bool open(const std::vector<pipedesc_t>& pipedesc = {}, bool is_multicast = false, int multicast_port = 5600) {
        if (is_opened())
            return false;
        if (pipedesc.empty()) {
            LOG_WARNING( "rtsp::server::open: pipeline is empty" );
            return false;
        }
        // initialize gstreamer
        initializer::get().set_looped(true);
        // validate pipeline
        for (const auto desc : pipedesc) {
            if (desc.empty()) {
                LOG_WARNING( "rtsp::server::open: pipeline desc is empty" );
                return false;
            }
            // pipeline
            const std::string pipeline = desc.at(0);
            if (pipeline.empty()) {
                LOG_WARNING(" rtsp::server::open: pipeline is empty" );
                return false;
            }
            // host
            const std::string host = desc.at(1);
            if (host.empty()) {
                LOG_WARNING( "rtsp::server::open: host is empty" );
                return false;
            }
            // port
            const std::string port = desc.at(2);
            if (port.empty()) {
                LOG_WARNING( "rtsp::server::open: port is empty" );
                return false;
            }
            // mount
            const std::string mount = desc.at(3);
            if (mount.empty()) {
                LOG_WARNING( "rtsp::server::open: mount is empty" );
                return false;
            }

            GstElement* pipeline_element = gst_parse_launch(pipeline.c_str(), NULL);
            if (!pipeline_element) {
                LOG_WARNING( "rtsp::server::open: pipeline {} is incorrect", pipeline );
                return false;
            }
            gst_object_unref(pipeline_element);
        }
        // take host, port, mount from first pipedesc
        std::string host{pipedesc.at(0).at(1)};
        std::string port{pipedesc.at(0).at(2)};
        // server object
        server = gst_rtsp_server_new();
        gst_rtsp_server_set_address(server, host.c_str());  // "0.0.0.0" allows to connect from all ip
        gst_rtsp_server_set_service(server, port.c_str());  // rtsp port
        // log clients
        g_signal_connect(server, "client-connected", G_CALLBACK(on_client_connected), nullptr);
        // mounts object
        mounts = gst_rtsp_server_get_mount_points(server);
        // factory objects
        factories.reserve(pipedesc.size());
        for (const auto desc : pipedesc) {
            std::string pipeline{desc.at(0)};
            std::string mount{"/" + desc.at(3)};
            GstRTSPMediaFactory *factory = gst_rtsp_media_factory_new();
            gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());        
            gst_rtsp_media_factory_set_shared(factory, true);
            // multicast
            if (is_multicast)
                g_signal_connect(factory, "media-configure", (GCallback)on_multicast, GINT_TO_POINTER(multicast_port));
            gst_rtsp_mount_points_add_factory(mounts, mount.c_str(), factory);
            factories.push_back(factory);
            LOG_INFO_FMT( "rtsp server bind: rtsp://{}:{}{}", host, port, mount );
        }
        server_source = gst_rtsp_server_attach(server, nullptr);
        if (!server_source) {
            LOG_WARNING( "rtsp::server::open: failed to attach server" );
            return false;
        }
        start();
        opened = true;
        return opened;
    }

    void close() { 
        stop();
    }

    inline bool const is_opened() const { return opened && server_source != 0; }

protected:

    void start() {
        if (is_opened())
            return;
        if (!initializer::get().is_looped() && !loop) {
            loop.attach(g_main_loop_new(nullptr, false));
            thread = std::thread([this](){
                g_main_loop_run(loop);
            });
        }
    }

    void stop() {
        if (!is_opened())
            return;
        // remove source
        if (server_source != 0) {
            g_source_remove(server_source);
            server_source = 0;
        }
        // disconnect factories
        for (auto* factory : factories)
            g_object_unref(factory);
        factories.clear();
        // unmount points
        if (mounts) {
            g_object_unref(mounts);
            mounts = nullptr;
        }
        // free server
        g_object_unref(server);
        server = nullptr;
        // loop release
        if (loop) {
            g_main_loop_quit(loop);
            if (thread.joinable())
                thread.join();
            loop.release();
        }
        opened = false;
        LOG_INFO("rtsp server closed");
    }

private:
    bool opened{ false };
    std::thread thread;
    safe_ptr<GMainLoop> loop;
    guint server_source{ 0 };
    GstRTSPServer* server{ nullptr };
    GstRTSPMountPoints* mounts{ nullptr };
    std::vector<GstRTSPMediaFactory*> factories;
};

static constexpr auto& def_codec_key = "h264";
static constexpr auto  def_codec_id = codec_id::h264;
static constexpr auto& def_format_key = "NV12";
static constexpr auto  def_format_id = format_id::nv12;
static constexpr auto& def_backend_key = "gst-auto";
static constexpr auto  def_backend_id = backend_id::gst_auto;
static constexpr auto& def_parser_key = "h264parse";
static constexpr auto& def_rtppay_key = "rtph264pay";
static constexpr auto& def_rtpdepay_key = "rtph264depay";
static constexpr auto& def_media_type = "video/x-raw";
#if (defined(_WIN64) || defined(_WIN32) || defined(__CYGWIN32__) || defined(__MINGW32__))
static constexpr auto& def_videoconvert = "videoconvert";
static constexpr auto  def_backend_fav = backend_id::gst_qsv;
static constexpr auto& def_subpipe_enc = "qsvh264enc";
static constexpr auto& def_element_enc = "qsvh264enc";
static constexpr auto& def_subpipe_dec = "h264parse ! qsvh264dec";
static constexpr auto& def_element_dec = "qsvh264dec";
static constexpr auto& def_camera_elem = "mfvideosrc";
static constexpr auto& def_camera_prop = "device-index=0";
static constexpr auto& def_camera_size = "1280x720";
#elif (defined(IS_NANO))
static constexpr auto& def_videoconvert = "videoconvert";
static constexpr auto  def_backend_fav = backend_id::gst_omx;
static constexpr auto& def_subpipe_enc = "omxh264enc control-rate=1 bitrate=2500000 ! video/x-h264, stream-format=byte-stream";
static constexpr auto& def_element_enc = "omxh264enc";
static constexpr auto& def_subpipe_dec = "h264parse ! omxh264dec ! nvvidconv ! video/x-raw,format=BGRx";
static constexpr auto& def_element_dec = "omxh264dec";
static constexpr auto& def_camera_elem = "nvarguscamerasrc";
static constexpr auto& def_camera_prop = "sensor-id=0";
static constexpr auto& def_camera_size = "1280x720";
#elif (defined(IS_RPI4) || defined(IS_RPIZERO))
static constexpr auto& def_videoconvert = "v4l2convert";
static constexpr auto  def_backend_fav = backend_id::gst_v4l2;
static constexpr auto& def_subpipe_enc = "v4l2h264enc extra-controls=\"controls,repeat_sequence_header=1,video_bitrate=2500000\" ! video/x-h264,level=(string)4";
static constexpr auto& def_element_enc = "v4l2h264enc";
static constexpr auto& def_subpipe_dec = "h264parse ! v4l2h264dec";
static constexpr auto& def_element_dec = "v4l2h264dec";
static constexpr auto& def_camera_elem = "v4l2src"; //"libcamerasrc"
static constexpr auto& def_camera_prop = "device=/dev/video0"; //"camera-name=0"
static constexpr auto& def_camera_size = "640x512";
#else //#elif (defined(IS_RPI5) || defined(__aarch64__))
static constexpr auto& def_videoconvert = "videoconvert";
static constexpr auto  def_backend_fav = backend_id::gst_basic;
static constexpr auto& def_subpipe_enc = "x264enc tune=zerolatency speed-preset=ultrafast";
static constexpr auto& def_element_enc = "x264enc";
static constexpr auto& def_subpipe_dec = "h264parse ! avdec_h264";
static constexpr auto& def_element_dec = "avdec_h264";
static constexpr auto& def_camera_elem = "v4l2src";
static constexpr auto& def_camera_prop = "device=/dev/video0";
static constexpr auto& def_camera_size = "640x512";
#endif

// encoder

struct encode_params_t {
    codec_id codec{ def_codec_id };
    format_id formatid{ def_format_id };
    std::string format{ def_format_key };
    std::string parser{ def_parser_key };
    std::string rtppay{ def_rtppay_key };
    backend_id backend{ def_backend_id };
    std::string codeckey{ def_codec_key };
    std::string convert{ def_videoconvert };
    std::string subpipe{ def_subpipe_enc };
    std::string element{ def_element_enc };
    backend_id backend_fav{ def_backend_fav };

    void setup(std::string const& backend_key, std::string const& codec_key) {
        if (!backend_key.empty() && utils::is_map_exist(backend_keys, backend_key))
            backend = utils::get_map_key(backend_keys, backend_key, fmt::format("invalid '{}' backend", backend_key));
        if (!codec_key.empty() && utils::is_map_exist(codec_keys, codec_key))
            encode(utils::get_map_key(codec_keys, codec_key, fmt::format("invalid or unsupported codec '{}'", codec_key)));
    }

    void encode(codec_id val) {
        codec = val;
        codeckey = utils::get_map_value(codec_keys, codec, fmt::format("invalid or unsupported codec '{}'", (int)codec));
        format = backend == backend_id::gst_auto ? get_available_priority_encoder_format_key(codec) : utils::get_map_value(map_format_by_backend, backend, "invalid or unsupported format");
        parser = utils::get_map_value(map_parser_element_by_codec, codec, "invalid parser");
        rtppay = utils::get_map_value(map_rtppay_element_by_codec, codec, fmt::format("invalid or unsupported rtp payloader using '{}' codec", codeckey));
        formatid = backend == backend_id::gst_auto ? get_available_priority_encoder_format(codec) : utils::get_map_key(format_keys, format, "invalid or unsupported format");
        convert = get_videoconvert(backend == backend_id::gst_auto ? get_available_priority_encoder_backend(codec) : backend, 0);
        backend_fav = backend == backend_id::gst_auto ? get_available_priority_encoder_backend(codec) : backend;
        subpipe = get_encoder(backend, codec);
        element = backend == backend_id::gst_auto ? get_available_priority_encoder(codec).name : get_extract_encoder(subpipe);
        LOG_INFO_FMT(
            "encode(): {}, backed: {}, element: {}, format: {}, parser: {}, rtppay: {}, convert: {}, subpipe: {}",
            codeckey, utils::get_map_value(backend_keys, backend), element, format, parser, rtppay, convert, subpipe
        );
    }
};

// decoder

struct decode_params_t {
    codec_id codec{ def_codec_id };
    format_id formatid{ def_format_id };
    std::string format{ def_format_key };
    std::string parser{ def_parser_key };
    std::string rtpdepay{ def_rtpdepay_key };
    backend_id backend{ def_backend_id };
    std::string codeckey{ def_codec_key };
    std::string convert{ def_videoconvert };
    std::string subpipe{ def_subpipe_dec };
    std::string element{ def_element_dec };
    backend_id backend_fav{ def_backend_fav };

    void setup(std::string const& backend_key, std::string const& codec_key) {
        if (!backend_key.empty() && utils::is_map_exist(backend_keys, backend_key))
            backend = utils::get_map_key(backend_keys, backend_key, fmt::format("invalid '{}' backend", backend_key));
        if (!codec_key.empty() && utils::is_map_exist(codec_keys, codec_key))
            decode(utils::get_map_key(codec_keys, codec_key, fmt::format("invalid or unsupported codec '{}'", codec_key)));
    }

    void decode(codec_id val) {
        codec = val;
        codeckey = utils::get_map_value(codec_keys, codec, fmt::format("invalid or unsupported codec '{}'", (int)codec));
        format = backend == backend_id::gst_auto ? get_available_priority_decoder_format_key(codec) : utils::get_map_value(map_format_by_backend, backend, "invalid or unsupported format");
        parser = utils::get_map_value(map_parser_element_by_codec, codec, "invalid parser");
        rtpdepay = utils::get_map_value(map_rtpdepay_element_by_codec, codec, fmt::format("invalid or unsupported rtp depayloader using '{}' codec", codeckey));
        formatid = backend == backend_id::gst_auto ? get_available_priority_decoder_format(codec) : utils::get_map_key(format_keys, format, "invalid or unsupported format");
        convert = get_videoconvert(backend == backend_id::gst_auto ? get_available_priority_decoder_backend(codec) : backend, 0);
        backend_fav = backend == backend_id::gst_auto ? get_available_priority_decoder_backend(codec) : backend;
        subpipe = get_decoder(backend, codec);
        element = backend == backend_id::gst_auto ? get_available_priority_decoder(codec).name : get_extract_decoder(subpipe);
        LOG_INFO_FMT(
            "decode(): {}, backed: {}, element: {}, format: {}, parser: {}, rtpdepay: {}, convert: {}, subpipe: {}",
            codeckey, utils::get_map_value(backend_keys, backend), element, format, parser, rtpdepay, convert, subpipe
        );
    }
};

} // namespace gst

#endif // #ifndef __GST_HPP
#include <signal.h>
#include "rtsp.hpp"

int main(int argc, char* argv[]) {
	// capture ctrl-c
	signal(SIGINT, app::rtsp_t::handler_sigint);
    // logging
    app::logging();
    // rtsp server
    app::rtsp_t rtsp;
    // rtsp config
    app::rtsp_t::config_t& config{ rtsp.config };
    // opts::parser
    opts::parser options(argc, argv, "rtsp", "video to rtsp server");
    options.get_options().set_width(256);
    meta::add_options(config, options.get_options(), config.descriptions);
    options.add_default(app::c_config_filename);
    // parsing arguments
    if (!options.parse()) {
        LOG_ERROR_FMT( "error parsing options" );
        return 1;
    }
    auto& parsed = options.get_parsed();
    // check for help option
    if (parsed.count("help")) {
        LOG_INFO_FMT( options.help() );
        LOG_INFO_FMT( "usage:" );
        LOG_INFO_FMT( "{} [--source=v4l2src] [--property=device=/dev/video0] [--frametype=video/x-raw] [--framesize=640x512] [--framerate=30] [--bitrate=1000]", argv[0] );
        return 0;
    }
    // loading config
    meta::do_parse(config, parsed);
    rtsp.conf_path = parsed["config"].as<std::string>();
    // logging info
    app::print_info(argc, argv);
    app::print_conf(parsed["config"].as<std::string>());
    app::print_pars("arguments", options);
    // check for save options
    options.do_default();
    // check for errors
    if (!config.is_rtspsink_valid()) {
        LOG_ERROR_FMT( "rtsp streaming params error, rtspsink: {}", config.rtspsink );
        //return 1;
    }
    LOG_INFO_FMT(
        "source: {}, {}x{} @{}fps, encoder: {}:{} @{}kbps", 
        config.source, config.get_frame_width(), config.get_frame_height(), config.framerate, config.backend, config.encoder, config.bitrate
    );
    // setting http server
    // on_config callback
    rtsp.on_config = [&options](nlohmann::json const& json, app::rtsp_t::config_t& config) -> bool {
        for (auto it = json.begin(); it != json.end(); ++it) {
            std::string const& key = it.key();
            if (!options.is_key_valid(key)) // || options.is_key_default(key)
                continue;
            auto const& value = it.value();
            options.set(key, value.is_string() ? value.get<std::string>() : value.dump());
        }
        // re-parse options
        bool const res = options.parse();
        if (!res)
            LOG_ERROR_FMT( "error parsing options" );
        meta::do_parse(config, options.get_parsed()); //app::print_pars("arguments", options);
        return res;
    };
    // on_help callback
    rtsp.on_help = [&options](app::rtsp_t::config_t& config) -> nlohmann::json {
        return meta::make_help(config, options.get_options());
    };
    // on_save callback
    rtsp.on_save = [&options](app::rtsp_t::config_t& config, std::string const& path) -> bool {
        return options.to_file(path);
    };
    // on_args callback
    rtsp.on_args = [&options](app::rtsp_t::config_t& config, bool just_changed) -> nlohmann::json {
        return options.to_json(just_changed);
    };
    // making pipeline and server
    if (!rtsp.open())
        LOG_ERROR_FMT( "rtsp.open() failed" );
    // wait for server to finish
    rtsp.wait();

    return 0;
}
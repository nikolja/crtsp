# CRTSP: Lightweight RTSP/HTTP WebRTC Streaming Server

## Overview

**CRTSP** is a lightweight, customizable C++ RTSP server and signaling system designed for real-time video streaming via GStreamer. It supports dynamic HTTP configuration, JSON-based APIs, and WebRTC signaling logic, and can run on Linux, Windows, or Raspberry Pi devices.

## Features

* 📡 RTSP streaming with GStreamer pipelines
* 🌐 HTTP server for live configuration and diagnostics
* 📦 JSON and CLI config loading/saving
* 🔄 WebRTC session support (via `webrtc_session`)
* 🧩 Modular C++ headers with meta reflection system
* 🧠 Built-in `http://<ip>:<port>` (WebRTC), `http://<ip>:<port>/config` UIs config (HTML forms), `http://<ip>:<port>/api` (HTTP API)

---

## Clone Instructions

```bash
git clone https://github.com/nikolja/crtsp.git
cd crtsp
```

## Build Instructions

### ✅ Dependencies (all platforms)

* C++20 compatible compiler (GCC / Clang / MSVC)
* GStreamer 1.0
* CMake 3.16+

### 📦 Linux / Raspberry Pi

Install required libraries:

```bash
sudo apt update && sudo apt upgrade
sudo apt install -y git build-essential g++ cmake ninja-build
sudo apt install -y v4l-utils libv4l-dev libx264-dev libjpeg-dev libglib2.0-dev libcamera-dev
sudo apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-nice gstreamer1.0-libcamera libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libgstrtspserver-1.0-dev
```

Build:

```bash
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --parallel $(nproc)
```

Install and build:

```bash
chmod +x setup/rpi/prepare.sh
./setup/rpi/prepare.sh
chmod +x setup/rpi/build.sh
./setup/rpi/build.sh
```

Autorun on Raspberry Pi:

```bash
cd setup/rpi
chmod +x install.sh
sudo reboot
```

### 🪟 Windows (MSVC + vcpkg)

1. Install [vcpkg](https://github.com/microsoft/vcpkg)
2. Install dependencies:

```powershell
vcpkg install gstreamer
```

3. Configure CMake:

```powershell
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake"
cmake --build . --parallel 18
```

### 🪟 Windows (MSVC)

1. Install [GStreamer 1.0] (https://gstreamer.freedesktop.org/documentation/installing/on-windows.html?gi-language=c)
2. Configure your development environment (edit the system environment variables -> control panel -> system -> advanced -> environment variables)
#### ➤ Path: 
  - `C:\gstreamer\1.0\msvc_x86_64\bin`
  - `C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0`
  - `C:\gstreamer\1.0\msvc_x86_64\lib`
#### ➤ System variables
  - `GSTREAMER_1_0_ROOT_MSVC_X86_64 = C:\gstreamer\1.0\msvc_x86_64`
  - `GSTREAMER_1_0_ROOT_X86_64 = C:\gstreamer\1.0\msvc_x86_64`
  - `GSTREAMER_DIR = C:\gstreamer\1.0\msvc_x86_64`

3. Configure CMake:

```powershell
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 18
```

or 

1. Install and configure via Windows PowerShell (x86) with Admin permission:

```powershell
Set-ExecutionPolicy RemoteSigned
.\setup\win\gstreamer.ps1
Set-ExecutionPolicy Default
```

2. Reboot Windows and run PowerShell again

```powershell
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 18
```

---

## Usage

### 🔧 Run the RTSP server

Windows:
```bash
./rtsp --config=conf.json --framesize=640x480 --bitrate=1500
```

Example `conf.json`:

```json
{
  "backend": "gst-basic",
  "bitrate": 500,
  "framesize": "480p"
}
```

Command-line args override JSON.

RPi5 libcamera:
```bash
./rtsp --source=libcamerasrc --framesize=720p --framerate=30 --format=NV12 --property=
```

RPi5 v4l2src:
```bash
/rtsp --property=device=/dev/video99 --framesize=480x320 --framerate=25
```

## Parameters

All configuration parameters can be used consistently across:

* CLI arguments: `--key=value`
* JSON config file (`--config=conf.json`)
* HTTP API (`POST /api` or `GET /api?command=config&key=value`)

Below is the full list of supported keys and their meanings, extracted from `app/rtsp.hpp`:

### Common parameters:

| Key          | Type   | Description                                                              |
| ------------ | ------ | ------------------------------------------------------------------------ |
| `source`     | string | video source (e.g., `v4l2src`, `mfvideosrc`, `libcamerasrc`, ...)        |
| `property`   | string | video source properties (e.g., `device=/dev/video0`, `sensor-id=0`, ...) |
| `mediatype`  | string | media type (e.g., `video/x-raw`, `image/jpeg`)                           |
| `framesize`  | string | resolution (e.g., `480p`, `640x480`, `WxH`, `W;H`)                       |
| `framerate`  | int    | video framerate (e.g., 30)                                               |
| `format`     | string | pixel format (e.g., `UYVY`, `NV12`, `I420`, `GRAY8`, ...)                |
| `decode`     | string | decoder if needed (e.g., `jpegdec`, `avdec_mjpeg`, ...)                  |
| `encoder`    | string | output codec (`h264`, `vp8`, `mjpeg`, ...)                               |
| `backend`    | string | encoder backend (`gst-basic`, `gst-qsv`, `gst-nv`, ...)                  |
| `bitrate`    | int    | encoder bitrate in kbit/s                                                |
| `tuning`     | string | encoder tuning (`stillimage`, `zerolatency`, ...)                        |
| `preset`     | string | encoder preset (`ultrafast`, `veryfast`, ...)                            |
| `keyframes`  | int    | distance between keyframes (0 = auto)                                    |
| `payload`    | int    | payload type of output packets                                           |
| `interval`   | int    | SPS/PPS insertion interval (0 = disable, -1 = with every IDR)            |
| `rtspsink`   | string | RTSP stream address (e.g., `0.0.0.0:8554/stream0`)                       |
| `rtspmcast`  | bool   | enable RTSP multicast                                                    |
| `rtspmport`  | int    | multicast port                                                           |
| `verbose`    | bool   | verbose level                                                            |
| `webrtctout` | int    | WebRTC source timeout (ms)                                               |
| `webrtcport` | int    | HTTP/WebRTC port (e.g., 8000)                                            |
| `webrtcstun` | string | STUN server URL (e.g., `stun://stun.l.google.com:19302`)                 |
| `webrtccont` | string | HTML/JS content file (e.g., `client.html`)                               |

> **Note:** not all keys may be supported by every backend. See `./rtsp --help`, `http://<ip>:<port>/help` or source code for details.

---

## HTTP API

### `/` WebRTC Client Page

By default, opening `http://<host>:<port>/` loads the WebRTC HTML client.

* Designed for previewing real-time stream in browser
* Uses `webrtc.js` to handle signaling and media
* Customizable via `webrtccont` parameter (e.g., `client.html`)
* Supports overlays, FPS display, disconnection handling, and more

The server includes a built-in HTTP interface powered by `httplib`. It provides:

* Diagnostic pages (`/`, `/help`, `/log`, `/config`)
* JSON-based configuration API (`/api`) ((e.g., `/api?command=args`, `/api?command=config&framesize=720p`)
* Live HTML-based form UI for config editing (`/config`)

### `GET /help`

Returns an HTML page with a table of all available configuration options (with types, current/default values, and descriptions).

* Fields: key, type, current value, default value, description
* Internally populated via `on_help()` with reflection from `opts::parser`

### `GET /log`

Returns the most recent portion of the server log (`log.txt` or configured log file).

* Response: `text/plain`
* Internally limited to \~128KB, last 8K lines

### `GET /config`

Returns editable HTML form.

* 🔵 Highlights modified fields (value != default)
* 🟡 Live edited fields (via JS)
* Submits changes to `/api` via `POST`

### `GET /api`

This endpoint allows invoking API commands via URL query parameters.
Useful for quick testing or browser access.

#### Example:

```
GET /api?command=config&bitrate=1000&framesize=640x480
```

* Equivalent to a `POST` with the same fields
* Supports `command=config`, `command=save`, `command=args`

### `POST /api`

This endpoint accepts commands in JSON payload format. Supported commands:

#### `command: "config"`

Updates runtime configuration parameters.

```json
{
  "command": "config",
  "bitrate": 1500,
  "framesize": "720p"
}
```

* Only changed fields are required
* Responds with `config command handled`

#### `command: "save"`

Saves current configuration to file.

```json
{
  "command": "save",
  "path": "custom.json" // optional, otherwise uses current or default
}
```

* Response: status message with file path

#### `command: "args"`

Dumps all current CLI/JSON args as JSON.

```json
{
  "command": "args"
}
```

* Response: formatted JSON array of all active options

**Payload:**

```json
{ "command": "config", "bitrate": 1500, "framesize": "720p" }
```

Updates config in‑place; only sends changed keys.

---

## Setup Scripts

The `setup/` directory contains platform-specific scripts for building, installing, and configuring CRTSP.

### Structure:

#### `setup/rpi/`

Scripts tailored for Raspberry Pi deployment:

* `prepare.sh`: Installs system dependencies (GStreamer, build tools, etc.)
* `build.sh`: Builds the project using CMake with Raspberry Pi-optimized flags
* `install.sh`: Installs binaries and sets up autorun on boot

#### `setup/win/`

Windows-specific helper scripts:

* `gstreamer.ps1`: Downloads and sets up the GStreamer SDK, sets environment variables, adds paths

These scripts simplify the process of configuring the environment for both development and headless deployment.

---

## Source Files

* `app/rtsp.cpp` : Main entry point and HTTP server setup
* `app/rtsp.hpp` : Implementation of application
* `src/opts.hpp` : CLI and JSON config parser (based on `cxxopts`)
* `src/meta.hpp` : Meta reflection & serialization helpers
* `src/wrtc.hpp` : WebRTC signaling, HTTP server support
* `src/wrtc.inl` : HTML/JS WebRTC page
* `src/gst.hpp`  : GStreamer pipeline utilities
* `src/log.hpp`  : Logging wrapper via spdlog
* `src/json.hpp` : Json helpers via nlohmann
* `src/utils.hpp`: Misc. helpers

---

# Third-Party

* [cxxopts](https://github.com/jarro2783/cxxopts) &nbsp; **Lightweight C++ command line option parser** &nbsp; *MIT*
* [json](https://github.com/nlohmann/json) &nbsp; **JSON for Modern C++** &nbsp; *MIT*
* [spdlog](https://github.com/gabime/spdlog) &nbsp; **Fast C++ logging library** &nbsp; *MIT*
* [cpp-httplib](https://github.com/yhirose/cpp-httplib) &nbsp; **A C++ header-only HTTP/HTTPS server and client library** &nbsp; *MIT*

---

## License

MIT license with an exception for embedded forms

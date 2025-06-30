# CRTSP: Lightweight RTSP/HTTP WebRTC Streaming Server

## Overview

**CRTSP** is a lightweight, customizable C++ RTSP server and signaling system designed for real-time video streaming via GStreamer. It supports dynamic HTTP configuration, JSON-based APIs, and WebRTC signaling logic, and can run on Linux, Windows, or Raspberry Pi devices.

## Features

* 📡 RTSP streaming with GStreamer pipelines
* 🌐 HTTP server for live configuration and diagnostics
* 📦 JSON and CLI config loading/saving
* 🔄 WebRTC session support (via `webrtc_session`)
* 🧩 Modular C++ headers with meta reflection system
* 🧠 Built-in `http://<ip>:<port>` (WebRTC), `http://<ip>:<port>/config` UIs config (HTML forms), http://<ip>:<port>/api (HTTP API)

---

## Clone Instructions

```bash
git clone https://github.com/nikolja/crtsp.git
cd crtsp
```

## Build Instructions

### ✅ Dependencies (all platforms)

* C++17 compatible compiler (GCC / Clang / MSVC)
* GStreamer 1.0
* CMake 3.16+

### 📦 Linux / Raspberry Pi

Install required libraries:

```bash
sudo apt update && sudo apt upgrade
sudo apt install -y git build-essential cmake ninja-build g++
sudo apt install -y v4l-utils libv4l-dev libx264-dev libjpeg-dev libglib2.0-dev libcamera-dev
sudo apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-nice gstreamer1.0-libcamera libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libgstrtspserver-1.0-dev
```

Build:

```bash
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
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
## Path: 
  * C:\gstreamer\1.0\msvc_x86_64\bin
  * C:\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0
  * C:\gstreamer\1.0\msvc_x86_64\lib
## System variables
  * GSTREAMER_1_0_ROOT_MSVC_X86_64: C:\gstreamer\1.0\msvc_x86_64
  * GSTREAMER_1_0_ROOT_X86_64: C:\gstreamer\1.0\msvc_x86_64
  * GSTREAMER_DIR: C:\gstreamer\1.0\msvc_x86_64
## Path:
  * C:\gstreamer\1.0\msvc_x86_64\bin
3. Configure CMake:

```powershell
mkdir build && cd build
cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel 18
```

---

## Usage

### 🔧 Run the RTSP server

```bash
./crtsp --config=crtsp.json --framesize=640x480 --bitrate=1500
```

Example `crtsp.json`:

```json
{
  "backend": "gst-basic",
  "bitrate": 500,
  "framesize": "480p"
}
```

Command-line args override JSON.

---

## HTTP API

### `GET /help`

* Returns static HTML table of available options (with types, current/default values, and descriptions).

### `GET /config`

* Returns editable HTML form.
* Highlights:

  * 🔵 Modified fields (value != default)
  * 🟡 Live edited fields (via JS)
* Submits changes to `/api` via `POST`.

### `POST /api`

**Payload:**

```json
{
  "command": "config",
  "bitrate": 1500,
  "framesize": "720p"
}
```

* Updates config in-place. Only changed keys are sent.

---

## Source Files

* `app/rtsp.cpp`: Main entry point and HTTP server setup
* `src/opts.hpp`: CLI and JSON config parser (based on `cxxopts`)
* `src/meta.hpp`: Meta reflection & serialization helpers
* `src/wrtc.hpp`: WebRTC signaling support
* `src/gst.hpp`: GStreamer pipeline utilities

---

# Third-Party

* [cxxopts](https://github.com/jarro2783/cxxopts) &nbsp; **Lightweight C++ command line option parser** &nbsp; *MIT*
* [json](https://github.com/nlohmann/json) &nbsp; **JSON for Modern C++** &nbsp; *MIT*
* [spdlog](https://github.com/gabime/spdlog) &nbsp; **Fast C++ logging library** &nbsp; *MIT*
* [cpp-httplib](https://github.com/yhirose/cpp-httplib) &nbsp; **A C++ header-only HTTP/HTTPS server and client library** &nbsp; *MIT*

---

## License

MIT (or specify if proprietary/internal)

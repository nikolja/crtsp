#!/bin/bash

sudo apt update && sudo apt upgrade
sudo apt install -y git build-essential g++ cmake ninja-build
sudo apt install -y v4l-utils libv4l-dev libx264-dev libjpeg-dev libglib2.0-dev libcamera-dev
sudo apt install -y gstreamer1.0-plugins-ugly gstreamer1.0-nice gstreamer1.0-libcamera libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libgstrtspserver-1.0-dev

echo "[INFO] Preparing done"
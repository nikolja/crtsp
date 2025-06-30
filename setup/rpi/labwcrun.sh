#!/bin/bash

USER=$(whoami)
AUTORUN_DST="rtsp.sh"
AUTORUN_USR="/home/$USER"
AUTORUN_DIR="$AUTORUN_USR/autorun"
AUTORUN_SRC="autorun.sh"
AUTORUN_LABWC="$AUTORUN_USR/.config/labwc/autostart"
AUTORUN_PATH="${AUTORUN_DIR}/${AUTORUN_DST}"
AUTORUN_EXEC="../../build/rtsp"

mkdir -p $AUTORUN_DIR

# We make sure that the $AUTORUN_SRC script exists
if [ ! -f "$AUTORUN_SRC" ]; then
    echo "[ERROR] script $AUTORUN_SRC not found!"
    exit 1
fi

# Read the contents of $AUTORUN_SRC
AUTORUN_SCRIPT=$(<"$AUTORUN_SRC")

# Create the autorun script file
echo -e "$AUTORUN_SCRIPT" > "$AUTORUN_DIR/$AUTORUN_DST"

# Make the autorun script executable
chmod +x "$AUTORUN_DIR/$AUTORUN_DST"

# If the executable found, run the executable
if [ -f "$AUTORUN_EXEC" ]; then
    echo "[INFO] Executable found $AUTORUN_EXEC"
    cp "$AUTORUN_EXEC" "$AUTORUN_DIR"
    echo "[INFO] Copy $AUTORUN_EXEC to $AUTORUN_DIR"
    # set permissive access to $AUTORUN_DIR/rtsp
    chmod +x "$AUTORUN_DIR/rtsp"
else
    echo "[ERROR] Executable $AUTORUN_EXEC not found"
fi

echo "[INFO] Configuring autorun via Labwc..."

# Checking if labwc is working
if ! pgrep -x "labwc" > /dev/null; then
    echo "[INFO] Enabling Wayland + Labwc via raspi-config..."
    # W3 - use the labwc window manager with Wayland backend
    sudo raspi-config nonint do_wayland W3
    echo "[INFO] Reboot the system for the changes to take effect"
    #echo "[INFO] Run Labwc..."
    #nohup labwc > /dev/null 2>&1 &
else 
    echo "[INFO] Labwc is already running"
fi

# Add launch via labwc
mkdir -p "$(dirname "$AUTORUN_LABWC")"

if ! grep -qxF "$AUTORUN_PATH &" "$AUTORUN_LABWC"; then
    echo "$AUTORUN_PATH &" >> "$AUTORUN_LABWC"
    echo "[INFO] Added to $AUTORUN_LABWC"
else
    echo "[INFO] Already existed at $AUTORUN_LABWC"
fi

echo "[INFO] Service Labwc successfully configured and started!"

# End script
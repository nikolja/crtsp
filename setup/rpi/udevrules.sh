#!/bin/bash

# v4l2-ctl --list-devices
# v4l2-ctl -d 0 --list-formats-ext
# v4l2-ctl -d 99 --list-formats-ext
# v4l2-ctl -d 97 --list-formats-ext
# udevadm info --name=/dev/video0 --attribute-walk 

# Path to the udev rules file
RULES_FILE="/etc/udev/rules.d/99-usb-camera.rules"

# Obtain sudo privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root."
    exit 1
fi

# Check if the udev rules file $RULES_FILE exists
if [ -f "$RULES_FILE" ]; then
    echo "The udev rules file $RULES_FILE exists."
    echo "Removing existing udev rules file: $RULES_FILE"
    sudo rm "$RULES_FILE"
    #exit 1
fi

# Content to add to the rules file

# --device=/dev/video99 --width=480 --height=320 --framerate=25
RULE_CONTENT_0='SUBSYSTEM=="video4linux", ATTRS{idVendor}=="534d", ATTRS{idProduct}=="0021", ATTR{index}=="0", SYMLINK+="video99"'
# --device=/dev/video99 --width=640 --height=512 --framerate=30
RULE_CONTENT_1='SUBSYSTEM=="video4linux", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="5830", ATTR{index}=="0", SYMLINK+="video99"'
# --device=/dev/video97 --width=640 --height=512 --framerate=30
# gst-launch-1.0 v4l2src device=/dev/video97 ! image/jpeg, width=1280, height=720, framerate=30/1 ! jpegdec ! videoconvert ! autovideosink
RULE_CONTENT_2='SUBSYSTEM=="video4linux", ATTRS{idVendor}=="0bda", ATTRS{idProduct}=="5842", ATTR{index}=="0", SYMLINK+="video97"'
# --device=/dev/video99 --width=640 --height=512 --framerate=30  (ID 09cb:4007 FLIR Systems Breach) -> nemesis
RULE_CONTENT_3='SUBSYSTEM=="video4linux", ATTRS{idVendor}=="09cb", ATTRS{idProduct}=="4007", ATTR{index}=="0", SYMLINK+="video99"'
# gst-launch-1.0 v4l2src device=/dev/video97 ! image/jpeg, width=1280, height=720, framerate=30/1 ! jpegdec ! videoconvert ! autovideosink  (ID 0abd:8050 Xitech USB Camera) -> nemesis
RULE_CONTENT_4='SUBSYSTEM=="video4linux", ATTRS{idVendor}=="0abd", ATTRS{idProduct}=="8050", ATTR{index}=="0", SYMLINK+="video97"'

# Create or overwrite the file with the content
#sudo bash -c "echo $RULE_CONTENT > $RULES_FILE"

echo "Creating udev rules file: $RULES_FILE"
sudo echo "$RULE_CONTENT_0" > "$RULES_FILE"
sudo echo "$RULE_CONTENT_1" >> "$RULES_FILE"
sudo echo "$RULE_CONTENT_2" >> "$RULES_FILE"
sudo echo "$RULE_CONTENT_3" >> "$RULES_FILE"
sudo echo "$RULE_CONTENT_4" >> "$RULES_FILE"

# Set file permissions
sudo chmod u=rw,g=r,o=r $RULES_FILE

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Print success message
echo "File $RULES_FILE has been created, permissions set, and udev rules reloaded."

# Check if the symlink was created
if [ -e /dev/video99 ]; then
    echo "Symlink /dev/video99 has been created."
else
    echo "Warning: Symlink /dev/video99 is not created yet. Connect the device and check again."
fi

if [ -e /dev/video97 ]; then
    echo "Symlink /dev/video97 has been created."
else
    echo "Warning: Symlink /dev/video97 is not created yet. Connect the device and check again."
fi

# End script
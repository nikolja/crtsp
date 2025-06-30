#!/bin/bash

# File to edit
CONFIG_FILE="/boot/firmware/config.txt"

# Obtain sudo privileges
if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root."
    exit 1
fi

# Check if the file exists
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "File $CONFIG_FILE not found."
    exit 1
fi

# Remove lines
sed -i '/^dtoverlay=uart3/D' "$CONFIG_FILE"
sed -i '/^dtoverlay=uart0/D' "$CONFIG_FILE"
sed -i '/^enable_uart=1/D' "$CONFIG_FILE"
sed -i '/^usb_max_current_enable=0/D' "$CONFIG_FILE"
sed -i '/^usb_max_current_enable=1/D' "$CONFIG_FILE"

# Add necessary settings after [all]
sed -i "/^\[all\]/a dtoverlay=uart3" "$CONFIG_FILE"
sed -i "/^\[all\]/a dtoverlay=uart0" "$CONFIG_FILE"
sed -i "/^\[all\]/a enable_uart=1" "$CONFIG_FILE"
sed -i "/^\[all\]/a usb_max_current_enable=1" "$CONFIG_FILE"

# Comment out the lines dtparam=uart0=on and dtparam=uart3=on
sed -i "s/^dtparam=uart0=on/#dtparam=uart0=on/" "$CONFIG_FILE"
sed -i "s/^dtparam=uart3=on/#dtparam=uart3=on/" "$CONFIG_FILE"

# Display success message
echo "dtoverlay=uart3 added to $CONFIG_FILE"
echo "dtoverlay=uart0 added to $CONFIG_FILE"
echo "enable_uart=1 added to $CONFIG_FILE"
echo "usb_max_current_enable=1 added to $CONFIG_FILE"
echo "File $CONFIG_FILE successfully updated."

# End script
#!/bin/bash

# Save current working directory
ORIGINAL_DIR=$(pwd)

# Determine the directory where the script itself is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Go to the script directory
cd "$SCRIPT_DIR" || exit 1
echo "[INFO] script directory: $(pwd)"

# Update package list
sudo apt-get update

# Install dependencies
sudo apt-get install gstreamer1.0-plugins-ugly
sudo apt-get install libgstrtspserver-1.0-dev
sudo apt-get install gstreamer1.0-nice

# Upgrade packages
sudo apt-get upgrade
sudo apt autoremove

sudo systemctl enable ssh
sudo systemctl start ssh

# Create a backup of the file
CONFIG_FILE="/boot/firmware/config.txt"
sudo cp "$CONFIG_FILE" "$CONFIG_FILE.bak"
echo "Backup created: $CONFIG_FILE.bak"

chmod +x udevrules.sh
chmod +x uartenable.sh
chmod +x labwcrun.sh

sudo ./udevrules.sh
sudo ./uartenable.sh
./labwcrun.sh

#Returning back
cd "$ORIGINAL_DIR" || exit 1
echo "[INFO] Returned to original directory: $(pwd)"

#sudo reboot

# End script
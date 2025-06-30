#!/bin/bash

USER=$(whoami)
AUTORUN="/home/$USER/autorun"

cd "$AUTORUN/"
./rtsp --config=conf.json
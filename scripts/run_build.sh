#!/bin/bash

cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DPROJECT_INSTALL_DIR='/media/LINUX/GOG Games/Airline Tycoon Deluxe/game'
ninja -C build install
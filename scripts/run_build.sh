#!/bin/bash

cmake -B build -S . -G Ninja
ninja -C build install
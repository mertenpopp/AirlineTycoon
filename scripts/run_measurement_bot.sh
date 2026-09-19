#!/bin/bash

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -rf dataBOT_*.csv
rm -rf dataBOT_*.txt
ruby threadpool.rb --prefix=dataBOT "/setbotlevel 2"
python concat.py 'dataBOT_*.csv'
cd -
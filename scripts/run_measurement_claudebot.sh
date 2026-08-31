#!/bin/bash

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -rf dataCLAUDE_*.csv
rm -rf dataCLAUDE_*.txt
ruby threadpool.rb --prefix=dataCLAUDE "/setbotlevel 4"
python concat.py 'dataCLAUDE_*.csv'
cd -
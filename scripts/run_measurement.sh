#!/bin/bash

cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -rf dataCLAUDE_freegame_*.csv
rm -rf dataCLAUDE_freegame_*.txt
ruby threadpool.rb 
python concat.py 'dataCLAUDE_*.csv'
cd -
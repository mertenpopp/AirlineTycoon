#!/bin/bash

./scripts/run_build.sh
cd "/media/LINUX/GOG Games/Airline Tycoon Deluxe/game/"
rm -rf dataCOMPETITION_*.csv
rm -rf dataCOMPETITION_*.txt
ruby threadpool.rb --prefix=dataCOMPETITION "/setbotlevel 24"
python concat.py 'dataCOMPETITION_*.csv' HA,PT SaldoGesamt,Firmenwert
cd -
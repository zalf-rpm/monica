#!/bin/bash
mkdir .build
cd .build
qmake ../monica-jv.pro
make
# set vars
PROJECT="example"
IN_DIR="in"
DB_INI="db.ini"
WEATHER_DIR="met"
WEATHER_PREFIX="MET_HF."
OUT_DIR="out"
# run monica
cd dest
./monica -p $PROJECT -d $IN_DIR -i $DB_INI -w $WEATHER_DIR -m $WEATHER_PREFIX -o $OUT_DIR


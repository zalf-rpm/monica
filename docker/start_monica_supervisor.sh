#!/bin/sh
#verify mounted dirs
ls -al /monica_data
if [ -d "/monica_data/climate-data" ]; then 
    ls -al "/monica_data/climate-data"
fi 
/usr/bin/supervisord -n -c /etc/supervisor/supervisord.conf
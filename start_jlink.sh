#!/bin/sh

# 判断进程是否存在，如果不存在就启动它
PIDS=`ps | grep /opt/jlink/bin/jlink | grep -v grep | awk '{print $1}'`
if [ "$PIDS" != "" ]; then
    echo "$PIDS"
else
    echo "start"
    setsid /opt/jlink/bin/jlink > /dev/null 2>&1 &
fi

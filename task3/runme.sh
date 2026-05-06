#!/bin/bash

make clean
make

CONFIG="/tmp/init.conf"
LOG="/tmp/myinit.log"
RESULT="result.txt"

rm -f $LOG

cat << EOF > $CONFIG
/usr/bin/sleep 100 /dev/null /tmp/out1
/usr/bin/sleep 101 /dev/null /tmp/out2
/usr/bin/sleep 102 /dev/null /tmp/out3
EOF

echo "Wait Result: start 3 proccess, restart for dead, SIGHUP" >> $RESULT
echo "------------------------------------" >> $RESULT

./myinit -c $CONFIG
sleep 2

if ! pgrep myinit > /dev/null; then
    echo "Critical error: daemon did not start" >> $RESULT
    cat $RESULT
    exit 1
fi

echo "--- 1: Start (3 process sleep) ---" >> $RESULT
ps -ef | grep "[s]leep 10" >> $RESULT

echo "---  2: Kill process 101 (restart) ---" >> $RESULT
pkill -f "sleep 101"
sleep 2
ps -ef | grep "[s]leep 10" >> $RESULT

echo "--- 3: SIGHUP---" >> $RESULT
cat << EOF > $CONFIG
/usr/bin/sleep 200 /dev/null /tmp/single_out
EOF
kill -HUP $(pgrep myinit)
sleep 2
ps -ef | grep "[s]leep 200" >> $RESULT

echo "--- 4: Log info ---" >> $RESULT
if [ -f $LOG ]; then
    cat $LOG >> $RESULT
else
    echo "Log file not found!" >> $RESULT
fi

pkill myinit
echo "Complete."
cat $RESULT

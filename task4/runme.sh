#!/bin/bash

OUT_FILE="result.txt"
SRV_LOG="/tmp/server.log"

until ! pkill -9 -x server 2>/dev/null; do sleep 0.1; done
until ! pkill -9 -x client 2>/dev/null; do sleep 0.1; done
echo "Kill processes"

make clean > /dev/null 2>&1
make > /dev/null
echo "/tmp/brownian.sock" > config
echo "0" > check.txt

echo "Generation random number"
rm -f nums.txt
yes $'5\n-5' | head -n 1000 | shuf > nums.txt

./server &
SRV_PID=$!
sleep 1

echo "Run Test1"
printf "Test1: Correct Calc\n" >> "$OUT_FILE"

rm -f delays.txt
CHILD_PIDS=""
for idx in $(seq 1 100); do
    ./client config nums.txt 0.1 >> delays.txt 2>/dev/null &
    CHILD_PIDS="$CHILD_PIDS $!"
done
wait $CHILD_PIDS
echo "Test1 complete"

CHECK_VAL=$(./client config check.txt 0 2>&1 >/dev/null | sed -n 's/.*LAST_RESP:\(.*\)/\1/p')
if [ "$CHECK_VAL" = "0" ]; then
    echo "Success: Internal state server 0" >> "$OUT_FILE"
else
    echo "ERROR: State server = '$CHECK_VAL'" >> "$OUT_FILE"
fi

echo "" >> "$OUT_FILE"
echo "Test2: Memory leak" >> "$OUT_FILE"
INITIAL_CONN_LOG=$(grep "ACCEPTED" "$SRV_LOG" | head -n 1)
FINAL_CONN_LOG=$(grep "ACCEPTED" "$SRV_LOG" | tail -n 1)
echo "First connect: $INITIAL_CONN_LOG" >> "$OUT_FILE"
echo "Last connect: $FINAL_CONN_LOG" >> "$OUT_FILE"

echo "" >> "$OUT_FILE"
echo "Test3: Asynch profiling" >> "OUT_FILE"
echo "Clients | Delay(s) | Time server | Max. delay | Effective time" >> "$OUT_FILE"
echo "---------------------------------------------------------------------------" >> "$OUT_FILE"

echo "Run Test3"
for num_clients in 10 50 100; do
    for sleep_time in 0 0.2 0.6 1.0; do
        rm -f delays.txt
        MARK_START=$(date +%s.%N)
        
        CHILD_PIDS=""
        for ((c=1; c<=num_clients; c++)); do
            ./client config nums.txt "$sleep_time" >> delays.txt 2>/dev/null &
            CHILD_PIDS="$CHILD_PIDS $!"
        done
        wait $CHILD_PIDS
        
        MARK_END=$(date +%s.%N)
        
        PEAK_DELAY=$(awk 'BEGIN{val=0} {if($1>val) val=$1} END{print val}' delays.txt)
        TOTAL_SRV_TIME=$(awk -v t_start="$MARK_START" -v t_end="$MARK_END" 'BEGIN { printf "%.4f", t_end - t_start }')
        REAL_EFFICIENCY=$(awk -v srv_t="$TOTAL_SRV_TIME" -v p_del="$PEAK_DELAY" 'BEGIN { printf "%.4f", srv_t - p_del }')
        
        printf "%-8s | %-11s | %-13s | %-14.4f | %-18s\n" "$num_clients" "$sleep_time" "$TOTAL_SRV_TIME" "$PEAK_DELAY" "$REAL_EFFICIENCY" >> "$OUT_FILE"
    done
done
echo "Test3 complete"

kill -9 $SRV_PID 2>/dev/null
cat "$OUT_FILE"

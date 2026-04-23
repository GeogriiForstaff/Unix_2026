#!/bin/bash

make clean
make


rm -f stats.txt result.txt
touch stats.txt

echo "Start 10 parallel processes to test locker" result.txt
echo "---RESULT ---" >> result.txt

for i in {1..10}
do
   ./locker -f sharedfile -s stats.txt &
   pids[${i}]=$!
done

echo "Test running, waiting 300s.."
sleep 300

# Stop
kill -SIGINT ${pids[*]}
wait

cat stats.txt >> result.txt
echo "Test complete" >> result.txt

cat result.txt

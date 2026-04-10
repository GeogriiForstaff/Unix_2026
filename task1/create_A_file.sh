#!/bin/bash
FILE="fileA"

SIZE=4194305

truncate -s $SIZE $FILE

printf "\x01" | dd of=$FILE bs=1 seek=0 conv=notrunc 2>/dev/null

printf "\x01" | dd of=$FILE bs=1 seek=10000 conv=notrunc 2>/dev/null

printf "\x01" | dd of=$FILE bs=1 seek=$((SIZE-1)) conv=notrunc 2>/dev/null

echo "file $FILE create. Size $SIZE byte"

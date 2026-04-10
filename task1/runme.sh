make clean
make



./create_A_file.sh


echo "Run Test"
./myprogram fileA fileB
gzip -k -f fileA
gzip -k -f fileB
gzip -cd fileB.gz | ./myprogram fileC
./myprogram -b 100 fileA fileD


echo "==========LOGS:============" > result.txt
echo "FILE | SIZE | BLOCKS" >> result.txt
for f in fileA fileA.gz fileB fileB.gz fileC fileD; do
    if [ -f "$f" ]; then
        LOGIC=$(stat -c %s "$f")
        DISK=$(stat -c %b "$f")
        echo "$f | $LOGIC | $DISK" >> result.txt
    fi
done

echo "--------------------------------------"
echo "Result in result.txt"
cat result.txt

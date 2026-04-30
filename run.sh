#!/bin/bash

ports=(8081 8082 8083 8084 8085 8086 8087 8088 8089 8090 8091 8092 8093 8094 8095 8096)
output="output.txt"

# CHANGE THESE
N=4000
T=8

echo "n=$N t=$T" | tee -a $output

for run in 1 2 3; do
    echo "  Run $run..." 

    for i in $(seq 0 $((T-1))); do
	    ./take2 $N ${ports[$i]} 1 > /dev/null 2>&1 &
	done

	sleep 1

	t_elapsed=$(./take2 $N 8080 0 2>&1 | grep "Execution time" | awk '{print $3}')
    echo "  $t_elapsed" | tee -a $output

    wait
done

echo "" | tee -a $output
echo "Done!"

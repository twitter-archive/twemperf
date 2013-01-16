#!/bin/sh

mcperf=~/workspace/twemperf/src/mcperf

server=localhost
port=11211

num_conns=10
conn_rate=10000

num_calls=1000
call_rate=0

lb_size=100
lb_size=1000

prefix="abc:"

niter=100

for i in `seq 1 ${niter}`; do
    for method in "set" "get" "gets" "delete" "add" "incr" "decr" "replace" "append" "prepend"; do
        printf "[%d] command: %s =>\n" $i $method
        ${mcperf} --server=${server} --port=${port} --num-conns=${num_conns} --conn-rate=${conn_rate} --num-calls=${num_calls} --call-rate=${call_rate} --method=${method} --sizes=u${lb_size},${lb_size} --prefix=${prefix} >> /tmp/mcperf.out 2>&1
    done
    grep --color "error\|Errors:" /tmp/mcperf.out | grep -v "0"
    mv /tmp/mcperf.out /tmp/mcperf.out.${i}
    sleep 2
done

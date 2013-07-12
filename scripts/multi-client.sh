#!/bin/bash

MCPERF=./mcperf

CLIENTS=16

LOG=mcperf.log

HOST=localhost
PORT=11211

NUM_CONNS=10
CONN_RATE=10000

NUM_CALLS=10000
CALL_RATE=0

for i in `seq $CLIENTS`
do
    printf "Cleaning up existing log file at %s\n" $LOG.$i
    rm -f $LOG.$i
    printf "starting client %s\n" $i
    $MCPERF --server=$HOST --port=$PORT --client=$i/$CLIENTS --num-conns=$NUM_CONNS --conn-rate=$CONN_RATE --num-calls=$NUM_CALLS --call-rate=$CONN_RATE >> $LOG.$i 2>&1 &
done


printf "Waiting for all clients to finish...."
wait
printf "done.\n"

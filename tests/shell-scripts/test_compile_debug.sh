#!/bin/bash
echo "Starting compilation..."
./wyn lib/compiler_complete.wyn 2>&1 &
PID=$!
echo "PID: $PID"

for i in {1..10}; do
    if ps -p $PID > /dev/null; then
        echo "Still running after $i seconds..."
        sleep 1
    else
        echo "Process ended after $i seconds"
        break
    fi
done

if ps -p $PID > /dev/null; then
    echo "Killing process..."
    kill -9 $PID
fi
wait $PID 2>/dev/null
echo "Exit code: $?"

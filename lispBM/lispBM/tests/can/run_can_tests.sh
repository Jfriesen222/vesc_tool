#!/bin/bash
# Run the CANopen interoperability test: a LispBM CANopen node in the REPL
# against a python-canopen master, both on vcan0.
#
# Requirements:
#   - REPL built with SocketCAN:  (cd ../../repl && make FEATURES="socketcan")
#   - vcan0 up:   sudo modprobe vcan
#                 sudo ip link add dev vcan0 type vcan
#                 sudo ip link set up vcan0
#   - venv:       ./setup_venv.sh

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPL="$SCRIPT_DIR/../../repl/repl"
PYTHON="$SCRIPT_DIR/venv/bin/python"

if ! ip link show vcan0 > /dev/null 2>&1; then
    echo "vcan0 does not exist (see header of this script)"; exit 1
fi
if [ ! -x "$PYTHON" ]; then
    echo "venv missing, run ./setup_venv.sh"; exit 1
fi
if [ ! -x "$REPL" ] || ! grep -q "can-open" "$REPL"; then
    echo "REPL not built with SocketCAN, run: make FEATURES=\"64 socketcan\" in repl/"; exit 1
fi

NODE_LOG="$(mktemp)"
"$REPL" --src="$SCRIPT_DIR/canopen_node.lisp" \
        --src="$SCRIPT_DIR/run_canopen_node.lisp" \
        --terminate > "$NODE_LOG" 2>&1 &
NODE_PID=$!
sleep 0.3

"$PYTHON" -I "$SCRIPT_DIR/canopen_master.py"
RES=$?

kill $NODE_PID 2> /dev/null
wait $NODE_PID 2> /dev/null

if grep -qi "error" "$NODE_LOG"; then
    echo "--- errors in node log:"
    grep -i -A3 "error" "$NODE_LOG"
    RES=1
fi
rm -f "$NODE_LOG"
exit $RES

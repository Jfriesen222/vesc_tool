#!/bin/bash
# Create a Python virtual environment with python-canopen (and python-can)
# for testing LispBM CAN/CANopen code over SocketCAN.
#
# Usage: ./setup_venv.sh          create tests/can/venv (if missing)
#        ./setup_venv.sh --force  recreate it from scratch
#
# Afterwards run Python test scripts with: ./venv/bin/python <script>

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/venv"

if [ "$1" == "--force" ]; then
    rm -rf "$VENV_DIR"
fi

if [ ! -d "$VENV_DIR" ]; then
    echo "Creating venv in $VENV_DIR"
    python3 -m venv "$VENV_DIR"
fi

"$VENV_DIR/bin/pip" install --quiet --upgrade pip
"$VENV_DIR/bin/pip" install --quiet canopen

"$VENV_DIR/bin/python" -c \
  "import canopen, can; print('canopen', canopen.__version__, '/ python-can', can.__version__)"

if ! ip link show vcan0 > /dev/null 2>&1; then
    echo ""
    echo "Note: vcan0 does not exist. Create it with:"
    echo "  sudo modprobe vcan"
    echo "  sudo ip link add dev vcan0 type vcan"
    echo "  sudo ip link set up vcan0"
fi

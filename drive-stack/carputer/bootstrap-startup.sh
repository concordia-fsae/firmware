#!/usr/bin/env bash
set -euo pipefail

echo "[bootstrap-startup] $(date -Is) configuring vcan interfaces"
modprobe vcan || true

if ! ip link show vcanVeh >/dev/null 2>&1; then
	ip link add vcanVeh type vcan
fi
if ! ip link show vcanBody >/dev/null 2>&1; then
	ip link add vcanBody type vcan
fi

ip link set vcanVeh up
ip link set vcanBody up

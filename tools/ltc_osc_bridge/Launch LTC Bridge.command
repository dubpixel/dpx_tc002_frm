#!/bin/bash
# LTC → OSC Bridge — double-click launcher (macOS)
# Make executable once: chmod +x "Launch LTC Bridge.command"
cd "$(dirname "$0")"
python3 -m pip install -r requirements.txt --quiet
python3 ltc_osc_bridge_gui.py

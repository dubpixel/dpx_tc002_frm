@echo off
cd /d "%~dp0"
python -m pip install -r requirements.txt --quiet
python ltc_osc_bridge_gui.py
pause

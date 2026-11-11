@echo off
cd /d "%~dp0"
node "node-red\abrir-pad.cjs"
if errorlevel 1 pause

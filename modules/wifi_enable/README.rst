:Author: pmwoodward3
:License: GPLv2
:Summary: WiFi Enable + TCP Server for 70D

Initializes the 70D WiFi stack (Lugh + PTP framework) and starts a TCP
echo server for remote camera control. Companion: camremote.py

Uses RAM-loaded socket APIs (validated by hw_test v15) and eventproc
call() names mapped from ROM1 RE (263 entries across 40 tables).

Default port: 5555. Override via ML/SETTINGS/WIFIEN.CFG (port number
as decimal text).

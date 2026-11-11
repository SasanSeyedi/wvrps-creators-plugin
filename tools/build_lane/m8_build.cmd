@echo off
rem M8: run build.cmd <ver> on CPUs 8-15 at BelowNormal (same courtesy as the ODK route)
cd /d C:\Users\Sasan\wvrps_creators_build
start "wvrps_b%1" /B /WAIT /BELOWNORMAL /AFFINITY FF00 cmd /c build.cmd %1
echo M8_BUILD_%1_EXIT=%ERRORLEVEL%

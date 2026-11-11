@echo off
rem Build WVRPS Creators against Epic's UE 5.5 (or 5.4 with arg 54). All output under C:\Users\Sasan\wvrps_creators_build.
set VER=%1
if "%VER%"=="" set VER=55
if "%VER%"=="55" set ENGINE=C:\Program Files\Epic Games\UE_5.5
if "%VER%"=="54" set ENGINE=C:\Program Files\Epic Games\UE_5.4
set OUT=C:\Users\Sasan\wvrps_creators_build\pkg%VER%
if exist "%OUT%" rmdir /s /q "%OUT%"
call "%ENGINE%\Engine\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="F:\Estate\Claude\ODK\plugins\WVRPSCreators\WVRPSCreators.uplugin" -Package="%OUT%" -TargetPlatforms=Win64 > C:\Users\Sasan\wvrps_creators_build\build%VER%.log 2>&1
echo EXIT %ERRORLEVEL% >> C:\Users\Sasan\wvrps_creators_build\build%VER%.log
exit /b %ERRORLEVEL%

@echo off
rem UE 5.4 workaround (no code change): compile BuildPlugin's own HostProject with UBT directly, adding -DisablePlugin=LightForge.
rem Cause, bisected over -DisablePlugin= across 614 engine plugins on this machine: the third-party plugin
rem Engine\Plugins\LightForge (Boundless Entertainment, "Installed": true, outside Engine\Plugins\Marketplace).
rem UBT finds its LightForge.Build.cs file but gets no rules Type for it, so ModuleRules.IsValidForTarget(null) throws
rem ArgumentNullException in UEBuildTarget.AddAllValidModulesToTarget (UEBuildTarget.cs:3938). BuildPlugin has no way
rem to pass -DisablePlugin, hence this direct call. Run "build.cmd 54" first (it fails, but leaves pkg54\HostProject).
set ENGINE=C:\Program Files\Epic Games\UE_5.4
set H=C:\Users\Sasan\wvrps_creators_build\pkg54\HostProject
"%ENGINE%\Engine\Binaries\ThirdParty\DotNet\6.0.302\windows\dotnet.exe" "%ENGINE%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" UnrealEditor Win64 Development -Project="%H%\HostProject.uproject" -plugin="%H%\Plugins\WVRPSCreators\WVRPSCreators.uplugin" -noubtmakefiles -nohotreload -DisablePlugin=LightForge -log="C:\Users\Sasan\wvrps_creators_build\ubt54_disable_lightforge.txt" > C:\Users\Sasan\wvrps_creators_build\build54_workaround.log 2>&1
echo EXIT %ERRORLEVEL% >> C:\Users\Sasan\wvrps_creators_build\build54_workaround.log

@echo off
rem Copy configuration files to target folder

setlocal
setlocal enableextensions

set src=%~dp0\src
set tgt=%1

if "%tgt%"=="" (
    echo Must specify target folder as a parameter
    goto end
)

if "%tgt%"=="debug" (
    set tgt=ms\built\debug
)

if "%tgt%"=="release" (
    set tgt=ms\built\release
)

set cfg=%tgt%\config

rmdir /s /q %cfg%
mkdir %cfg%

echo Copying to %cfg%

echo d | xcopy /s %src%coordsys %cfg%\coordsys
echo d | xcopy /s %src%perl %cfg%\perl
echo d | xcopy /s %src%snap\config\format %cfg%\format
echo d | xcopy /s %src%snap\config\snap %cfg%\snap
echo d | xcopy /s %src%snapspec\config\snapspec %cfg%\snapspec
echo d | xcopy /s %src%snaplist\config\snaplist %cfg%\snaplist
echo d | xcopy /s %src%snap_manager\config %cfg%
echo d | xcopy /s %src%packages %cfg%\package
copy %src%/VERSION %tgt%
copy %src%/VERSIONID %tgt%

:end

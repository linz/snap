@echo off
rem Copy configuration files to target folder

setlocal
setlocal enableextensions

set src=%~dp0
set tgt=%1

if "%tgt%"=="" (
    echo Must specify target folder as a parameter
    goto end
)

if "%tgt%"=="debug" (
    set tgt=..\ms\built\debug
)

if "%tgt%"=="release" (
    set tgt=..\ms\built\release
)

set tgt=%tgt%\config

rmdir /s /q %tgt%
mkdir %tgt%

echo Copying to %tgt%

echo d | xcopy /s %src%coordsys %tgt%\coordsys
echo d | xcopy /s %src%perl %tgt%\perl
echo d | xcopy /s %src%snap\config\format %tgt%\format
echo d | xcopy /s %src%snap\config\snap %tgt%\snap
echo d | xcopy /s %src%snapspec\config\snapspec %tgt%\snapspec
echo d | xcopy /s %src%snaplist\config\snaplist %tgt%\snaplist
echo d | xcopy /s %src%snap_manager\config %tgt%
echo d | xcopy /s %src%packages %tgt%\package

:end

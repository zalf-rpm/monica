@echo off
set MONICA_BUILD_NUMBER=
for /f "delims=" %%i in (%1\build-number.txt) do set MONICA_BUILD_NUMBER=%%i
set /a MONICA_BUILD_NUMBER=%MONICA_BUILD_NUMBER%+1
echo !define BuildNumber %MONICA_BUILD_NUMBER% > %1\build-number.nsh
echo %MONICA_BUILD_NUMBER% > %1\build-number.txt

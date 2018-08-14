set version=%1
set build=%2
set arch=%3
"%ProgramFiles(x86)%\NSIS\makensis" /DX%arch%  /DVersionNumber=%version% /DBuildNumber=%build% installer-script.nsi
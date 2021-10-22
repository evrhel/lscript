@echo off

echo Build library classes...
..\x64\Release\lsasm.exe -o ..\lscript\lib -r -i src

pause
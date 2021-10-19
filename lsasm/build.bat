@echo off

echo Build library classes...
..\x64\Release\lsasm.exe -o ..\lscript\lib ^
	Main.lasm ^
	String.lasm ^
	StringBuilder.lasm ^
	System.lasm ^
	Process.lasm ^
	StdFileHandle.lasm ^
	FileOutputStream.lasm ^
	FileInputStream.lasm ^
	Math.lasm ^
	Object.lasm ^
	Int.lasm ^
	Long.lasm ^
	Class.lasm

pause
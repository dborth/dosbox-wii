@echo off
rem Builds your game into a .wuhb: double-click this file.
rem Edit these first. They are what the Wii U Menu shows.
set NAME=My DOS Game
set SHORT_NAME=My DOS Game
set AUTHOR=Your Name
set OUTPUT=MyDOSGame.wuhb

cd /d "%~dp0"

rem The tool: in tools\, then on the PATH, then in a devkitPro install
set TOOL=
if exist "tools\wuhbtool.exe" set TOOL=tools\wuhbtool.exe
if "%TOOL%"=="" (where wuhbtool >nul 2>nul && set TOOL=wuhbtool)
if "%TOOL%"=="" if exist "C:\devkitPro\tools\bin\wuhbtool.exe" set TOOL=C:\devkitPro\tools\bin\wuhbtool.exe
if "%TOOL%"=="" (
	echo wuhbtool.exe not found. Put it in the tools folder ^(see tools\README.txt^).
	pause
	exit /b 1
)

if not exist dosbox-wiiu.rpx (
	echo dosbox-wiiu.rpx not found: it belongs in this folder, next to build.bat.
	pause
	exit /b 1
)

if not exist content\autoboot.conf (
	echo content\autoboot.conf not found: it is the conf your game starts with.
	pause
	exit /b 1
)

"%TOOL%" dosbox-wiiu.rpx "%OUTPUT%" --content=content --name="%NAME%" --short-name="%SHORT_NAME%" --author="%AUTHOR%" --icon=meta\icon.png --tv-image=meta\tv_splash.png --drc-image=meta\drc_splash.png
if errorlevel 1 (
	pause
	exit /b 1
)

echo.
echo Built %OUTPUT%. Copy it to wiiu\apps\ on your SD card.
pause

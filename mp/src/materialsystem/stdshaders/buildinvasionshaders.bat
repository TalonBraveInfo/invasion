@echo off
setlocal

set GAMEDIR=%cd%\..\..\..\game\invasion
set SDKBINDIR="%ProgramFiles%\Steam\SteamApps\common\Source SDK Base 2013 Multiplayer\bin"
set SOURCEDIR=..\..

call buildsdkshaders.bat

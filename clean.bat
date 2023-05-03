@echo off
rem ===========================================================================
rem Clean folder utility
rem                                               Copyright (c) 2006-2016 MAYO.
rem ===========================================================================

@echo on
pause clean each folder?

cd /d %~dp0

del *.aps  /s /f /q
del *.suo /a:h /s /f /q
del *.ncb /s /f /q
del *.user  /s /f /q
rmdir /s /q Release
rmdir /s /q Debug
rmdir /s /q Win32
rmdir /s /q Win32
rmdir /s /q x64
rmdir /s /q x64

pause

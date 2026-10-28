@echo off
rem Runs every program in demo\safety. Usage: demo\run_safety_demo.bat [path-to-jscpp.exe]
cd /d "%~dp0.."
set J=%1
if "%J%"=="" set J=build\jscpp.exe
for %%f in (demo\safety\*.cppsafe) do (
    echo === %%f
    type %%f
    echo --- jscpp run %%f
    %J% run %%f
    echo ^(exit status %ERRORLEVEL%^)
    echo.
)

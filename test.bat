@echo off
setlocal enabledelayedexpansion
set cflags=
if "%1" == "-g" (
    shift
    set cflags= -D WAH_DEBUG
)
for %%i in (wah_test_%1*.c) do (
    echo ## Running %%i...
    gcc -W -Wall%cflags% %%i -o %%~ni && %%~ni
    if !errorlevel! neq 0 (
        echo.
        echo ## %%i failed.
        goto end
    )
    echo.
)
echo ## All tests passed.
:end

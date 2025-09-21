@echo off
setlocal enabledelayedexpansion
for %%i in (wah_test_%1*.c) do (
    echo ## Running %%i...
    gcc -W -Wall %%i -o %%~ni && %%~ni
    if !errorlevel! neq 0 (
        echo.
        echo ## %%i failed.
        goto end
    )
    echo.
)
echo ## All tests passed.
:end

if defined KOKORO_BUILD_ID (
    choco install llvm --version 6.0.1 -y
    if errorlevel 1 exit /b %errorlevel%

    choco install windows-sdk-10.0 -y
    echo "DEBUG: echoing log file"
    type "T:\tmp\chocolatey\windows-sdk-10.0.log"
    if errorlevel 1 exit /b %errorlevel%

    refreshenv
)

exit /b 0

@echo off
echo ============================================
echo  FolderCompare - Build Script
echo ============================================
echo.

:: Try MSYS2 UCRT64 g++ (installed)
if exist C:\msys64\usr\bin\bash.exe (
    echo [Compiler] MSYS2 ucrt64 g++
    echo [Build]    Compiling (static - no DLL needed)...
    C:\msys64\usr\bin\bash.exe -lc "PATH=/ucrt64/bin:$PATH && cd '/d/--App/Compare' && windres FolderCompare.rc -O coff -o FolderCompare.res && g++ FolderCompare.cpp FolderCompare.res -o FolderCompare.exe -mwindows -lcomctl32 -lshell32 -lole32 -lgdiplus -ldwmapi -luxtheme -std=c++17 -O2 -static-libgcc -static-libstdc++ -static"
    goto CHECK
)

:: Try system g++
where g++ >nul 2>&1
if %errorlevel%==0 (
    echo [Compiler] System g++
    windres FolderCompare.rc -O coff -o FolderCompare.res
    g++ -o FolderCompare.exe FolderCompare.cpp FolderCompare.res -mwindows -lcomctl32 -lshell32 -lole32 -lgdiplus -ldwmapi -luxtheme -std=c++17 -O2
    goto CHECK
)

:: Try MSVC cl
where cl >nul 2>&1
if %errorlevel%==0 (
    echo [Compiler] MSVC cl
    rc /fo FolderCompare.res FolderCompare.rc
    cl /nologo /EHsc /DUNICODE /D_UNICODE /std:c++17 /O2 FolderCompare.cpp ^
        FolderCompare.res /link /SUBSYSTEM:WINDOWS comctl32.lib shell32.lib ole32.lib user32.lib gdi32.lib
    goto CHECK
)

echo [ERROR] No compiler found. Install MSYS2 from https://www.msys2.org
pause
exit /b 1

:CHECK
if exist FolderCompare.exe (
    echo.
    echo [SUCCESS] FolderCompare.exe built!
    start FolderCompare.exe
) else (
    echo [FAILED] Check errors above.
)
pause

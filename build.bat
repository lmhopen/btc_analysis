@echo off
echo Compiling BTC Analysis Tool...
echo.

set W64DEVKIT=C:\raylib\w64devkit
set PATH=%W64DEVKIT%\bin;%W64DEVKIT%\libexec\gcc\x86_64-w64-mingw32\15.0.0
set RAYLIB=%W64DEVKIT%\x86_64-w64-mingw32

if not exist obj mkdir obj

echo [1/6] Compiling data.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\data.c -o obj\data.o
if errorlevel 1 goto error

echo [2/6] Compiling calculate.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\calculate.c -o obj\calculate.o
if errorlevel 1 goto error

echo [3/6] Compiling chart.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\chart.c -o obj\chart.o
if errorlevel 1 goto error

echo [4/6] Compiling crosshair.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\crosshair.c -o obj\crosshair.o
if errorlevel 1 goto error

echo [5/6] Compiling network.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\network.c -o obj\network.o
if errorlevel 1 goto error

echo [6/6] Compiling main.c...
gcc -Wall -O2 -std=c99 -Isrc -I%RAYLIB%\include -c src\main.c -o obj\main.o
if errorlevel 1 goto error

echo.
echo Linking...
gcc obj\data.o obj\calculate.o obj\chart.o obj\crosshair.o obj\network.o obj\main.o -o btc_analysis.exe -L%RAYLIB%\lib -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 goto error

echo.
echo ========================================
echo BUILD SUCCESS!
echo ========================================
echo Run btc_analysis.exe to start the program
echo.
echo Options:
echo   --no-update  : Skip network data update
echo   --no-save    : Skip auto-save chart image
echo.
pause
exit /b 0

:error
echo.
echo ========================================
echo BUILD FAILED!
echo ========================================
pause
exit /b 1

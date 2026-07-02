@echo off
rem ---------------------------------------------------------------------------
rem Generates build\FractalStudio.sln for Visual Studio (2022 or newer).
rem Edit QT_DIR to point at your Qt installation, then double-click this file.
rem ---------------------------------------------------------------------------

set QT_DIR=C:\Qt\6.7.2\msvc2022_64

rem Target GPU architecture: 89 = Ada (RTX 40xx), 86 = Ampere (RTX 30xx),
rem 75 = Turing (RTX 20xx). Set USE_CUDA=OFF for a CPU-only build.
set CUDA_ARCH=89
set USE_CUDA=ON

cmake -B build -S . -A x64 ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    -DUSE_CUDA=%USE_CUDA% ^
    -DCUDA_ARCH=%CUDA_ARCH%

if errorlevel 1 (
    echo.
    echo CMake configuration failed. Check QT_DIR and that the CUDA Toolkit
    echo is installed ^(or set USE_CUDA=OFF for a CPU-only build^).
) else (
    echo.
    echo Done. Open build\FractalStudio.sln, set FractalStudio as the startup
    echo project, choose Release ^| x64 and build.
)
pause

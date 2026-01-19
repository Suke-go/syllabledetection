@echo off
REM Wasm Build Script for Syllable Detector

set EMSDK_PATH=%~dp0..\..\emsdk
set EMCC=%EMSDK_PATH%\upstream\emscripten\emcc.bat
set LLVM_ROOT=%EMSDK_PATH%\upstream
set NODE_JS=%EMSDK_PATH%\node\22.16.0_64bit\bin\node.exe
set BINARYEN_ROOT=%EMSDK_PATH%\upstream

REM Set environment
set PATH=%EMSDK_PATH%\upstream\emscripten;%EMSDK_PATH%\upstream;%EMSDK_PATH%\node\22.16.0_64bit\bin;%PATH%

REM Create output directory
if not exist "wasm_build" mkdir wasm_build

echo Building Wasm...

%EMCC% ^
    -I ..\..\include ^
    -I ..\..\extern\kissfft ^
    ..\..\src\syllable_detector.c ^
    ..\..\src\dsp\agc.c ^
    ..\..\src\dsp\biquad.c ^
    ..\..\src\dsp\envelope.c ^
    ..\..\src\dsp\high_freq_energy.c ^
    ..\..\src\dsp\mfcc.c ^
    ..\..\src\dsp\spectral_flux.c ^
    ..\..\src\dsp\wavelet.c ^
    ..\..\src\dsp\zff.c ^
    ..\..\extern\kissfft\kiss_fft.c ^
    ..\..\extern\kissfft\kiss_fftr.c ^
    -O3 ^
    -s WASM=1 ^
    -s MODULARIZE=1 ^
    -s EXPORT_NAME="SyllableModule" ^
    -s EXPORTED_FUNCTIONS="['_syllable_create','_syllable_process','_syllable_flush','_syllable_destroy','_syllable_reset','_syllable_default_config','_syllable_set_realtime_mode','_syllable_recalibrate','_syllable_is_calibrating','_malloc','_free']" ^
    -s EXPORTED_RUNTIME_METHODS="['ccall','cwrap','setValue','getValue']" ^
    -s ALLOW_MEMORY_GROWTH=1 ^
    -s INITIAL_MEMORY=16MB ^
    -o wasm_build\syllable.js

if %ERRORLEVEL% EQU 0 (
    echo Build successful!
    echo Output: wasm_build\syllable.js and wasm_build\syllable.wasm
) else (
    echo Build failed with error code %ERRORLEVEL%
)

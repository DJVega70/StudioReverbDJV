@echo off
REM Requires: Visual Studio 2022 (Desktop development with C++), CMake, Git
cmake -B build -G "Visual Studio 17 2022" -A x64 || exit /b 1
cmake --build build --config Release --target StudioReverb_VST3 || exit /b 1
echo.
echo Built: build\StudioReverb_artefacts\Release\VST3\Studio Reverb.vst3
echo Copy that folder to C:\Program Files\Common Files\VST3\  (run as admin)

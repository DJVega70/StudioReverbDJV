# Studio Reverb (VST3)

C++/JUCE port of the Studio Reverb web app: synthetic-IR convolution reverb, low/high cut,
stereo chorus-modulated wet path, equal-power mix, output gain, spectrum display.

## Build on Windows
1. Install Visual Studio 2022 (Desktop development with C++), CMake and Git.
2. Run `build-windows.bat`.
3. Copy `build\StudioReverb_artefacts\Release\VST3\Studio Reverb.vst3` to
   `C:\Program Files\Common Files\VST3\` (admin).
4. FL Studio: Options > Manage plugins > Find installed plugins (verify/rescan).
   Add it from the mixer FX slot: Studio Reverb.

## No Visual Studio? Use GitHub Actions
Push this folder to a GitHub repo; the workflow in `.github/workflows/build.yml` builds on a
Windows runner. Download the `StudioReverb-VST3-win64` artifact from the Actions tab.

## Licensing
JUCE 8 is AGPLv3 / commercial and bundles the VST3 SDK. Fine for personal use; if you
distribute the plugin you must comply with AGPL (publish source) or buy a JUCE license.

# Building VintageDrummer Windows VST3 from source

## What was missing

The published source contained the sampler/DSP and UI, but no build target,
iPlug2/SDK revision, `resources/resource.h`, Windows `.rc` resource table, or
Roboto font definition/data and the BG_FN fallback definition. The code explicitly reads the samples, grooves and
images from RT_RCDATA resources. Merely adding include paths would not make a
self-contained working instrument.

This build restores that wiring without changing `VintageDrummer.cpp`,
`VintageDrummer.h`, `config.h`, audio/MIDI assets or the old release binary.
`background.png` is embedded as IDR_PNG_BG even though the external-file fallback
in the existing code looks for `bg.png`. All ten WAVs, seven MIDI grooves, both
PNGs and Roboto Regular are embedded. No sample pack beside the VST3 is required.

## Requirements

- Windows 10/11 **x64**.
- Visual Studio 2022 or Build Tools 2022, with **Desktop development with C++**,
  MSVC v143 x64/x86 tools and a Windows 10/11 SDK (including `rc.exe`).
- CMake 3.25 or newer; Git; Python 3.9 or newer, all on PATH.
- Internet during dependency setup and the first CMake configure.

Use a short writable source path, for example `C:\src\VintageDrummer`. No admin
access is needed to build. Open PowerShell in the repository root.

```powershell
git clone https://github.com/ZalexAlex1919/VintageDrummer.git
cd VintageDrummer
# If reviewing an unmerged build PR, switch to its branch first.
python scripts/check_build.py
python scripts/bootstrap.py
python scripts/check_build.py --dependencies
cmake --preset windows-vs2022-x64
cmake --build --preset windows-release --parallel 2
python scripts/check_build.py --plugin build/windows-x64/out/VintageDrummer.vst3/Contents/x86_64-win/VintageDrummer.vst3
```

The complete new plug-in is the **directory**:

```text
build/windows-x64/out/VintageDrummer.vst3/
  Contents/x86_64-win/VintageDrummer.vst3
```

Copy the whole outer `VintageDrummer.vst3` directory into a VST3 scan location,
for example `C:\Program Files\Common Files\VST3` (may need elevation). Close the
DAW first and remove/relocate its old copy to avoid duplicate IDs. Rescan and load
**vintage drummer** as an instrument. The build never reads or copies the existing
`vintage_drummer/VST/VintageDrummer.vst3` release file.

For IDE use, open `build/windows-x64/VintageDrummer.sln`, select Release/x64 and
build `VintageDrummer-vst3`. CMake generates the solution and project files;
there are no machine-specific `.vcxproj` files to maintain. Debug builds can be
made with `cmake --build build/windows-x64 --config Debug --target VintageDrummer-vst3`.
Debug and Release share the output path; keep separate build directories if needed.

## Dependencies and reproducibility limits

`dependencies.lock.json` pins full commit IDs for iPlug2 and VST3 SDK 3.8.0.
The SDK's base, pluginterfaces, public.sdk and cmake submodule revisions are
inherited from that pinned SDK commit. `bootstrap.py` fetches those commits and
leaves Git metadata for inspection. The font comes from the pinned iPlug2 example;
Roboto's Apache-2.0 notice is already in this repository.

The chosen renderer is NanoVG/OpenGL 2, using bundled iPlug2/WDL graphics sources;
Skia and downloaded prebuilt graphics libraries are unnecessary. The upstream
CMake modules also eagerly fetch WIL v1.0.240803.1 and WebView2 SDK 1.0.2903.40,
even though this instrument does not use WebView UI. These upstream versioned
network downloads are a remaining dependency/integrity limitation, not part of
the DSP. Do not substitute arbitrary iPlug2/SDK versions without retesting.

This is a pinned **source build recipe**, not a claim of bit-for-bit identity with
v1.0.1: its original compiler/framework versions are unknown. VS/SDK/CMake versions
and the hosted runner image can change. The resulting module is unsigned. Keep
this repository's LICENSE, LICENSES, THIRD_PARTY_NOTICES, iPlug2 LICENSE.txt and
SDK LICENSE.txt with redistributed builds; review upstream dependency notices too.

## Verification

`python scripts/check_build.py` runs on any OS: it checks every C++ resource ID
against the resource script, unique numeric IDs, asset paths and file signatures.
`--dependencies` verifies checked-out dependency and selected SDK submodule
revisions. These checks do not compile or test the DSP.

The GitHub Actions **Windows VST3 from source** workflow uses windows-2022 to
configure with MSVC, compile C++ and `.rc`, and inspect the newly built x64 module.
The Windows check compares every embedded resource and the font byte-for-byte
with their inputs, loads the DLL and checks the VST3 exports. This detects missing
resources/imports but does not instantiate the instrument or render audio. The
workflow uploads the newly compiled bundle and notices as an Actions artifact;
it does not modify or publish a GitHub release.

Required manual Windows/DAW acceptance checks:

1. Scan and instantiate in an x64 VST3 host; open/reopen the full UI and check
   background, logo, font and controls.
2. Trigger notes 24, 26, 27, 28, 29, 30, 31, 32, 33, 34 and their +12 aliases;
   check ten drum sounds, velocity, per-drum volume, master/room and hat choke.
3. Check groove list, playback if supported by the host, and MIDI drag/export;
   verify all seven embedded grooves without external asset directories.
4. Test 44.1/48/96 kHz, different block sizes, save/reload and multiple instances.
   Compare sound/behavior with v1.0.1; run Steinberg validator/pluginval if available.

Linux source checks cannot establish Windows compiler/linker success, DAW
compatibility, real-time safety or audible equivalence. Consult the actual Actions
run result before treating a proposed configuration as a verified Windows build.

## Troubleshooting

- Missing SDK/includes: rerun bootstrap and `--dependencies`; do not use an empty
  SDK directory containing only README.md.
- Wrong generator/platform or no compiler: install the C++ workload/SDK, use the
  VS2022 x64 preset, then regenerate in a clean build directory.
- Network error during CMake configure: check access to GitHub and NuGet for the
  upstream WIL/WebView2 setup; retry configure after restoring access.
- Silent instrument or missing graphics: run the Windows resource check against
  the **new** inner module; ensure the DAW has not loaded a duplicate old copy.

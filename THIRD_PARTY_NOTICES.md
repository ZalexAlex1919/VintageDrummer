# Third-Party Notices

This project includes or depends on third-party components. Their respective licenses apply.

## 1. iPlug2 Framework
- Project: https://github.com/iPlug2/iPlug2
- License: zlib-like license (see upstream `LICENSE.txt`)
- Role in project: plugin framework/runtime.

## 2. Steinberg VST 3 SDK
- Project: https://github.com/steinbergmedia/vst3sdk
- License: MIT (current VST3 SDK licensing)
- Role in project: VST3 format integration.

## 3. Roboto Font (Embedded in Binaries)
- Name: Roboto Regular
- Upstream: https://fonts.google.com/specimen/Roboto
- Copyright attribution (from binary font metadata):
  - "Font data copyright Google 2012"
  - Designer: Christian Robertson
- License: Apache License 2.0
- Role in project: UI text rendering (`Roboto-Regular` loaded by plugin UI code)
- Note: this font was detected in compiled plugin binaries (`VintageDrummer.vst3` and `VintageDrummer.dll`).

## 4. Third-Party Assets/Fonts in Repository Files
After scanning repository files of `v1.0.0`, no standalone third-party font files (`.ttf/.otf/.woff`) were found in the tracked source tree.
If new external assets/fonts are added later, append them here with:
- component name
- source URL
- license name
- required attribution text

## 5. VST2 Distribution Legal Note
If you distribute VST2 binaries (`.dll` VST2), ensure you are legally entitled to do so.
According to Steinberg licensing FAQ, VST2 binary distribution is allowed only for parties that signed the VST2 license agreement before October 2018.
If you do not have that entitlement, do not distribute VST2 builds.

Reference:
- https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html

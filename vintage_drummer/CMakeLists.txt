# The root is deliberately the original source directory: iPlug2 finds config.h here.
set(VD_FONT "${IPLUG2_DIR}/Examples/IPlugEffect/resources/fonts/Roboto-Regular.ttf")
configure_file(resources/plugin.rc.in "${CMAKE_CURRENT_BINARY_DIR}/plugin.rc" @ONLY)
iplug_add_plugin(VintageDrummer
  FORMATS VST3
  SOURCES VintageDrummer.cpp VintageDrummer.h config.h resources/resource.h
          "${CMAKE_CURRENT_BINARY_DIR}/plugin.rc"
  DEFINES ROBOTO_FN="Roboto-Regular.ttf" BG_FN="background.png"
)
target_include_directories(VintageDrummer-vst3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
# Resource file names are resolved through this include directory by rc.exe.
target_include_directories(VintageDrummer-vst3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

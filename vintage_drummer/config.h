#pragma once

#define PLUG_NAME "vintage drummer"
#define PLUG_MFR "VintageLabs"
#define PLUG_VERSION_HEX 0x00010000
#define PLUG_VERSION_STR "1.0.0"
#define PLUG_UNIQUE_ID 'VDrm'
#define PLUG_MFR_ID 'VLab'
#define PLUG_URL_STR "https://example.com"
#define PLUG_EMAIL_STR "support@example.com"
#define PLUG_COPYRIGHT_STR "Copyright 2026 VintageLabs"
#define PLUG_CLASS_NAME VintageDrummer

#define BUNDLE_NAME "VintageDrummer"
#define BUNDLE_MFR "VintageLabs"
#define BUNDLE_DOMAIN "com"

#ifndef BUNDLE_ID
#define BUNDLE_ID "com.vintagelabs.vintagedrummer"
#endif

#ifndef APP_GROUP_ID
#define APP_GROUP_ID ""
#endif

#define PLUG_CHANNEL_IO "0-2"
#define SHARED_RESOURCES_SUBPATH "VintageDrummer"

#define PLUG_LATENCY 0
#define PLUG_TYPE 1
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 980
#define PLUG_HEIGHT 620
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define PLUG_MIN_WIDTH PLUG_WIDTH
#define PLUG_MAX_WIDTH PLUG_WIDTH
#define PLUG_MIN_HEIGHT PLUG_HEIGHT
#define PLUG_MAX_HEIGHT PLUG_HEIGHT

#define AUV2_ENTRY VintageDrummer_Entry
#define AUV2_ENTRY_STR "VintageDrummer_Entry"
#define AUV2_FACTORY VintageDrummer_Factory
#define AUV2_VIEW_CLASS VintageDrummer_View
#define AUV2_VIEW_CLASS_STR "VintageDrummer_View"

#define AAX_TYPE_IDS 'VDm1', 'VDm2'
#define AAX_PLUG_MFR_STR "VLab"
#define AAX_PLUG_NAME_STR "VintageDrummer"
#define AAX_DOES_AUDIOSUITE 0
#define AAX_PLUG_CATEGORY_STR "Synth"

#define VST3_SUBCATEGORY "Instrument|Drum"
#define CLAP_MANUAL_URL "https://example.com/manual"
#define CLAP_SUPPORT_URL "https://example.com/support"
#define CLAP_DESCRIPTION "Vintage drum sampler"
#define CLAP_FEATURES "instrument", "sampler"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

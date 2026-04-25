#include "all_features.h"

#undef FEATURE_QUICK_ERASE    // Canon has it
#undef FEATURE_IMAGE_EFFECTS  // None working
#undef FEATURE_AF_PATTERNS    // Canon has it
#undef FEATURE_MLU_HANDHELD   // Not needed, Canon's silent mode is much better
#undef FEATURE_SHUTTER_LOCK   // Canon has a dedicated button for it
#undef FEATURE_IMAGE_POSITION // assume it is not needed with a variangle display
#undef FEATURE_ARROW_SHORTCUTS // No suitable button found

// TIMER_B has untraceable problems (causes banding/patterns)
// Timer A-only works via HiJello/FastTv setting (fps_criteria=3)
// David_Hugh found this workaround; recommended for 70D
// Previous QEMU crash was invalid (stale 25KB autoexec.bin on SD image)
// S3.1a: Confirmed booting in QEMU with proper 462KB build (2026-04-25)
// Build: 462KB with FPS override (+11KB vs 451KB baseline)
#define FEATURE_FPS_OVERRIDE

/* see comments in lens.c */
#undef FEATURE_FOLLOW_FOCUS
#undef FEATURE_RACK_FOCUS
#undef FEATURE_FOCUS_STACKING

/* disable RAW zebras as they cause problems in QR and LV too */
#undef FEATURE_RAW_ZEBRAS

// we got enough infos on main display and top lcd.
// Mainly this got disabled due to the bottom line
// (time, battery) flickering 
// ToDo: alternative coordinates not using outer ones
// custom builds may include it til we achieve it
#undef FEATURE_FLEXINFO

/* Audio Features */
#define FEATURE_AUDIO_METERS
#define FEATURE_AUDIO_REMOTE_SHOT

/* Features ported from 6D/5D3 (DIGIC V peers) */
#define FEATURE_ZOOM_TRICK_5D3 /* Double-click to zoom shortcut (5D3/6D) */
#define FEATURE_KEN_ROCKWELL_ZOOM_5D3 /* Zoom from image review mode (5D3/6D) */
#define FEATURE_SWAP_INFO_PLAY       /* Swap info display in playback mode (6D) */
#define FEATURE_LV_FOCUS_BOX_SNAP_TO_X5_RAW /* Snap focus box to x5 in raw mode (5D3) */
#define FEATURE_FOCUS_PEAK_DISP_FILTER /* Focus peaking as display filter (6D) */

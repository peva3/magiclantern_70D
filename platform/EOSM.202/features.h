#include "all_features.h"
/* Can't Be Implemented */
#undef FEATURE_FORCE_LIVEVIEW // Already Live View
#undef FEATURE_MLU // No Mirror
#undef FEATURE_MLU_HANDHELD
#undef FEATURE_STICKY_DOF // No DOF button
#undef FEATURE_IMAGE_EFFECTS // DigicV new effects check "Art Filter"
#undef FEATURE_INTERMEDIATE_ISO_PHOTO_DISPLAY // Well.. it will work in 1 mode
#undef FEATURE_AF_PATTERNS // No regular AF
#undef FEATURE_VOICE_TAGS // Just to be sure


/* Working — confirmed */
#define FEATURE_CROP_MODE_HACK
#define FEATURE_AUDIO_REMOTE_SHOT
#define FEATURE_SHUTTER_FINE_TUNING // "It works! Timer values not applied until record (normal for EOSM)"

//~ #define FEATURE_VIDEO_HACKS // unclean patching


/* Some Hope Yet — may work on this camera */
#undef FEATURE_TRAP_FOCUS
#undef FEATURE_FOLLOW_FOCUS
//#undef FEATURE_RACK_FOCUS  // enabled (was commented-out)
//#undef FEATURE_FOCUS_STACKING  // enabled (was commented-out)
#undef FEATURE_GHOST_IMAGE // No way to pick image but works.
#undef FEATURE_SET_MAINDIAL // Set taken over by Q
#undef FEATURE_PLAY_EXPOSURE_FUSION
#undef FEATURE_PLAY_COMPARE_IMAGES
#undef FEATURE_PLAY_TIMELAPSE
#undef FEATURE_PLAY_EXPOSURE_ADJUST
#undef FEATURE_QUICK_ZOOM
#undef FEATURE_QUICK_ERASE
#undef FEATURE_LV_FOCUS_BOX_FAST
#undef FEATURE_LV_FOCUS_BOX_SNAP
#undef FEATURE_ARROW_SHORTCUTS
#undef FEATURE_MAGIC_ZOOM_FULL_SCREEN // https://bitbucket.org/hudson/magic-lantern/issue/2272/full-screen-magic-zoom-is-garbled-on-700d

#define FEATURE_EYEFI_TRICKS

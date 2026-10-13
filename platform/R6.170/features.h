//#define CONFIG_HELLO_WORLD
#define FEATURE_VRAM_RGBA

#define CONFIG_COMPOSITOR_XCM

#define FEATURE_DONT_CLICK_ME
#define FEATURE_SHOW_SHUTTER_COUNT
#define FEATURE_SHOW_FREE_MEMORY

// TSKMON hooks every context switch and reads an unverified timer MMIO on R6;
// disabled while bisecting the boot freeze.
#undef CONFIG_TSKMON
#undef FEATURE_SHOW_TASKS
#undef FEATURE_SHOW_CPU_USAGE
#define FEATURE_SHOW_GUI_EVENTS

#undef CONFIG_ADDITIONAL_VERSION
#define FEATURE_SCREENSHOT

#define FEATURE_GLOBAL_DRAW
#define FEATURE_CROPMARKS

// Image capture not yet reliable on DIGIC X
#define CONFIG_IMAGE_CAPTURE_NOT_WORKING

#undef CONFIG_CRASH_LOG
#undef CONFIG_PROP_REQUEST_CHANGE
#undef CONFIG_AUTOBACKUP_ROM

// BISECT: skip task dispatch hook
#define CONFIG_NO_TASK_HOOK

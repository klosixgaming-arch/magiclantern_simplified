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

/* With CONFIG_NO_TASK_HOOK, TASK_OVERRIDE(gui_main_task, ml_gui_main_task) never
 * takes effect, so ML's button handlers never see any key. Poll Canon's GUI event
 * ring instead: TRASH toggles the ML menu, other keys are fed to the menu while open.
 * The poller stands down automatically if ml_gui_main_task ever starts running. */
#define CONFIG_GUI_EVENT_RING_POLL

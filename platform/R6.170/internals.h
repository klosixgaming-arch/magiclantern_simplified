/**
 * Camera internals for EOS R6 1.5.0
 */

/** This camera has a DIGIC X chip */
#define CONFIG_DIGIC_X

/** Digic 8 does not have bitmap font in ROM, try to load it from card **/
#define CONFIG_NO_BFNT

/** Execute platform prepare function in post_init_task **/
#define CONFIG_PLATFORM_POST_INIT

/** disable SRM for now
 * in current state SRM_AllocateMemoryResourceFor1stJob makes camera crash
 * even if just one buffer is requrested.
 */
#define CONFIG_MEMORY_SRM_NOT_WORKING

/* has LV */
#define CONFIG_LIVEVIEW

/* disable legacy state objects hooks on DIGIC X */
//#define CONFIG_STATE_OBJECT_HOOKS

#define CONFIG_TASK_STRUCT_V2_SMP
#define CONFIG_TASK_ATTR_STRUCT_V5
#define CONFIG_MALLOC_STRUCT_V2

/** \file
 * Common startup code (for all models).
 *
 * !!!!!! FOR NEW PORTS, READ PROPERTY.C FIRST !!!!!!
 * OTHERWISE YOU CAN CAUSE PERMANENT CAMERA DAMAGE
 * 
 */
/*
 * Copyright (C) 2009 Trammell Hudson <hudson+ml@osresearch.net>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "dryos.h"
#include "dryos_rpc.h"
#include "config.h"
#include "version.h"
#include "bmp.h"
#include "menu.h"
#include "property.h"
#include "consts.h"
#include "tskmon.h"
#include "rom_values.h"

#include <stdarg.h>
#include <stdio.h>
#include "boot-hack.h"
#include "ml-cbr.h"
#include "backtrace.h"

extern int uart_printf(const char * fmt, ...);

extern void platform_post_init();

#if defined(FEATURE_GPS_TWEAKS)
#include "gps.h"
#endif

#if defined(CONFIG_HELLO_WORLD)
#include "fw-signature.h"
#endif

#if defined(CONFIG_MMU_REMAP)
#include "patch.h"
#endif

static int _hold_your_horses = 1; // 0 after config is read
int ml_started = 0; // 1 after ML is fully loaded
int ml_gui_initialized = 0; // 1 after gui_main_task is started 
struct task *first_task = 0; // first item in the array of task structs

/**
 * Called by DryOS when it is dispatching (or creating?)
 * a new task.
 */
static void
my_task_dispatch_hook(
        struct context **p_context_old,    /* on new DryOS (6D+), this argument is different (small number, unknown meaning) */
        struct task *prev_task_unused,     /* only present on new DryOS */
        struct task *next_task_new         /* only present on new DryOS; old versions use HIJACK_TASK_ADDR */
)
{
    struct task * next_task = 
        #ifdef CONFIG_NEW_DRYOS_TASK_HOOKS
        next_task_new;
        #else
        *(struct task **)(HIJACK_TASK_ADDR);
        #endif

/* very verbose; disabled by default */
#undef DEBUG_TASK_HOOK
#ifdef DEBUG_TASK_HOOK
#ifdef CONFIG_NEW_DRYOS_TASK_HOOKS
    /* new DryOS */
    qprintf("[****] task_hook(%x) %x(%s) -> %x(%s), from %x\n",
        p_context_old,
        prev_task_unused, prev_task_unused ? prev_task_unused->name : "??",
        next_task, next_task ? next_task->name : "??",
        read_lr()
    );
#else
    /* old DryOS */
    qprintf("[****] task_hook(%x) -> %x(%s), from %x\n",
        p_context_old,
        next_task, next_task ? next_task->name : "??",
        read_lr()
    );
#endif  /* CONFIG_NEW_DRYOS_TASK_HOOKS */
#endif  /* DEBUG_TASK_HOOK */

    if (!next_task)
        return;

#ifdef CONFIG_NEW_DRYOS_TASK_HOOKS
    /* on new DryOS, first argument is not context; get it from the task structure */
    /* this also works for some models with old-style DryOS, but not all */
    struct context *context = next_task->context;
#else
    /* on old DryOS, context is passed as argument
     * on some models (not all!), it can be found in the task structure as well */
    struct context *context = p_context_old ? (*p_context_old) : 0;
#endif
    
    if (!context)
        return;
    
#ifdef CONFIG_TSKMON
    tskmon_task_dispatch(next_task);
#endif
    
    if (ml_started)
    {
        /* all task overrides should be done by now */
        return;
    }

    // Do nothing unless a new task is starting via the trampoile
    if(context->pc != (uint32_t)task_trampoline)
        return;

    thunk entry = (thunk) next_task->entry;

    qprintf("[****] Starting task %x(%x) %s\n", next_task->entry, next_task->arg, next_task->name);

    // Search the task_mappings array for a matching entry point
    extern struct task_mapping _task_overrides_start[];
    extern struct task_mapping _task_overrides_end[];
    struct task_mapping *mapping = _task_overrides_start;

    for( ; mapping < _task_overrides_end ; mapping++ )
    {
#if defined(POSITION_INDEPENDENT)
        mapping->replacement = PIC_RESOLVE(mapping->replacement);
#endif
        thunk original_entry = mapping->orig;
        if( original_entry != entry )
            continue;

/* -- can't call debugmsg from this context */
        qprintf("[****] Replacing task %x with %x\n",
            original_entry,
            mapping->replacement
        );

        next_task->entry = mapping->replacement;
        break;
    }
}

extern void dbg_blink(int n);

int current_init_func_idx = 0;
const char * current_init_func_name = "";

/** Call all of the init functions  */
static void
call_init_funcs()
{
    extern struct task_create _init_funcs_start[];
    extern struct task_create _init_funcs_end[];
    struct task_create * init_func = _init_funcs_start;

    int idx = 0;
    for( ; init_func < _init_funcs_end ; init_func++ )
    {
#if defined(POSITION_INDEPENDENT)
        init_func->entry = PIC_RESOLVE(init_func->entry);
        init_func->name = PIC_RESOLVE(init_func->name);
#endif
        current_init_func_idx = idx + 1;
        current_init_func_name = init_func->name ? init_func->name : "unknown";
        thunk entry = (thunk) init_func->entry;
        entry();
        idx++;
        if (idx == 12)
        {
            dbg_blink(3); // Milestone 3: first 12 init functions completed
        }
    }
    dbg_blink(4); // Milestone 4: all 24 init functions completed
}

static void nop( void ) { }
void menu_init( void ) __attribute__((weak,alias("nop")));
void debug_init( void ) __attribute__((weak,alias("nop")));

static int magic_off = 0; // Set to 1 to disable ML
static int magic_off_request = 0;

int magic_is_off() 
{
    return magic_off; 
}

void _disable_ml_startup() {
    magic_off_request = 1;
}

#if defined(CONFIG_AUTOBACKUP_ROM)

#define BACKUP_BLOCKSIZE 0x00100000

static void backup_region(char *file, uint32_t base, uint32_t length)
{
    FILE *handle = NULL;
    uint32_t size = 0;
    uint32_t pos = 0;

    if (base == 0 || length == 0)
        return;

    /* already backed up that region? */
    if((FIO_GetFileSize( file, &size ) == 0) && (size == length) )
    {
        return;
    }
    
    /* no, create file and store data */

    void* buf = malloc(BACKUP_BLOCKSIZE);
    if (!buf) return;

    handle = FIO_CreateFile(file);
    if (handle)
    {
      while(pos < length)
      {
         uint32_t blocksize = BACKUP_BLOCKSIZE;
        
          if(length - pos < blocksize)
          {
              blocksize = length - pos;
          }
          
          /* copy to RAM before saving, because ROM is slow and may interfere with LiveView */
          memcpy(buf, &((uint8_t*)base)[pos], blocksize);
          
          FIO_WriteFile(handle, buf, blocksize);
          pos += blocksize;
      }
      FIO_CloseFile(handle);
    }
    
    free(buf);
}

static void backup_rom_task()
{
    backup_region("ML/LOGS/ROM1.BIN", ROM1_ADDR, ROM1_SIZE);
    backup_region("ML/LOGS/ROM0.BIN", ROM0_ADDR, ROM0_SIZE);
}
#endif

#ifdef CONFIG_HELLO_WORLD

static void draw_test_pattern(int colour)
{
    uint8_t *b = bmp_vram();

    // draw a rectangle on the exact visible border
    for (int y=0; y < 480; y++)
    {
        bmp_putpixel_fast(b, 0, y, colour);
        bmp_putpixel_fast(b, 719, y, colour);
    }
    for (int x=0; x < 720; x++)
    {
        bmp_putpixel_fast(b, x, 0, colour);
        bmp_putpixel_fast(b, x, 479, colour);
    }
}

#ifdef FEATURE_VRAM_RGBA

/** kitor: This aint pretty, but we selectively call bmp init functions
 *  and run required tasks. Other solution would be to have function in bmp.c
 *  to break static scope.
 *
 * We should be safe to run them as:
 * - we are already past _mem_init()
 * - if this code runs, we successfully started ml_init task */
static void init_bmp_indexed()
{
    //first call bmp_init
    extern struct task_create _init_funcs_start[];
    extern struct task_create _init_funcs_end[];
    struct task_create * init_func = _init_funcs_start;

    for( ; init_func < _init_funcs_end ; init_func++ )
    {
        if(strcmp(init_func->name, "bmp_init") == 0)
        {
            thunk entry = (thunk) init_func->entry;
            entry();
            break;
        }
    }

    //then start redraw_task
    extern struct task_create _tasks_start[];
    extern struct task_create _tasks_end[];
    struct task_create * task = _tasks_start;

    for( ; task < _tasks_end ; task++ )
    {
        if(strcmp(task->name, "redraw_task") == 0)
        {
            task_create(
                task->name,
                task->priority,
                task->stack_size,
                task->entry,
                task->arg
            );
            break;
        }
    }
}
#endif

/* ---- Rung 4: On-Screen Display Test (FEATURE_VRAM_RGBA & CONFIG_COMPOSITOR_XCM) ----
 * Step-by-step test directly rendering to Canon's screen:
 *   1 blink:  8s wait completed, LiveView active
 *   2 blinks: MARV display surface successfully obtained via XCM
 *   Pulsing centered colored rectangle on LCD: SUCCESS!
 */
extern void* _pXCM;
extern struct MARV *XCM_GetSourceSurface(void *pXCM, uint32_t layer_id);
extern struct MARV *_rgb_vram_info;
extern int XimrExe(void *);
extern struct semaphore *winsys_sem;
extern void gui_enqueue_message(uint32_t ev_type, uint32_t ev_param, uint32_t arg3, uint32_t arg4);

static void blink_code(int count)
{
    for (int i = 0; i < count; i++)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(200);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
        msleep(200);
    }
    msleep(800);
}

static void led_pattern_forever(int pulses, int on_ms, int gap_ms, int pause_ms)
{
    while (1)
    {
        for (int i = 0; i < pulses; i++)
        {
            MEM(CARD_LED_ADDRESS) = LEDON;  msleep(on_ms);
            MEM(CARD_LED_ADDRESS) = LEDOFF; msleep(gap_ms);
        }
        msleep(pause_ms);
    }
}

#define FONTW 8
#define FONTH 8

static const uint8_t fontImg8x8[108][8] =
{
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0020 (space)
		{ 0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00}, // U+0021 (!)
		{ 0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0022 (")
		{ 0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00}, // U+0023 (#)
		{ 0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00}, // U+0024 ($)
		{ 0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00}, // U+0025 (%)
		{ 0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00}, // U+0026 (&)
		{ 0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0027 (')
		{ 0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00}, // U+0028 (()
		{ 0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00}, // U+0029 ())
		{ 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00}, // U+002A (*)
		{ 0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00}, // U+002B (+)
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+002C (,)
		{ 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}, // U+002D (-)
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+002E (.)
		{ 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00}, // U+002F (/)
		{ 0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00}, // U+0030 (0)
		{ 0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}, // U+0031 (1)
		{ 0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}, // U+0032 (2)
		{ 0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}, // U+0033 (3)
		{ 0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}, // U+0034 (4)
		{ 0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}, // U+0035 (5)
		{ 0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00}, // U+0036 (6)
		{ 0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00}, // U+0037 (7)
		{ 0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00}, // U+0038 (8)
		{ 0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00}, // U+0039 (9)
		{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00}, // U+003A (:)
		{ 0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06}, // U+003B (//)
		{ 0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00}, // U+003C (<)
		{ 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00}, // U+003D (=)
		{ 0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00}, // U+003E (>)
		{ 0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00}, // U+003F (?)
		{ 0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00}, // U+0040 (@)
		{ 0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}, // U+0041 (A)
		{ 0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}, // U+0042 (B)
		{ 0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}, // U+0043 (C)
		{ 0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}, // U+0044 (D)
		{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}, // U+0045 (E)
		{ 0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}, // U+0046 (F)
		{ 0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}, // U+0047 (G)
		{ 0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}, // U+0048 (H)
		{ 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0049 (I)
		{ 0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}, // U+004A (J)
		{ 0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}, // U+004B (K)
		{ 0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}, // U+004C (L)
		{ 0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}, // U+004D (M)
		{ 0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}, // U+004E (N)
		{ 0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}, // U+004F (O)
		{ 0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}, // U+0050 (P)
		{ 0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}, // U+0051 (Q)
		{ 0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}, // U+0052 (R)
		{ 0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}, // U+0053 (S)
		{ 0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0054 (T)
		{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}, // U+0055 (U)
		{ 0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0056 (V)
		{ 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}, // U+0057 (W)
		{ 0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}, // U+0058 (X)
		{ 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}, // U+0059 (Y)
		{ 0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}, // U+005A (Z)
		{ 0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00}, // U+005B ([)
		{ 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00}, // U+005C (\)
		{ 0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00}, // U+005D (])
		{ 0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00}, // U+005E (^)
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF}, // U+005F (_)
		{ 0x0C, 0x0C, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+0060 (`)
		{ 0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00}, // U+0061 (a)
		{ 0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00}, // U+0062 (b)
		{ 0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00}, // U+0063 (c)
		{ 0x38, 0x30, 0x30, 0x3e, 0x33, 0x33, 0x6E, 0x00}, // U+0064 (d)
		{ 0x00, 0x00, 0x1E, 0x33, 0x3f, 0x03, 0x1E, 0x00}, // U+0065 (e)
		{ 0x1C, 0x36, 0x06, 0x0f, 0x06, 0x06, 0x0F, 0x00}, // U+0066 (f)
		{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0067 (g)
		{ 0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00}, // U+0068 (h)
		{ 0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+0069 (i)
		{ 0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E}, // U+006A (j)
		{ 0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00}, // U+006B (k)
		{ 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}, // U+006C (l)
		{ 0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00}, // U+006D (m)
		{ 0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00}, // U+006E (n)
		{ 0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00}, // U+006F (o)
		{ 0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F}, // U+0070 (p)
		{ 0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78}, // U+0071 (q)
		{ 0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00}, // U+0072 (r)
		{ 0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00}, // U+0073 (s)
		{ 0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00}, // U+0074 (t)
		{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00}, // U+0075 (u)
		{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}, // U+0076 (v)
		{ 0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00}, // U+0077 (w)
		{ 0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00}, // U+0078 (x)
		{ 0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F}, // U+0079 (y)
		{ 0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00}, // U+007A (z)
		{ 0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00}, // U+007B ({)
		{ 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x00}, // U+007C (|)
		{ 0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00}, // U+007D (})
		{ 0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // U+007E (~)
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}  // U+007F
};

static void ml_font_draw_rgba(uint32_t *vram, uint32_t stride, uint32_t x_pos, uint32_t y_pos,
                              uint32_t fg_color, uint32_t bg_color, uint32_t scale, const char *text)
{
    if (!vram || !text) return;
    uint32_t length = strlen(text);
    if (scale == 0) scale = 1;

    for (uint32_t i = 0; i < length; i++)
    {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 0x20 || ch > 0x7E)
            ch = ' ';
        uint32_t ix = ch - 0x20;

        uint32_t char_x = x_pos + i * (FONTW * scale);

        for (uint32_t y = 0; y < FONTH * scale; y++)
        {
            uint32_t py = y_pos + y;
            uint32_t *row = &vram[py * stride];
            uint8_t font_row = fontImg8x8[ix][y / scale];

            for (uint32_t x = 0; x < FONTW * scale; x++)
            {
                uint32_t px = char_x + x;
                int is_set = (font_row & (1 << (x / scale))) != 0;
                if (is_set)
                {
                    row[px] = fg_color;
                }
                else if (bg_color != 0)
                {
                    row[px] = bg_color;
                }
            }
        }
    }
}

static void ml_font_printf_rgba(uint32_t *vram, uint32_t stride, uint32_t x_pos, uint32_t y_pos,
                                uint32_t fg_color, uint32_t bg_color, uint32_t scale, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ml_font_draw_rgba(vram, stride, x_pos, y_pos, fg_color, bg_color, scale, buf);
}

/* Cache maintenance to Point of Coherency (PoC):
 * Cortex-A53 / DIGIC X display controller and DMA engines read DRAM directly.
 * Calling DCCMVAC pushes CPU-cached writes to DRAM so the display controller
 * immediately sees solid, flicker-free pixels.
 */
static inline void clean_dcache_range(void *start, uint32_t size)
{
    uint32_t addr = (uint32_t)start & ~31;
    uint32_t end = (uint32_t)start + size;
    asm volatile("dsb sy" ::: "memory");
    for (; addr < end; addr += 32)
    {
        asm volatile("mcr p15, 0, %0, c7, c10, 1" : : "r" (addr));
    }
    asm volatile("dsb sy" ::: "memory");
}

#define RING_INDEX_ADDR   0x5474
#define RING_BUFFER_BASE  0x7F554

static const char *get_button_name(uint32_t code)
{
    switch (code)
    {
        case 0x14:
        case 0x15: return "TRASH";
        case 0xA7: return "JOYSTICK";
        case 0x12:
        case 0x13: return "PLAY";
        case 0x0A:
        case 0x0B: return "SET";
        case 0x0C:
        case 0x0D: return "MENU";
        case 0x0E:
        case 0x0F: return "INFO";
        case 0x1C:
        case 0x1D: return "MAGNIFY";
        case 0x22:
        case 0x23: return "M-Fn";
        case 0x2C:
        case 0x2D: return "Q";
        case 0x33:
        case 0x34: return "RATE";
        case 0x9F:
        case 0xA0: return "HALF-SHT";
        case 0xA1:
        case 0xA2: return "FULL-SHT";
        case 0x00: return "WHEEL L";
        case 0x01: return "WHEEL R";
        case 0x02: return "DIAL2 L";
        case 0x03: return "DIAL2 R";
        case 0x04: return "SET-W L";
        case 0x05: return "SET-W R";
        case 0x06: return "RING L";
        case 0x07: return "RING R";
        case 0x08: return "TOP-D L";
        case 0x09: return "TOP-D R";
        case 0x4E: return "EXP-D L";
        case 0x4F: return "EXP-D R";
        case 0x89: return "EXP-D L";
        case 0x8A: return "EXP-D R";
        case 0xB0:
        case 0xB2: return "DIAL L";
        case 0xB1:
        case 0xB3: return "DIAL R";
        case 0x3C:
        case 0x3D: return "JOY RGHT";
        case 0x3E:
        case 0x3F: return "JOY LEFT";
        case 0x40:
        case 0x41: return "JOY UP";
        case 0x42:
        case 0x43: return "JOY DOWN";
        case 0x76: return "JOY CLK";
        case 0x77: return "JOY UP";
        case 0x78: return "JOY UP-R";
        case 0x79: return "JOY UP-L";
        case 0x7A: return "JOY RGHT";
        case 0x7B: return "JOY LEFT";
        case 0x7C: return "JOY DN-R";
        case 0x7D: return "JOY DN-L";
        case 0x7E: return "JOY DOWN";
        case 0x7F: return "JOY CTR";
        case 0x80: return "JOY REL";
        case 0xBE: return "REC";
        case 0xBF: return "AF-PT";
        case 0xD9: return "LENS-FN";
        default:   return (code == 0) ? "WHEEL L" : "KEY";
    }
}

static void clear_overlay(uint32_t *vram, uint32_t stride,
                          uint32_t box_w, uint32_t box_h,
                          uint32_t x_start, uint32_t y_start)
{
    if (!vram) return;
    uint32_t x_end = x_start + box_w;
    uint32_t y_end = y_start + box_h;

    for (uint32_t y = y_start; y < y_end; y++)
    {
        uint32_t *row = &vram[y * stride];
        for (uint32_t x = x_start; x < x_end; x++)
        {
            row[x] = 0x00000000; /* transparent */
        }
    }
    for (uint32_t y = y_start; y < y_end; y++)
    {
        clean_dcache_range(&vram[y * stride + x_start], box_w * sizeof(uint32_t));
    }
}

static void draw_zebra_box(uint32_t *vram, uint32_t stride, int zebra_mode, int anim_offset)
{
    if (!vram || zebra_mode == 0) return;

    /* Mode 1: Mini Box in top-right corner; Mode 2: Center Zone */
    uint32_t zx = (zebra_mode == 1) ? 740 : 260;
    uint32_t zy = (zebra_mode == 1) ? 24  : 160;
    uint32_t zw = (zebra_mode == 1) ? 200 : 440;
    uint32_t zh = (zebra_mode == 1) ? 70  : 220;

    uint32_t col_stripe1 = (zebra_mode == 1) ? 0xFF00FFFF : 0xFF0055FF; /* Yellow (70%) or Orange (100%) */
    uint32_t col_stripe2 = 0xEB181414; /* dark charcoal */
    uint32_t col_border  = 0xFFFF0000; /* red border */

    /* Draw animated diagonal stripes */
    for (uint32_t y = zy; y < zy + zh; y++)
    {
        uint32_t *row = &vram[y * stride];
        for (uint32_t x = zx; x < zx + zw; x++)
        {
            if (y < zy + 3 || y >= zy + zh - 3 || x < zx + 3 || x >= zx + zw - 3)
            {
                row[x] = col_border;
            }
            else
            {
                int stripe = ((x + y + anim_offset) / 6) % 2;
                row[x] = (stripe == 0) ? col_stripe1 : col_stripe2;
            }
        }
    }

    /* Overlay label */
    const char *lbl = (zebra_mode == 1) ? "ZEBRA MINI" : "ZEBRA CENTER ZONE";
    ml_font_printf_rgba(vram, stride, zx + 12, zy + 12, 0xFF000000, 0x00000000, 2, lbl);
    ml_font_printf_rgba(vram, stride, zx + 12, zy + 38, 0xFFFFFFFF, 0x00000000, 2, "LV OVERLAY");

    for (uint32_t y = zy; y < zy + zh; y++)
    {
        clean_dcache_range(&vram[y * stride + zx], zw * sizeof(uint32_t));
    }
}

static void clear_zebra_box(uint32_t *vram, uint32_t stride)
{
    if (!vram) return;

    /* Clear both mini box and center zone */
    uint32_t zones[2][4] = {
        { 740, 24,  200, 70  },
        { 260, 160, 440, 220 }
    };

    for (int z = 0; z < 2; z++)
    {
        uint32_t zx = zones[z][0];
        uint32_t zy = zones[z][1];
        uint32_t zw = zones[z][2];
        uint32_t zh = zones[z][3];

        for (uint32_t y = zy; y < zy + zh; y++)
        {
            uint32_t *row = &vram[y * stride];
            for (uint32_t x = zx; x < zx + zw; x++)
            {
                row[x] = 0x00000000;
            }
            clean_dcache_range(&row[zx], zw * sizeof(uint32_t));
        }
    }
}

struct KeyEventLog {
    uint32_t slot;
    uint32_t ev_type;
    uint32_t ev_param;
    uint32_t ev_arg;
    uint32_t ev_obj;
    uint32_t seq;
};

#define KEY_HISTORY_MAX 10
static struct KeyEventLog key_history[KEY_HISTORY_MAX];
static uint32_t total_events_logged = 0;

/* Option A Telemetry Globals */
static uint16_t g_batt_mv   = 0;
static int8_t   g_batt_temp = 0;

/* Option E Shutter & Serial Telemetry Globals */
static uint32_t g_shutter_mech = 0;
static uint32_t g_shutter_elec = 0;
static uint32_t g_shutter_total = 0;
static char g_body_serial_str[32] = "PENDING";

/* Option F Audio Telemetry Globals */
static int g_audio_mic_plugged __attribute__((unused)) = 0;       /* PROP_MIC_PHYSICAL_CONNECT (0x8003002B) */
static int g_audio_hp_plugged __attribute__((unused)) = 0;        /* PROP_HEADPHONE_PHYSICAL_CONNECT (0x80030055) */
static int g_audio_hdmi_plugged __attribute__((unused)) = 0;      /* PROP_HDMI_PHYSICAL_CONNECT (0x8003002C) */
static int g_audio_rec_vol __attribute__((unused)) = 0;           /* PROP_MOVIE_REC_VOLUME (0x80000080) */
static int g_audio_rec_vol_line __attribute__((unused)) = 0;      /* PROP_MOVIE_REC_VOLUME_LINE (0x80000081) */
static int g_audio_rec_mode __attribute__((unused)) = 0;          /* PROP_MOVIE_SOUND_RECORD (0x80000082) */
static int g_audio_wind_cut __attribute__((unused)) = 0;          /* PROP_WIND_CUT (0x80000083) */
static int g_audio_attenuator __attribute__((unused)) = 0;        /* PROP_ATTENUATOR (0x80000084) */
static int g_audio_dual_out __attribute__((unused)) = 0;          /* PROP_DUAL_OUTPUT (0x800000CC) */
static int g_audio_buzzer __attribute__((unused)) = 0;            /* PROP_BUZZER (0x80020006) */
static uint32_t g_audio_ctx_addr __attribute__((unused)) = 0;
static uint32_t g_audio_sig_addr __attribute__((unused)) = 0;

/* Option B Screenshot HUD Globals */
static int screenshot_toast_frames = 0;
static char screenshot_toast_msg[64] = "";
static int screenshot_counter = 1;
static int last_screenshot_cycle = -100;

static void draw_key_hud(uint32_t *vram, uint32_t stride,
                         uint32_t box_w, uint32_t box_h, uint32_t x_start, uint32_t y_start,
                         uint32_t gui_mode, int cycle)
{
    if (!vram) return;

    uint32_t x_end = x_start + box_w;
    uint32_t y_end = y_start + box_h;
    uint32_t tx    = x_start + (box_w - 624) / 2;

    uint32_t col_cyan    = 0xFFFFFF00; /* Cyan */
    uint32_t col_green   = 0xFF00FF00; /* Green */
    uint32_t col_yellow  = 0xFF00FFFF; /* Yellow */
    uint32_t col_white   = 0xFFFFFFFF; /* White */
    uint32_t col_dark    = 0xEB141212; /* Dark charcoal */
    uint32_t col_row_bg  = 0xEB282222; /* Highlighted live row bg */
    uint32_t col_border  = col_cyan;

    /* 1. Solid background */
    for (uint32_t y = y_start; y < y_end; y++)
    {
        uint32_t *row = &vram[y * stride];
        for (uint32_t x = x_start; x < x_end; x++)
        {
            row[x] = col_dark;
        }
    }

    /* 2. 4-pixel Border */
    for (uint32_t y = y_start; y < y_end; y++)
    {
        uint32_t *row = &vram[y * stride];
        if (y < y_start + 4 || y >= y_end - 4)
        {
            for (uint32_t x = x_start; x < x_end; x++) row[x] = col_border;
        }
        else
        {
            for (uint32_t b = 0; b < 4; b++)
            {
                row[x_start + b] = col_border;
                row[x_end - 1 - b] = col_border;
            }
        }
    }

    /* 3. Header lines (All safely inside HUD box, below any display overscan) */
    ml_font_printf_rgba(vram, stride, tx, y_start + 8, col_cyan, col_dark, 2,
                        "[ OPTION F: AUDIO SUBSYSTEM & SHUTTER HUD ]");

    /* Shutter & Serial line (ALWAYS visible, never hidden by toasts!) */
    ml_font_printf_rgba(vram, stride, tx, y_start + 28, col_white, col_dark, 2,
                        "SHT: Mech=%d Elec=%d Tot=%d | SN:%s",
                        (int)g_shutter_mech, (int)g_shutter_elec, (int)g_shutter_total, g_body_serial_str);

    /* Audio Subsystem Telemetry line (ALWAYS visible) */
    ml_font_printf_rgba(vram, stride, tx, y_start + 48, col_green, col_dark, 2,
                        "AUD: Mic:%s HP:%s | Vol:%d Att:%s | WCut:%s",
                        g_audio_mic_plugged ? "EXT" : "INT",
                        g_audio_hp_plugged ? "ON" : "OFF",
                        g_audio_rec_vol,
                        g_audio_attenuator ? "ON" : "OFF",
                        g_audio_wind_cut ? "ON" : "OFF");

    /* Toast or Status banner line */
    if (screenshot_toast_frames > 0)
    {
        ml_font_printf_rgba(vram, stride, tx, y_start + 68, col_yellow, col_dark, 2,
                            "*** %s ***", screenshot_toast_msg);
        screenshot_toast_frames--;
    }
    else
    {
        ml_font_printf_rgba(vram, stride, tx, y_start + 68, 0xFF88CCFF, col_dark, 2,
                            "CODEC: 48kHz 24b LPCM | HDMI:%s Buzzer:%s",
                            g_audio_hdmi_plugged ? "ON" : "OFF",
                            g_audio_buzzer ? "ON" : "OFF");
    }

    /* Separator line */
    uint32_t sep1_y = y_start + 88;
    for (uint32_t x = x_start + 8; x < x_end - 8; x++) vram[sep1_y * stride + x] = col_cyan;

    /* 4. LIVE / MOST RECENT EVENT */
    if (total_events_logged > 0)
    {
        /* Highlight box for live event */
        for (uint32_t y = y_start + 92; y < y_start + 154; y++)
        {
            uint32_t *row = &vram[y * stride];
            for (uint32_t x = x_start + 8; x < x_end - 8; x++) row[x] = col_row_bg;
        }

        uint32_t p = key_history[0].ev_param;
        uint32_t a = key_history[0].ev_arg;
        uint32_t o = key_history[0].ev_obj;
        int16_t dy = (int16_t)(a >> 16);
        int16_t dx = (int16_t)(a & 0xFFFF);

        ml_font_printf_rgba(vram, stride, tx, y_start + 94, col_yellow, col_row_bg, 2,
                            "-> LIVE: 0x%02X (%-3d) [%-9.9s]  #%-5d",
                            p, p, get_button_name(p), key_history[0].seq);
        ml_font_printf_rgba(vram, stride, tx, y_start + 114, col_white, col_row_bg, 2,
                            "   ARG : 0x%08X  [H:%-4d L:%-4d]",
                            a, dy, dx);
        ml_font_printf_rgba(vram, stride, tx, y_start + 134, col_green, col_row_bg, 2,
                            "   OBJ : 0x%08X  Dec:%-10d",
                            o, (int32_t)o);
    }
    else
    {
        ml_font_printf_rgba(vram, stride, tx, y_start + 114, col_yellow, col_dark, 2,
                            ">>> READY: PRESS BUTTON / ROTATE DIAL <<<");
    }

    /* Separator line */
    uint32_t sep2_y = y_start + 158;
    for (uint32_t x = x_start + 8; x < x_end - 8; x++) vram[sep2_y * stride + x] = col_cyan;

    /* 5. TABLE HEADER */
    ml_font_printf_rgba(vram, stride, tx, y_start + 164, col_cyan, col_dark, 2,
                        "#  KEY  DEC NAME      ARG(HEX) OBJ(HEX)");

    uint32_t sep3_y = y_start + 184;
    for (uint32_t x = x_start + 8; x < x_end - 8; x++) vram[sep3_y * stride + x] = col_cyan;

    /* 6. HISTORY ROWS (Last 10 events) */
    for (int r = 0; r < KEY_HISTORY_MAX; r++)
    {
        uint32_t ry = y_start + 190 + r * 19;
        if (key_history[r].seq > 0)
        {
            uint32_t fg = (r == 0) ? col_yellow : col_white;
            ml_font_printf_rgba(vram, stride, tx, ry, fg, col_dark, 2,
                                "%02d 0x%02X %-3d %-9.9s %08X %08X",
                                (r + 1),
                                key_history[r].ev_param,
                                key_history[r].ev_param,
                                get_button_name(key_history[r].ev_param),
                                key_history[r].ev_arg,
                                key_history[r].ev_obj);
        }
        else
        {
            ml_font_printf_rgba(vram, stride, tx, ry, 0xFF888888, col_dark, 2,
                                "%02d ---- --- --------- -------- --------");
        }
    }

    /* 7. Flush cache */
    for (uint32_t y = y_start; y < y_end; y++)
    {
        clean_dcache_range(&vram[y * stride + x_start], box_w * sizeof(uint32_t));
    }
}

static void dump_marv_to_file(void)
{
    static char buf[4096];
    int len = 0;

    uint32_t pMarv = 0x14B000;
    uint32_t pDesc = *(volatile uint32_t *)pMarv;
    if (!pDesc)
    {
        pMarv = 0x18384;
        pDesc = *(volatile uint32_t *)pMarv;
    }

    len += snprintf(buf + len, sizeof(buf) - len, "DESC=%X\n", pDesc);

    if (pDesc)
    {
        for (int i = 0; i <= 14; i++)
        {
            uint32_t val = *(volatile uint32_t *)(pDesc + i * 4);
            len += snprintf(buf + len, sizeof(buf) - len, "%X=%X\n", i * 4, val);
        }

        uint32_t pBmp = *(volatile uint32_t *)(pDesc + 4);
        len += snprintf(buf + len, sizeof(buf) - len, "BMP_FIRST_16:\n");
        if (pBmp)
        {
            for (int i = 0; i < 16; i++)
            {
                uint32_t val = *(volatile uint32_t *)(pBmp + i * 4);
                len += snprintf(buf + len, sizeof(buf) - len, "%X=%X\n", i, val);
            }

            len += snprintf(buf + len, sizeof(buf) - len, "ROW1_AT_4096:\n");
            for (int i = 0; i < 8; i++)
            {
                uint32_t val = *(volatile uint32_t *)(pBmp + 4096 + i * 4);
                len += snprintf(buf + len, sizeof(buf) - len, "%X=%X\n", i, val);
            }

            len += snprintf(buf + len, sizeof(buf) - len, "ROW1_AT_3840:\n");
            for (int i = 0; i < 8; i++)
            {
                uint32_t val = *(volatile uint32_t *)(pBmp + 3840 + i * 4);
                len += snprintf(buf + len, sizeof(buf) - len, "%X=%X\n", i, val);
            }
        }
    }

    /* Also dump Layer 1 if present */
    uint32_t pDesc1 = *(volatile uint32_t *)0x14B00C;
    if (pDesc1 && pDesc1 != pDesc)
    {
        len += snprintf(buf + len, sizeof(buf) - len, "\nLAYER1_DESC=%X\n", pDesc1);
        for (int i = 0; i <= 14; i++)
        {
            uint32_t val = *(volatile uint32_t *)(pDesc1 + i * 4);
            len += snprintf(buf + len, sizeof(buf) - len, "L1_%X=%X\n", i * 4, val);
        }
    }

    /* Write to root of SD card */
    const char *targets[] = { "MARVDUMP.TXT", "A:/MARVDUMP.TXT", "B:/MARVDUMP.TXT" };
    for (int t = 0; t < 3; t++)
    {
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, buf, len);
            FIO_CloseFile(f);
        }
    }
}

struct KeyLogEntry {
    uint32_t ev_param;
    uint32_t ev_arg;
    uint32_t ev_obj;
};
#define KEY_FILE_LOG_MAX 250
static struct KeyLogEntry key_file_log[KEY_FILE_LOG_MAX];
static uint32_t key_file_log_count = 0;
static uint32_t last_flushed_key_count = 0;

static void flush_keylog_to_file(void)
{
    if (key_file_log_count == last_flushed_key_count || key_file_log_count == 0) return;

    static char kbuf[8192];
    int len = 0;
    len += snprintf(kbuf + len, sizeof(kbuf) - len, "=== EOS R6 HARDWARE KEY LOG (Auto-Recorded) ===\n");
    len += snprintf(kbuf + len, sizeof(kbuf) - len, "#    HEX   DEC  NAME       ARG(HEX)   OBJ(HEX)\n");

    for (uint32_t i = 0; i < key_file_log_count && i < KEY_FILE_LOG_MAX; i++)
    {
        uint32_t p = key_file_log[i].ev_param;
        uint32_t a = key_file_log[i].ev_arg;
        uint32_t o = key_file_log[i].ev_obj;
        len += snprintf(kbuf + len, sizeof(kbuf) - len,
                        "%03d  0x%02X  %d  %s  %08X  %08X\n",
                        i + 1, p, p, get_button_name(p), a, o);
        if (len > (int)sizeof(kbuf) - 100) break;
    }

    const char *targets[] = { "KEYLOG.TXT", "A:/KEYLOG.TXT", "B:/KEYLOG.TXT" };
    for (int t = 0; t < 3; t++)
    {
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, kbuf, len);
            FIO_CloseFile(f);
        }
    }
    last_flushed_key_count = key_file_log_count;
}

static void dump_sw_table_to_file(void)
{
    static char sbuf[4096];
    int len = 0;
    len += snprintf(sbuf + len, sizeof(sbuf) - len, "=== EOS R6 ROM SWITCH FUNCTION TABLE (0xE0109CE4) ===\n");
    len += snprintf(sbuf + len, sizeof(sbuf) - len, "IDX  HANDLER     DESCRIPTION\n");

    uint32_t tbl = 0xE0109CE4;
    for (int i = 0; i < 46; i++)
    {
        uint32_t fn = *(volatile uint32_t *)(tbl + i * 4);
        len += snprintf(sbuf + len, sizeof(sbuf) - len, "[%02d] 0x%08X\n", i, fn);
        if (len > (int)sizeof(sbuf) - 60) break;
    }

    const char *targets[] = { "SW_TABLE.TXT", "A:/SW_TABLE.TXT", "B:/SW_TABLE.TXT" };
    for (int t = 0; t < 3; t++)
    {
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, sbuf, len);
            FIO_CloseFile(f);
        }
    }
}

static void dump_option_a_diagnostics(void)
{
    static char out_buf[32768];
    int out_len = 0;

    /* Read Battery / Parameter State at 0x0011D28C */
    uint32_t batt_base = 0x0011D28C;
    g_batt_mv   = *(volatile uint16_t*)(batt_base + 0);
    g_batt_temp = *(volatile int8_t*)(batt_base + 4);

    /* 1. Header */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "================================================================================\n"
                 "=== CANON EOS R6 FIRMWARE 1.7.0 -- OPTION A DIAGNOSTIC & TELEMETRY REPORT ====\n"
                 "================================================================================\n"
                 "Architecture : Dual-Core Cortex-A9 | DIGIC X | DryOS Kernel\n\n");

    /* 2. Hardware Status & Battery Telemetry */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "--- [1] HARDWARE STATUS & BATTERY TELEMETRY ---\n"
                 "Battery Status Word    : 0x%04X (%d bars)\n"
                 "Internal Battery Temp  : %d C\n"
                 "EFIC Sensor Temp       : %d C\n\n",
                 (uint32_t)g_batt_mv, (int)g_batt_mv,
                 (int)g_batt_temp,
                 efic_temp);

    /* Raw Parameter Block Dump (64 bytes) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "Raw 64-Byte Parameter Block Dump (0x0011D28C):\n");
    for (int r = 0; r < 4; r++)
    {
        uint32_t off = r * 16;
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                     "  +0x%02X: %08X %08X %08X %08X\n",
                     off,
                     *(volatile uint32_t*)(batt_base + off),
                     *(volatile uint32_t*)(batt_base + off + 4),
                     *(volatile uint32_t*)(batt_base + off + 8),
                     *(volatile uint32_t*)(batt_base + off + 12));
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 3. Active DryOS Task */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "--- [2] ACTIVE DRYOS TASK DESCRIPTOR ---\n");
    struct task *cur = *(struct task **)0x1028;
    if (cur && (uint32_t)cur >= 0x1000 && (uint32_t)cur <= 0x3FFFFFF0)
    {
        const char *tname = cur->name;
        if ((uint32_t)tname < 0x1000 || ((uint32_t)tname > 0x3FFFFFF0 && (uint32_t)tname < 0xE0000000))
        {
            tname = "???";
        }
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                     "Task ID    : %d\n"
                     "Task Name  : %s\n"
                     "Priority   : 0x%02X\n"
                     "State      : 0x%02X\n"
                     "CPU Core   : %d (requested %d)\n"
                     "Stack Base : 0x%08X (Size: 0x%06X)\n"
                     "Entry Point: 0x%08X\n\n",
                     cur->taskId & 0xFF,
                     tname,
                     cur->run_prio,
                     cur->currentState,
                     cur->cpu_assigned, cur->cpu_requested,
                     cur->stackStartAddr, cur->stackSize,
                     (uint32_t)cur->entry);
    }

    /* 4. DryOS Vector Names (640 entries from 0x0002F810) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "--- [3] DRYOS INTERRUPT & EVENT VECTOR REGISTRY (0x0002F810) ---\n"
                 "IDX  VECTOR_NAME\n");

    uint32_t *vtable = (uint32_t *)0x0002F810;
    int vcount = 0;
    for (int i = 0; i < 640; i++)
    {
        char *vname = (char *)vtable[i];
        if (vname && (((uint32_t)vname >= 0x1000 && (uint32_t)vname <= 0x3FFFFFF0) ||
                      ((uint32_t)vname >= 0xE0000000 && (uint32_t)vname <= 0xEFFFFFFF)))
        {
            if (vname[0] >= 32 && vname[0] <= 126)
            {
                if (out_len < (int)sizeof(out_buf) - 100)
                {
                    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                                 "[%03d] %s\n", i, vname);
                    vcount++;
                }
            }
        }
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "Total Registered Vectors: %d\n\n", vcount);

    /* 5. DryOS Task Vector Registry (64 entries from 0x00031A34) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "--- [4] DRYOS TASK VECTOR REGISTRY (0x00031A34) ---\n"
                 "IDX  TASK_VECTOR_NAME\n");

    uint32_t *tvtable = (uint32_t *)0x00031A34;
    int tvcount = 0;
    for (int i = 0; i < 64; i++)
    {
        char *tvname = (char *)tvtable[i];
        if (tvname && (((uint32_t)tvname >= 0x1000 && (uint32_t)tvname <= 0x3FFFFFF0) ||
                       ((uint32_t)tvname >= 0xE0000000 && (uint32_t)tvname <= 0xEFFFFFFF)))
        {
            if (tvname[0] >= 32 && tvname[0] <= 126)
            {
                if (out_len < (int)sizeof(out_buf) - 100)
                {
                    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                                 "[%02d] %s\n", i, tvname);
                    tvcount++;
                }
            }
        }
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                 "Total Registered Task Vectors: %d\n\n", tvcount);

    /* Single Atomic File Write */
    const char *targets[] = { "OPTION_A.TXT", "A:/OPTION_A.TXT", "B:/OPTION_A.TXT" };
    for (int t = 0; t < 3; t++)
    {
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, out_buf, out_len);
            FIO_CloseFile(f);
            break;
        }
    }
}

static FILE *open_file_for_writing(const char *basename);

/* =========================================================================
 * OPTION D: 14-BIT BAYER RAW SENSOR STREAM & EDMAC VIDEO ARCHITECTURE
 * ========================================================================= */

static void save_raw_sensor_buffer(const char *filename, uint32_t buf_addr, uint32_t size)
{
    /* Safety filter: must be in DRAM, non-null, size between 1KB and 64MB */
    if (!buf_addr || size < 1024 || size > 64 * 1024 * 1024)
    {
        return;
    }
    uint32_t phys = buf_addr & 0x3FFFFFFF;
    if (phys < 0x00100000 || phys >= 0x40000000)
    {
        return;
    }

    FILE *f = open_file_for_writing(filename);
    if (!f) return;

    const uint32_t chunk_size = 64 * 1024;
    uint32_t written = 0;
    while (written < size)
    {
        uint32_t to_write = (size - written > chunk_size) ? chunk_size : (size - written);
        FIO_WriteFile(f, (const void *)(buf_addr + written), to_write);
        written += to_write;
        msleep(1);
    }
    FIO_CloseFile(f);
}

static void dump_option_d_raw_diagnostics(void)
{
    static char out_buf[16384];
    int out_len = 0;

    volatile uint32_t *raw_desc  = (volatile uint32_t *)0x0001EF88;
    volatile uint32_t *dump_ctrl = (volatile uint32_t *)0x00030278;

    /* Actively latch live sensor raw stream (safe direct descriptor enable) */
    dump_ctrl[1] = 1; /* [0x0003027C] = 1 */
    raw_desc[11] = 1; /* [0x0001EFB4] = 1 */


    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "================================================================================\n"
        "=== CANON EOS R6 FIRMWARE 1.7.0 -- OPTION D: RAW SENSOR & EDMAC REPORT       ===\n"
        "================================================================================\n"
        "Architecture : Dual-Core Cortex-A53 | DIGIC X | DryOS Kernel\n\n");

    /* 1. LiveView Drive RAW Descriptor Block (0x0001EF88 - Mem1Path) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [1] LIVEVIEW SENSOR RAW DESCRIPTOR (0x0001EF88 - Mem1Path) ---\n");

    uint32_t raw_bpp        = raw_desc[4];  /* +0x10 = 0x1EF98 */
    uint32_t raw_width      = (raw_desc[5] > 0) ? raw_desc[5] : raw_desc[9];   /* 1832 px */
    uint32_t raw_height     = (raw_desc[6] > 0) ? raw_desc[6] : raw_desc[10];  /* 1220 px */
    uint32_t raw_pitch      = raw_desc[7];  /* +0x1C = 0x1EFA4 */
    uint32_t raw_size       = raw_desc[8];  /* +0x20 = 0x1EFA8 */
    if (raw_size < 100000 && raw_pitch > 0 && raw_height > 0)
    {
        raw_size = raw_pitch * raw_height; /* 1888 * 1220 = 2,303,360 bytes */
    }
    uint32_t raw_crop_x     = raw_desc[9];  /* +0x24 = 0x1EFAC */
    uint32_t raw_crop_y     = raw_desc[10]; /* +0x28 = 0x1EFB0 */
    uint32_t raw_buf_low    = raw_desc[20]; /* +0x50 = 0x1EFD8 */
    uint32_t raw_buf_high   = raw_desc[21]; /* +0x54 = 0x1EFDC */
    uint32_t crop_w         = raw_desc[22]; /* +0x58 = 0x1EFE0 */
    uint32_t crop_h         = raw_desc[23]; /* +0x5C = 0x1EFE4 */

    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "Sensor Raw BPP          : %d bpp\n"
        "Sensor Raw Width        : %d px\n"
        "Sensor Raw Height       : %d px\n"
        "Sensor Line Pitch       : %d bytes\n"
        "Frame Buffer Size       : %d bytes (0x%08X)\n"
        "Physical Buffer Address : 0x%08X (High: 0x%08X)\n"
        "Active Sensor Crop      : %d x %d (Offset: %d, %d)\n\n",
        raw_bpp, raw_width, raw_height, raw_pitch, raw_size, raw_size,
        raw_buf_low, raw_buf_high, crop_w, crop_h, raw_crop_x, raw_crop_y);

    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "Raw 64-Byte Descriptor Memory Dump (0x0001EF88):\n");
    for (int r = 0; r < 16; r += 4)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "  +0x%02X: %08X %08X %08X %08X\n",
            r * 4, raw_desc[r], raw_desc[r+1], raw_desc[r+2], raw_desc[r+3]);
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 2. LiveView Raw Dump Controller Block (0x00030278) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [2] LIVEVIEW RAW DUMP CONTROLLER (0x00030278) ---\n"
        "Raw 32-Byte Controller State Dump (0x00030278):\n");
    for (int r = 0; r < 8; r += 4)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "  +0x%02X: %08X %08X %08X %08X\n",
            r * 4, dump_ctrl[r], dump_ctrl[r+1], dump_ctrl[r+2], dump_ctrl[r+3]);
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 3. Bayer Raw Sensor Buffer Memory Validation */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [3] LIVE BAYER RAW PIXEL BUFFER PROBE ---\n");
    uint32_t buf_ptr = raw_buf_low;
    if ((buf_ptr >= 0x00100000 && buf_ptr < 0x3FFFFFFF) ||
        (buf_ptr >= 0x40100000 && buf_ptr < 0x7FFFFFFF))
    {
        volatile uint16_t *pixels = (volatile uint16_t *)buf_ptr;
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "Buffer Pointer Valid    : YES (0x%08X)\n"
            "First 16 Bayer Pixels (14-bit):\n  ", buf_ptr);
        for (int p = 0; p < 16; p++)
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                "%04X ", pixels[p]);
        }
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

        uint32_t min_val = 0xFFFF, max_val = 0, sum = 0;
        for (int p = 0; p < 512; p++)
        {
            uint16_t v = pixels[p];
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
            sum += v;
        }
        uint32_t avg = sum / 512;
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "Sample Black Level Min  : %d (0x%04X)\n"
            "Sample Signal Max       : %d (0x%04X)\n"
            "Sample Average Level    : %d (0x%04X)\n\n",
            min_val, min_val, max_val, max_val, avg, avg);
    }
    else
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "Buffer Pointer Status   : PENDING / NOT ALLOCATED (0x%08X)\n"
            "Note                    : Sensor pipeline initializes upon entering LiveView recording mode\n\n",
            buf_ptr);
    }

    /* 4. Canon Firmware RAW Native Hooks */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [4] CANON DIGIC X NATIVE RAW HOOK ENTRY POINTS ---\n"
        "lv_save_raw             : 0xE015FA34 (Trigger LiveView RAW write latch)\n"
        "lv_raw_dump             : 0xE015FB38 (Full raw frame dump to SD card)\n"
        "get_lv_raw_buffer       : 0xE0381838 (Returns 64-bit physical address [0x1EFD8])\n"
        "get_lv_raw_size         : 0xE038180E (Calculates (w * h * bpp) / 8)\n"
        "lv_capraw_dump          : 0xE0160052 (Capture RAW dump)\n"
        "lv_raw_100dump          : 0xE015FF7C (Burst 100-frame raw sequence)\n\n");

    /* 5. Video EDMAC Channel Architecture */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [5] VIDEO EDMAC HARDWARE ENGINE (0xE1A0BEF8 / 0xE1A0C830) ---\n"
        "EDMAC Channel 0 Control : 0xD2406000\n"
        "EDMAC Channel 1 Control : 0xD2422000 | Address: 0xD2424000 | Connect: 0xD2425000\n"
        "EDMAC Channel 2 Control : 0xD2422100 | Address: 0xD2424040 | Connect: 0xD2425080\n"
        "EDMAC Channel 3 Control : 0xD2422200 | Address: 0xD2424080 | Connect: 0xD24250C0\n"
        "EDMAC Channel Stride    : 0x100 bytes (Control), 0x40 bytes (Address)\n"
        "Status Register Offset  : +0x00B8\n"
        "Transfer Size Register  : +0x2800\n");

    /* Single Atomic File Write */
    const char *targets[] = { "OPTION_D_RAW_REPORT.TXT", "OPTION_D.TXT", "A:/OPTION_D.TXT", "B:/OPTION_D.TXT" };
    for (int t = 0; t < 4; t++)
    {
        FIO_RemoveFile(targets[t]);
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, out_buf, out_len);
            FIO_CloseFile(f);
            break;
        }
    }
}

/* =========================================================================
 * OPTION E: DRYOS PROPERTY DATABASE & SHUTTER COUNT EXTRACTION
 * ========================================================================= */

static void dump_option_e_properties(void)
{
    static char out_buf[32768];
    int out_len = 0;

    /* 1. Extract Physical Shutter Counts via Native Canon SCS Routine */
    typedef int (*scs_read_count_fn)(int dev, int mode, uint32_t *mech_count, uint32_t *elec_count);
    scs_read_count_fn scsReadReleaseCount = (scs_read_count_fn)0xE05615B3;

    uint32_t mech = 0, elec = 0;
    int scs_ret = scsReadReleaseCount(0, 0, &mech, &elec);
    g_shutter_mech = mech;
    g_shutter_elec = elec;
    g_shutter_total = mech + elec;

    /* 2. Query Properties via PROPAD_GetPropertyAddress */
    typedef int (*propad_get_addr_fn)(uint32_t prop_id, uint32_t *addr_out, uint32_t *len_out);
    propad_get_addr_fn PROPAD_GetPropertyAddress = (propad_get_addr_fn)0xE07E0743;

    uint32_t cig_addr = 0, cig_len = 0;
    int cig_ret = PROPAD_GetPropertyAddress(0x12000004, &cig_addr, &cig_len);

    uint32_t srv_addr = 0, srv_len = 0;
    int srv_ret = PROPAD_GetPropertyAddress(0x12000005, &srv_addr, &srv_len);

    uint32_t err_addr = 0, err_len = 0;
    int err_ret = PROPAD_GetPropertyAddress(0x12000003, &err_addr, &err_len);

    /* 3. Query Camera Body ID via prop_get_value (0xE07CA5F3) */
    typedef int (*prop_get_fn)(uint32_t prop_id, void **buf_out, uint32_t *len_out);
    prop_get_fn prop_get_value = (prop_get_fn)0xE07CA5F3;

    void *body_id_buf = NULL;
    uint32_t body_id_len = 0;
    int body_ret = prop_get_value(0x01000006, &body_id_buf, &body_id_len);

    if (body_ret == 0 && body_id_buf && body_id_len >= 8)
    {
        uint64_t bid = 0;
        memcpy(&bid, body_id_buf, 8);
        if (bid > 0)
        {
            uint32_t s_hi = (uint32_t)(bid / 1000000000ULL);
            uint32_t s_lo = (uint32_t)(bid % 1000000000ULL);
            snprintf(g_body_serial_str, sizeof(g_body_serial_str), "%03d%09d", (int)s_hi, (int)s_lo);
        }
    }
    else if (cig_ret == 0 && cig_addr >= 0x1000 && cig_len >= 8)
    {
        uint64_t bid = 0;
        memcpy(&bid, (const void *)cig_addr, 8);
        if (bid > 0)
        {
            uint32_t s_hi = (uint32_t)(bid / 1000000000ULL);
            uint32_t s_lo = (uint32_t)(bid % 1000000000ULL);
            snprintf(g_body_serial_str, sizeof(g_body_serial_str), "%03d%09d", (int)s_hi, (int)s_lo);
        }
    }

    /* 4. Format Report Header */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "================================================================================\n"
        "=== CANON EOS R6 FIRMWARE 1.7.0 -- OPTION E: PROPERTY & SHUTTER REPORT       ===\n"
        "================================================================================\n"
        "Architecture : Dual-Core Cortex-A53 | DIGIC X | DryOS Kernel\n"
        "Camera Model : Canon EOS R6 (0x%08X)\n"
        "Body Serial  : %s\n\n",
        (uint32_t)0x80000453, g_body_serial_str);

    /* 5. Shutter Count Telemetry */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [1] PHYSICAL SHUTTER ACTUATION TELEMETRY ---\n"
        "Mechanical Shutter Count : %d actuations\n"
        "Electronic Shutter Count : %d actuations\n"
        "Total Shutter Actuations : %d actuations\n"
        "Native SCS Status Return : 0x%08X\n\n",
        (int)g_shutter_mech, (int)g_shutter_elec, (int)g_shutter_total, (uint32_t)scs_ret);

    /* 6. Battery & Environmental State */
    uint32_t gui_mode = *(volatile uint32_t *)0x8D6C;
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [2] ENVIRONMENTAL & HARDWARE TELEMETRY ---\n"
        "Battery Status Word      : 0x%04X (%d bars)\n"
        "Battery Temperature      : %d C\n"
        "Active GUI Mode          : 0x%08X (%d)\n\n",
        (uint32_t)g_batt_mv, (int)g_batt_mv,
        (int)g_batt_temp,
        gui_mode, gui_mode);

    /* 7. Property Database Handles (0x00004748) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [3] SROM FROM PROPERTY HANDLES TABLE (0x00004748) ---\n"
        "IDX  SUBSYSTEM   HANDLE_PTR  STATUS       HEADER (FIRST 16 BYTES)\n");

    const char *subsys_names[13] = {
        "TUNE", "FIX", "LENS", "RING", "RASEN", "CUSTOM",
        "CIG_DATA", "SERVICE_DATA", "ERROR_LOG",
        "DURAN_MAIN", "DURAN_SUB", "LUCKY_TBL", "PICTSTYLE"
    };

    volatile uint32_t *handles = (volatile uint32_t *)0x00004748;
    for (int i = 0; i < 13; i++)
    {
        uint32_t h = handles[i];
        int valid = (h >= 0x1000 && h < 0x40000000);
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "[%02d] %-12s 0x%08X  %-11s ",
            i, subsys_names[i], h, valid ? "ACTIVE" : "UNMAPPED");

        if (valid)
        {
            volatile uint32_t *hp = (volatile uint32_t *)h;
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                "%08X %08X %08X %08X\n", hp[0], hp[1], hp[2], hp[3]);
        }
        else
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "--\n");
        }
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 8. Specific Subsystem Reports: CIG_DATA & SERVICE_DATA */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [4] CIG_DATA (PROP 0x12000004) & SERVICE_DATA (PROP 0x12000005) ---\n"
        "CIG_DATA Address    : 0x%08X (Length: %d bytes, Status: 0x%X)\n",
        cig_addr, (int)cig_len, cig_ret);
    if (cig_addr >= 0x1000 && cig_addr < 0x40000000 && cig_len > 0)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "  Hexdump (first 64 bytes):\n");
        uint32_t dlen = (cig_len > 64) ? 64 : cig_len;
        for (uint32_t b = 0; b < dlen; b += 16)
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "    +0x%02X: ", b);
            for (uint32_t j = 0; j < 16 && b + j < dlen; j++)
            {
                out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "%02X ",
                    *(volatile uint8_t *)(cig_addr + b + j));
            }
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");
        }
    }

    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "SERVICE_DATA Address: 0x%08X (Length: %d bytes, Status: 0x%X)\n",
        srv_addr, (int)srv_len, srv_ret);
    if (srv_addr >= 0x1000 && srv_addr < 0x40000000 && srv_len > 0)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "  Hexdump (first 128 bytes):\n");
        uint32_t dlen = (srv_len > 128) ? 128 : srv_len;
        for (uint32_t b = 0; b < dlen; b += 16)
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "    +0x%02X: ", b);
            for (uint32_t j = 0; j < 16 && b + j < dlen; j++)
            {
                out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "%02X ",
                    *(volatile uint8_t *)(srv_addr + b + j));
            }
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");
        }
    }

    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "ERROR_LOG Address   : 0x%08X (Length: %d bytes, Status: 0x%X)\n\n",
        err_addr, (int)err_len, err_ret);

    /* 9. ROM Memory Descriptors (0xE0D9BE68) Sample */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [5] ROM MASTER MEMORY DESCRIPTORS (0xE0D9BE68) SAMPLE ---\n"
        "IDX  CHUNK_ID    FLAGS       BASE_ADDR   LENGTH (BYTES) LENGTH (KB)\n");

    volatile uint32_t *rom_tbl = (volatile uint32_t *)0xE0D9BE68;
    for (int i = 0; i < 32; i++)
    {
        uint32_t id    = rom_tbl[i * 36 + 0];
        uint32_t flags = rom_tbl[i * 36 + 1];
        uint32_t bptr  = rom_tbl[i * 36 + 2];
        uint32_t blen  = rom_tbl[i * 36 + 6];
        if (bptr != 0 || blen != 0)
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
                "[%02d] 0x%08X  0x%08X  0x%08X  %10d    (%d KB)\n",
                i, id, flags, bptr, (int)blen, (int)(blen / 1024));
        }
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 10. Write OPTION_E_PROPERTY_REPORT.TXT to SD card */
    const char *targets[] = {
        "OPTION_E_PROPERTY_REPORT.TXT",
        "A:/OPTION_E_PROPERTY_REPORT.TXT",
        "B:/OPTION_E_PROPERTY_REPORT.TXT"
    };
    for (int t = 0; t < 3; t++)
    {
        FIO_RemoveFile(targets[t]);
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, out_buf, out_len);
            FIO_CloseFile(f);
            break;
        }
    }

    /* 11. Dump Binary Payloads: PROP_CIG.BIN & PROP_SERVICE.BIN */
    if (cig_addr >= 0x1000 && cig_addr < 0x40000000 && cig_len > 0)
    {
        FILE *fc = open_file_for_writing("PROP_CIG.BIN");
        if (fc)
        {
            FIO_WriteFile(fc, (const void *)cig_addr, cig_len);
            FIO_CloseFile(fc);
        }
    }

    if (srv_addr >= 0x1000 && srv_addr < 0x40000000 && srv_len > 0)
    {
        FILE *fs = open_file_for_writing("PROP_SERVICE.BIN");
        if (fs)
        {
            FIO_WriteFile(fs, (const void *)srv_addr, srv_len);
            FIO_CloseFile(fs);
        }
    }
}

/* =========================================================================
 * OPTION F: AUDIO SUBSYSTEM & HARDWARE CODEC ARCHITECTURE
 * ========================================================================= */

static void dump_option_f_audio(void)
{
    static char out_buf[32768];
    int out_len = 0;

    /* 1. Format Header */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "================================================================================\n"
        "=== CANON EOS R6 FIRMWARE 1.7.0 -- OPTION F: AUDIO SUBSYSTEM & CODEC REPORT  ===\n"
        "================================================================================\n"
        "Architecture : Dual-Core Cortex-A53 | DIGIC X | DryOS Kernel\n"
        "Camera Model : Canon EOS R6 (0x%08X)\n"
        "Sampling     : 48 kHz / 24-bit LPCM (Movie) | 48 kHz / 16-bit (Standard)\n"
        "Body Serial  : %s\n\n",
        (uint32_t)0x80000453, g_body_serial_str);

    /* 2. Decoded DryOS Audio Properties Registry */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [1] REVERSE-ENGINEERED DRYOS AUDIO PROPERTY REGISTRY ---\n"
        "PROP_ID     PROPERTY_NAME                    DESCRIPTION / DISPATCHER\n"
        "0x8003002B  PROP_MIC_PHYSICAL_CONNECT        External 3.5mm Mic Jack Detect (0=Int, 1=Ext)\n"
        "0x80030055  PROP_HEADPHONE_PHYSICAL_CONNECT  3.5mm Headphone Jack Detect (0=Unplugged, 1=Plugged)\n"
        "0x8003002C  PROP_HDMI_PHYSICAL_CONNECT       HDMI Audio Stream Connect State\n"
        "0x80000080  PROP_MOVIE_REC_VOLUME            Movie Audio Recording Gain (0..64 Manual / Auto)\n"
        "0x80000081  PROP_MOVIE_REC_VOLUME_LINE       Line Recording Volume Gain\n"
        "0x80000082  PROP_MOVIE_SOUND_RECORD          Audio Recording Mode (0=Auto, 1=Manual, 2=Disable)\n"
        "0x80000083  PROP_WIND_CUT                    Wind Noise Cut Filter (0=Off, 1=On)\n"
        "0x80000084  PROP_ATTENUATOR                  Microphone Attenuator Toggle (0=Off, 1=On)\n"
        "0x800000CC  PROP_DUAL_OUTPUT                 Dual Audio Routing Path\n"
        "0x80020006  PROP_BUZZER                      Camera Speaker / Buzzer Beep Enable\n\n");

    /* 3. Audio DRAM State Object (0x00017B54) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [2] MASTER AUDIO STATE OBJECT IN DRAM (0x00017B54) ---\n");
    volatile uint32_t *p_audio_ctx = (volatile uint32_t *)0x00017B54;
    uint32_t audio_ctx = 0;
    uint32_t audio_sig = 0;
    if ((uint32_t)p_audio_ctx >= 0x1000 && (uint32_t)p_audio_ctx < 0x40000000)
    {
        audio_ctx = p_audio_ctx[0];
        audio_sig = p_audio_ctx[2]; /* +0x8 */
    }
    g_audio_ctx_addr = audio_ctx;
    g_audio_sig_addr = audio_sig;

    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "Audio Context Pointer [0x00017B54 + 0x00]: 0x%08X\n"
        "Audio Signal Pointer  [0x00017B54 + 0x08]: 0x%08X\n\n",
        audio_ctx, audio_sig);

    if (audio_sig >= 0x1000 && audio_sig < 0x40000000)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "  Audio Signal Parameter Buffer (First 64 Bytes at 0x%08X):\n", audio_sig);
        for (uint32_t b = 0; b < 64; b += 16)
        {
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "    +0x%02X: ", b);
            for (uint32_t j = 0; j < 16; j++)
            {
                out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "%02X ",
                    *(volatile uint8_t *)(audio_sig + b + j));
            }
            out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");
        }
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");
    }

    /* 4. DIGIC X Hardware Audio MMIO Architecture */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [3] HARDWARE AUDIO CONTROLLER & CODEC MMIO MAP ---\n"
        "REGISTER    NAME / FUNCTION                          CLOCK / POWER DOMAIN\n"
        "0xD2710000  AUDIO_CTRL_BASE_0                        Gated by PowerSave (Active in Rec/Play)\n"
        "0xD2720000  AUDIO_CTRL_BASE_1                        Audio DSP Main Interface Controller\n"
        "0xD2720008  AUDIO_STATUS_REG                         Bit 5/6: Serial Ready / Busy Flag\n"
        "0xD272000C  AUDIO_CONFIG_REG                         Sample Clock / Routing Control\n"
        "0xD2721064  AUDIO_CLOCK_DIV_REG                      I2S MCLK / BCLK Prescaler\n"
        "0xD2722000  AUDIO_SERIAL_COMMAND_REG                 Serial Command Payload Transmit\n"
        "0xD2722008  AUDIO_SERIAL_STATUS_REG                  Serial Command Acknowledge Register\n\n");

    /* 5. SoundDev Event Procedures Table (0xE0522600) */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [4] SOUNDDEV EVENT PROCEDURE DISPATCH TABLE (0xE0522600) ---\n"
        "IDX  PROCEDURE_NAME                    NAME_PTR    HANDLER_ADDR\n");
    const char *sounddev_procs[] = {
        "SoundDev_InitializeMine",
        "SoundDev_TerminateMine",
        "SoundDev_RegisterEventProcedure",
        "SoundDev_TranscodeGetBuffSize",
        "SoundDev_TranscodeInit",
        "SoundDev_SetAudioInit",
        "SoundDev_SetMovieStart",
        "SoundDev_SetMovieStop",
        "SoundDev_SetAudioEnd",
        "SoundDev_SetInStart",
        "SoundDev_SetOutStart",
        "SoundDev_RegisterALVObserveCBR",
        "SoundDev_UnregisterALVObserveCBR"
    };
    for (int s = 0; s < 13; s++)
    {
        out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
            "[%02d] %-33s 0x%08X  0x%08X\n",
            s, sounddev_procs[s],
            (uint32_t)(0xE0D033AC + s * 24),
            (uint32_t)(0xE0522600 + s * 8));
    }
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len, "\n");

    /* 6. ROM Audio Gain Matrices & Preamp Descriptors */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [5] ROM AUDIO GAIN & PREAMP CALIBRATION TABLES ---\n"
        "MICGAIN Table Base       : 0xE0402480\n"
        "MICGAIN Left Channel Adj : 0xE040249C\n"
        "MICGAIN Right Channel Adj: 0xE04024BC\n"
        "DCCUT DC-Removal Filter  : 0xE0402500 (Coef: 0xE0402510)\n"
        "Speaker Gain Table       : 0xE0402954\n"
        "Headphone Gain Table     : 0xE0402964\n"
        "Beep Output Gain Table   : 0xE0402940\n"
        "VAIF Analog Preamp Reg   : 0xE04029B8 (Gain: 20.5 dB / 26.0 dB Front-End)\n"
        "SD Calibration File Hook : AUDSig.cfg (0xE0403FBC on A:/ or B:/)\n\n");

    /* 7. Audio Codec Control & Serial Interface */
    out_len += snprintf(out_buf + out_len, sizeof(out_buf) - out_len,
        "--- [6] AUDIO CODEC SERIAL CONTROL & DMA ENGINE ---\n"
        "Serial Command Routine   : AudioICSerialCommand (0xE0404720)\n"
        "Serial Send Routine      : AudioICSerialCommand_Send (0xE1048061)\n"
        "Serial Wait / Test       : AudioICWaitTest (0xE0404738)\n"
        "actrl State Machine Tbl  : 0xE1048090 (Handlers: 0xE03FCA99, 0xE03FF755, 0xE03FCCE8, 0xE03FD1C5)\n"
        "DMA Architecture         : ARM PrimeCell PL330 DMA Engine (DMA330_0..DMA330_3)\n"
        "EDMAC Streaming Domain   : EDOMAIN_EDMAC Audio FIFO Read/Write Channels\n\n");

    /* 8. Write OPTION_F_AUDIO_REPORT.TXT to SD card */
    const char *targets[] = {
        "OPTION_F_AUDIO_REPORT.TXT",
        "A:/OPTION_F_AUDIO_REPORT.TXT",
        "B:/OPTION_F_AUDIO_REPORT.TXT"
    };
    for (int t = 0; t < 3; t++)
    {
        FIO_RemoveFile(targets[t]);
        FILE *f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, out_buf, out_len);
            FIO_CloseFile(f);
            break;
        }
    }
}

/* =========================================================================
 * OPTION B: BIT-EXACT 32-BIT BMP SCREENSHOT DUMPER
 * ========================================================================= */

static void make_bmp_header_32bpp(uint8_t hdr[54], int32_t width, int32_t height)
{
    memset(hdr, 0, 54);
    uint32_t image_size = (uint32_t)(width * height * 4);
    uint32_t file_size = 54 + image_size;
    int32_t neg_height = -height; /* Negative height specifies Top-Down DIB (row 0 at top) */

    /* BITMAPFILEHEADER (14 bytes) */
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = (uint8_t)(file_size & 0xFF);
    hdr[3] = (uint8_t)((file_size >> 8) & 0xFF);
    hdr[4] = (uint8_t)((file_size >> 16) & 0xFF);
    hdr[5] = (uint8_t)((file_size >> 24) & 0xFF);
    hdr[10] = 54; /* Pixel array starts at offset 54 */

    /* BITMAPINFOHEADER (40 bytes) */
    hdr[14] = 40; /* Header size */
    hdr[18] = (uint8_t)(width & 0xFF);
    hdr[19] = (uint8_t)((width >> 8) & 0xFF);
    hdr[20] = (uint8_t)((width >> 16) & 0xFF);
    hdr[21] = (uint8_t)((width >> 24) & 0xFF);
    hdr[22] = (uint8_t)(neg_height & 0xFF);
    hdr[23] = (uint8_t)((neg_height >> 8) & 0xFF);
    hdr[24] = (uint8_t)((neg_height >> 16) & 0xFF);
    hdr[25] = (uint8_t)((neg_height >> 24) & 0xFF);
    hdr[26] = 1;  /* Planes = 1 */
    hdr[28] = 32; /* 32 bits per pixel (BGRA little-endian) */
    /* Compression = BI_RGB (0) at hdr[30..33] */
    hdr[34] = (uint8_t)(image_size & 0xFF);
    hdr[35] = (uint8_t)((image_size >> 8) & 0xFF);
    hdr[36] = (uint8_t)((image_size >> 16) & 0xFF);
    hdr[37] = (uint8_t)((image_size >> 24) & 0xFF);
    /* 2835 pixels/meter (~72 DPI) */
    hdr[38] = 0x13; hdr[39] = 0x0B;
    hdr[42] = 0x13; hdr[43] = 0x0B;
}

static inline uint32_t blend_argb(uint32_t p0, uint32_t p1)
{
    uint32_t a = (p0 >> 24) & 0xFF;
    if (a == 255)
    {
        return p0;
    }
    if (a == 0)
    {
        return p1 | 0xFF000000;
    }
    uint32_t b0 = p0 & 0xFF;
    uint32_t g0 = (p0 >> 8) & 0xFF;
    uint32_t r0 = (p0 >> 16) & 0xFF;

    uint32_t b1 = p1 & 0xFF;
    uint32_t g1 = (p1 >> 8) & 0xFF;
    uint32_t r1 = (p1 >> 16) & 0xFF;

    uint32_t inv_a = 255 - a;
    uint32_t b = (b0 * a + b1 * inv_a) / 255;
    uint32_t g = (g0 * a + g1 * inv_a) / 255;
    uint32_t r = (r0 * a + r1 * inv_a) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

static FILE *open_file_for_writing(const char *basename)
{
    char path[64];
    FIO_RemoveFile(basename);
    FILE *f = FIO_CreateFile(basename);
    if (f) return f;

    snprintf(path, sizeof(path), "A:/%s", basename);
    FIO_RemoveFile(path);
    f = FIO_CreateFile(path);
    if (f) return f;

    snprintf(path, sizeof(path), "B:/%s", basename);
    FIO_RemoveFile(path);
    f = FIO_CreateFile(path);
    return f;
}

#define CHUNK_ROWS 16
#define CHUNK_BUF_SIZE (CHUNK_ROWS * 960 * sizeof(uint32_t)) /* 61,440 bytes */
static uint8_t chunk_buf[CHUNK_BUF_SIZE];

/* mode: 0 = Composite (L1 + L0), 1 = L0 only (UI), 2 = L1 only (LiveView Video) */
static int save_screenshot_960x540(const char *filename, uint32_t *vram0, uint32_t stride0,
                                   uint32_t *vram1, uint32_t stride1, int mode)
{
    FILE *f = open_file_for_writing(filename);
    if (!f) return -1;

    uint8_t hdr[54];
    make_bmp_header_32bpp(hdr, 960, 540);
    FIO_WriteFile(f, hdr, 54);

    for (int y = 0; y < 540; y += CHUNK_ROWS)
    {
        int rows_to_write = (y + CHUNK_ROWS <= 540) ? CHUNK_ROWS : (540 - y);
        for (int r = 0; r < rows_to_write; r++)
        {
            int cy = y + r;
            uint32_t *dest = (uint32_t *)&chunk_buf[r * 960 * sizeof(uint32_t)];
            uint32_t *p0 = vram0 ? &vram0[cy * stride0] : NULL;
            uint32_t *p1 = vram1 ? &vram1[cy * stride1] : NULL;

            for (int x = 0; x < 960; x++)
            {
                uint32_t px0 = p0 ? p0[x] : 0;
                uint32_t px1 = p1 ? p1[x] : 0xFF000000;

                if (mode == 0)
                {
                    dest[x] = blend_argb(px0, px1);
                }
                else if (mode == 1)
                {
                    dest[x] = px0;
                }
                else
                {
                    dest[x] = px1 | 0xFF000000;
                }
            }
        }
        FIO_WriteFile(f, chunk_buf, rows_to_write * 960 * sizeof(uint32_t));
    }

    FIO_CloseFile(f);
    return 0;
}

static int save_raw_canvas_2048x1080(const char *filename, uint32_t *vram)
{
    if (!vram) return -1;
    FILE *f = open_file_for_writing(filename);
    if (!f) return -1;

    uint8_t hdr[54];
    make_bmp_header_32bpp(hdr, 2048, 1080);
    FIO_WriteFile(f, hdr, 54);

    /* Stream directly from DRAM in 512 KB chunks (64 rows of 2048 words = 524,288 bytes) */
    const uint32_t chunk_bytes = 64 * 2048 * sizeof(uint32_t);
    const uint32_t total_bytes = 1080 * 2048 * sizeof(uint32_t);
    uint8_t *src = (uint8_t *)vram;
    uint32_t written = 0;

    while (written < total_bytes)
    {
        uint32_t to_write = total_bytes - written;
        if (to_write > chunk_bytes) to_write = chunk_bytes;
        FIO_WriteFile(f, src + written, to_write);
        written += to_write;
    }

    FIO_CloseFile(f);
    return 0;
}

static void capture_screenshot_set(int is_boot, int counter)
{
    MEM(CARD_LED_ADDRESS) = LEDON;

    struct MARV *marv0 = *(struct MARV **)0x14B000;
    if (!marv0 || marv0->signature != 0x5652414D) marv0 = *(struct MARV **)0x18384;
    struct MARV *marv1 = *(struct MARV **)0x14B00C;
    if (marv1 && marv1->signature != 0x5652414D) marv1 = NULL;

    uint32_t *vram0 = (marv0 && marv0->signature == 0x5652414D) ? (uint32_t *)marv0->bitmap_data : NULL;
    uint32_t stride0 = (marv0 && marv0->width >= 1024) ? marv0->width : 2048;

    uint32_t *vram1 = (marv1 && marv1->signature == 0x5652414D) ? (uint32_t *)marv1->bitmap_data : NULL;
    uint32_t stride1 = (marv1 && marv1->width >= 1024) ? marv1->width : 2048;

    char fname[64];
    volatile uint32_t *raw_desc = (volatile uint32_t *)0x0001EF88;
    uint32_t raw_buf = raw_desc[20];
    if (!raw_buf || raw_buf < 0x00100000)
    {
        raw_buf = raw_desc[22]; /* Secondary buffer offset */
    }
    uint32_t raw_pitch  = raw_desc[7] ? raw_desc[7] : 1888;
    uint32_t raw_height = (raw_desc[6] > 0) ? raw_desc[6] : (raw_desc[10] > 0 ? raw_desc[10] : 1220);
    uint32_t raw_sz     = raw_pitch * raw_height; /* 1888 * 1220 = 2,303,360 bytes */

    if (is_boot)
    {
        /* 1. Boot Baseline LCD Screen (Composite) */
        save_screenshot_960x540("BOOT_LCD.BMP", vram0, stride0, vram1, stride1, 0);

        /* 2. Boot LiveView Sensor Video Feed */
        if (vram1)
        {
            save_screenshot_960x540("BOOT_LV.BMP", vram0, stride0, vram1, stride1, 2);
        }

        /* 3. Boot GUI Overlay & Raw Canvas */
        if (vram0)
        {
            save_screenshot_960x540("BOOT_UI.BMP", vram0, stride0, vram1, stride1, 1);
            save_raw_canvas_2048x1080("BOOT_RAW.BMP", vram0);
        }

        /* 4. Boot Bayer 14-Bit Sensor RAW Frame */
        if (raw_buf >= 0x00100000 && raw_buf < 0x80000000)
        {
            save_raw_sensor_buffer("BOOT_SENSOR.RAW", raw_buf, raw_sz);
        }
    }
    else
    {
        /* On-Demand Button Trigger */
        snprintf(fname, sizeof(fname), "SCRN%04d.BMP", counter);
        save_screenshot_960x540(fname, vram0, stride0, vram1, stride1, 0);

        if (vram1)
        {
            snprintf(fname, sizeof(fname), "LV%04d.BMP", counter);
            save_screenshot_960x540(fname, vram0, stride0, vram1, stride1, 2);
        }

        if (vram0)
        {
            snprintf(fname, sizeof(fname), "RAW%04d.BMP", counter);
            save_raw_canvas_2048x1080(fname, vram0);
        }

        if (raw_buf >= 0x00100000 && raw_buf < 0x80000000)
        {
            snprintf(fname, sizeof(fname), "SENS%04d.RAW", counter);
            save_raw_sensor_buffer(fname, raw_buf, raw_sz);
        }
    }

    MEM(CARD_LED_ADDRESS) = LEDOFF;

    /* Confirmation Double Blink */
    for (int b = 0; b < 2; b++)
    {
        msleep(40);
        MEM(CARD_LED_ADDRESS) = LEDON;
        msleep(40);
        MEM(CARD_LED_ADDRESS) = LEDOFF;
    }
}

static void hello_world(void *arg)
{
    /* Immediate startup confirmation: 1 quick flash */
    MEM(CARD_LED_ADDRESS) = LEDON;
    for (volatile int i = 0; i < 1500000; i++);
    MEM(CARD_LED_ADDRESS) = LEDOFF;

    /* 1-second startup wait */
    msleep(1000);

    /* 3 quick blinks: ready confirmation */
    for (int b = 0; b < 3; b++)
    {
        MEM(CARD_LED_ADDRESS) = LEDON;  msleep(60);
        MEM(CARD_LED_ADDRESS) = LEDOFF; msleep(60);
    }

    /* STEP 2: obtain Layer 0 MARV surface from Canon's MARV table */
    struct MARV *marv = NULL;
    for (int retry = 0; retry < 30; retry++)
    {
        marv = *(struct MARV **)0x14B000;
        if (!marv || marv->signature != 0x5652414D)
        {
            marv = *(struct MARV **)0x18384;
        }
        if (marv && marv->signature == 0x5652414D && marv->bitmap_data)
        {
            break;
        }
        msleep(50);
    }

    if (!marv || marv->signature != 0x5652414D)
    {
        led_pattern_forever(1, 100, 100, 500);
    }

    if (!marv->bitmap_data)
    {
        led_pattern_forever(2, 100, 100, 500);
    }

    /* Dump MARV structure directly to MARVDUMP.TXT on SD card */
    dump_marv_to_file();
    dump_sw_table_to_file();
    dump_option_a_diagnostics();
    dump_option_d_raw_diagnostics();
    dump_option_e_properties();
    dump_option_f_audio();

    /* Safe display area centered on 960x540 display: x=180..820, y=80..470 */
    uint32_t box_w   = 640;
    uint32_t box_h   = 390;
    uint32_t x_start = 180;
    uint32_t y_start = 80;

    /* Read initial ring index safely */
    uint32_t last_seen_index = *(volatile uint32_t *)RING_INDEX_ADDR;

    /* Reset event log */
    memset(key_history, 0, sizeof(key_history));
    total_events_logged = 0;

    /* Continuous live loop: runs at ~33 fps indefinitely */
    static int s_boot_sensor_saved = 0;
    for (int cycle = 0; ; cycle++)
    {
        if (cycle == 30 || cycle == 90)
        {
            dump_marv_to_file();
        }

        /* Periodic raw latch re-assertion and keylog flush */
        if (cycle % 30 == 0)
        {
            volatile uint32_t *rdesc = (volatile uint32_t *)0x0001EF88;
            volatile uint32_t *dctrl = (volatile uint32_t *)0x00030278;
            dctrl[1] = 1;  /* [0x0003027C] = 1 */
            rdesc[11] = 1; /* [0x0001EFB4] = 1 */
            flush_keylog_to_file();
        }

        /* Dynamic Bayer Raw Frame Latch Check */
        if (!s_boot_sensor_saved && cycle >= 15)
        {
            volatile uint32_t *rdesc = (volatile uint32_t *)0x0001EF88;
            uint32_t live_raw = rdesc[20] ? rdesc[20] : rdesc[22];
            if (live_raw >= 0x00100000 && live_raw < 0x80000000)
            {
                s_boot_sensor_saved = 1;
                uint32_t p = rdesc[7] ? rdesc[7] : 1888;
                uint32_t h = (rdesc[6] > 0) ? rdesc[6] : (rdesc[10] > 0 ? rdesc[10] : 1220);
                uint32_t sz = p * h;
                save_raw_sensor_buffer("BOOT_SENSOR.RAW", live_raw, sz);
                dump_option_d_raw_diagnostics();
                snprintf(screenshot_toast_msg, sizeof(screenshot_toast_msg),
                         "LATCHED RAW: BOOT_SENSOR.RAW (%d KB)", (int)(sz / 1024));
                screenshot_toast_frames = 150;
            }
        }

        if (cycle == 35)
        {
            dump_option_e_properties();
            dump_option_f_audio();
            snprintf(screenshot_toast_msg, sizeof(screenshot_toast_msg),
                     "OPTION F: AUD 48kHz | SHT M:%d E:%d",
                     (int)g_shutter_mech, (int)g_shutter_elec);
            screenshot_toast_frames = 180;
        }

        if (cycle == 45)
        {
            capture_screenshot_set(1, 0);
            dump_option_d_raw_diagnostics();
            dump_option_e_properties();
            dump_option_f_audio();
            snprintf(screenshot_toast_msg, sizeof(screenshot_toast_msg),
                     "BOOT CAPTURED: LCD+UI+LV%s",
                     s_boot_sensor_saved ? "+RAW!" : " (RAW PENDING)");
            screenshot_toast_frames = 150;
        }

        /* Check Layer 0 and Layer 1 MARVs */
        struct MARV *marv0 = *(struct MARV **)0x14B000;
        if (!marv0 || marv0->signature != 0x5652414D) marv0 = *(struct MARV **)0x18384;

        struct MARV *marv1 = *(struct MARV **)0x14B00C;
        if (marv1 && marv1->signature != 0x5652414D) marv1 = NULL;

        uint32_t *vram0 = (marv0 && marv0->signature == 0x5652414D) ? (uint32_t *)marv0->bitmap_data : NULL;
        uint32_t stride0 = (marv0 && marv0->width >= 1024) ? marv0->width : 2048;

        uint32_t *vram1 = (marv1 && marv1->signature == 0x5652414D) ? (uint32_t *)marv1->bitmap_data : NULL;
        uint32_t stride1 = (marv1 && marv1->width >= 1024) ? marv1->width : 2048;

        uint32_t gui_mode = *(volatile uint32_t *)0x8D6C;

        /* Solid, continuous HUD drawing: renders every cycle (~33 fps)
         * Draws primarily on Layer 0 UI surface, preserving Layer 1 LiveView
         */
        if (vram0)
        {
            draw_key_hud(vram0, stride0, box_w, box_h, x_start, y_start, gui_mode, cycle);
        }
        else if (vram1)
        {
            draw_key_hud(vram1, stride1, box_w, box_h, x_start, y_start, gui_mode, cycle);
        }

        /* Fast button, dial & joystick poll: 2 slices of 15ms = 30ms total cycle (~33 fps) */
        for (int slice = 0; slice < 2; slice++)
        {
            uint32_t cur_index = *(volatile uint32_t *)RING_INDEX_ADDR;
            if (cur_index <= 1024)
            {
                uint32_t dist = (cur_index - last_seen_index + 1025) % 1025;
                if (dist > 50)
                {
                    last_seen_index = (cur_index - 5 + 1025) % 1025;
                }

                while (last_seen_index != cur_index)
                {
                    uint32_t slot = last_seen_index;
                    last_seen_index = (last_seen_index + 1) % 1025;

                    uint32_t *slot_ptr = (uint32_t *)(RING_BUFFER_BASE + slot * 16);
                    uint32_t ev_type  = slot_ptr[0];
                    uint32_t ev_param = slot_ptr[1];
                    uint32_t ev_obj   = slot_ptr[2];
                    uint32_t ev_arg   = slot_ptr[3];

                    /* Ignore empty / uninitialized slots */
                    if (ev_param == 0 && ev_arg == 0)
                    {
                        continue;
                    }

                    /* Filter out periodic mode/power polling noise (0x44, 0x45) so history remains clean */
                    if (ev_param == 0x44 || ev_param == 0x45)
                    {
                        continue;
                    }

                    /* Ignore display refresh messages (0xB7) */
                    if (ev_param == 0xB7)
                    {
                        continue;
                    }

                    /* Log this event into history! */
                    total_events_logged++;

                    for (int h = KEY_HISTORY_MAX - 1; h > 0; h--)
                    {
                        key_history[h] = key_history[h - 1];
                    }

                    key_history[0].slot     = slot;
                    key_history[0].ev_type  = ev_type;
                    key_history[0].ev_param = ev_param;
                    key_history[0].ev_arg   = ev_arg;
                    key_history[0].ev_obj   = ev_obj;
                    key_history[0].seq      = total_events_logged;

                    /* Append to persistent SD card key log */
                    if (key_file_log_count < KEY_FILE_LOG_MAX)
                    {
                        key_file_log[key_file_log_count].ev_param = ev_param;
                        key_file_log[key_file_log_count].ev_arg   = ev_arg;
                        key_file_log[key_file_log_count].ev_obj   = ev_obj;
                        key_file_log_count++;
                    }

                    /* Option B, D, E & F: Screenshot, Raw, Property & Audio Refresh (RATE: 0x33, MAGNIFY: 0x1C, M-Fn: 0x22, TRASH: 0x14, KEY: 0x32) */
                    if ((ev_param == 0x33 || ev_param == 0x1C || ev_param == 0x22 || ev_param == 0x14 || ev_param == 0x32) && ev_arg != 0)
                    {
                        if (cycle - last_screenshot_cycle >= 10) /* Fast ~300ms debounce */
                        {
                            last_screenshot_cycle = cycle;
                            dump_option_e_properties();
                            dump_option_f_audio();
                            capture_screenshot_set(0, screenshot_counter);
                            snprintf(screenshot_toast_msg, sizeof(screenshot_toast_msg),
                                     "F: AUD+SHT M:%d E:%d (SCRN%04d)",
                                     (int)g_shutter_mech, (int)g_shutter_elec, screenshot_counter);
                            screenshot_toast_frames = 150;
                            screenshot_counter++;
                        }
                    }
                }
            }
            msleep(15);
        }

        /* Calm 1 Hz Heartbeat LED */
        if (cycle % 30 == 0) MEM(CARD_LED_ADDRESS) = LEDON;
        else if (cycle % 30 == 3) MEM(CARD_LED_ADDRESS) = LEDOFF;
    }
}


#endif

#ifdef CONFIG_DUMPER_BOOTFLAG
static void dumper_bootflag()
{
    msleep(5000);
    SetGUIRequestMode(GUIMODE_PLAY);
    msleep(1000);
    bmp_fill(COLOR_BLACK, 0, 0, 720, 480);
    bmp_printf(FONT_LARGE, 50, 100, "Please wait...");
    msleep(2000);

    if (CURRENT_GUI_MODE != GUIMODE_PLAY)
    {
        bmp_printf(FONT_LARGE, 50, 150, "Hudson, we have a problem!");
        return;
    }
    
    /* this requires CONFIG_AUTOBACKUP_ROM */
    bmp_printf(FONT_LARGE, 50, 150, "ROM Backup...");
    backup_rom_task();

    // do not try to enable bootflag in LiveView, or during sensor cleaning (it will fail while writing to ROM)
    // no check is done here, other than a large delay and doing this while in Canon menu
    // todo: check whether the issue is still present with interrupts disabled
    bmp_printf(FONT_LARGE, 50, 200, "EnableBootDisk...");
    uint32_t old = cli();
    call("EnableBootDisk");
    sei(old);

    bmp_printf(FONT_LARGE, 50, 250, ":)");
}
#endif

static void led_fade(int arg1, void * on)
{
    /* emulate a fade-out PWM using a HPTimer */
    static int k = 16000;
    if (k > 0)
    {
        if (on)
            _card_led_on();
        else
            _card_led_off();
        int next_delay = (on ? k : 16000 - k);   /* cycle: 16000 us => 62.5 Hz */
        SetHPTimerNextTick(arg1, next_delay, led_fade, led_fade, (void *) !on);
        k -= MAX(16, k/32);  /* adjust fading speed and shape here */
    }
}

/* This runs ML initialization routines and starts user tasks.
 * Unlike init_task, from here we can do file I/O and others.
 */
void dbg_blink(int n)
{
#if defined(CARD_LED_ADDRESS) && defined(LEDON) && defined(LEDOFF)
    for (int k = 0; k < n; k++)
    {
        *(volatile uint32_t *)CARD_LED_ADDRESS = LEDON;
        for (volatile int j = 0; j < 12500000; j++);
        *(volatile uint32_t *)CARD_LED_ADDRESS = LEDOFF;
        for (volatile int j = 0; j < 12500000; j++);
        msleep(10);
    }
    msleep(20);
    for (volatile int j = 0; j < 30000000; j++);
#endif
}

static void my_big_init_task()
{
    _mem_init();
    _find_ml_card();

    extern int _set_at_startup;
    _set_at_startup = config_flag_file_setting_load("ML/SETTINGS/REQUIRE.SET");

    dbg_blink(1); // Milestone 1: Memory, card, and startup config initialized

    // at this point, gui_main_task should be started and should be able to tell whether SET was pressed at startup
    if (magic_off_request != _set_at_startup)
    {
        /* should we bypass loading ML? */
        /* (pressing SET after this point will be ignored) */
        magic_off = 1;

    #if defined(CONFIG_ADDITIONAL_VERSION)
        /* fixme: enable on all models */
        extern char additional_version[];
        additional_version[0] = '-';
        additional_version[1] = 'm';
        additional_version[2] = 'l';
        additional_version[3] = '-';
        additional_version[4] = 'o';
        additional_version[5] = 'f';
        additional_version[6] = 'f';
        additional_version[7] = '\0';
    #endif

        /* some very basic feedback - fade out the SD led */
        SetHPTimerAfterNow(1000, led_fade, led_fade, 0);

        dbg_blink(2);  /* magic_off bypassed ML load */
        /* do not continue loading ML */
        return;
    }

#if defined(CONFIG_MMU_REMAP) && defined(CONFIG_RPC)
    int err = apply_normal_patches();
    if (err < 0)
        qprintf("Error from apply_normal_patches: %d\n", err);
#endif

    _load_fonts();

    // SJE not sure on best place to do this.  Before HELLO_WORLD is nice
    // if possible, needs to be after DryOS inits task scheduler.  I assume
    // that has happened but don't know how to check.
    //
    // DryOS keeps task structs in an array, the first task is created
    // very early and never removed (the "idle" task).  TaskId is index
    // into the array, so we can find first item knowing any other.
    first_task = current_task->self - ((current_task->taskId & 0xffff) - 1);

#ifdef CONFIG_HELLO_WORLD
    hello_world(0);
    return;
#endif

#ifdef CONFIG_DUMPER_BOOTFLAG
    dumper_bootflag();
    return;
#endif

#ifdef CONFIG_XF605
    uart_printf("hello from ML, before early tasks");
#endif

    call("DisablePowerSave");
    _ml_cbr_init();
    menu_init();
    debug_init();
    dbg_blink(2); // Milestone 2: Menus & GUI subsystem ready

    call_init_funcs(); // Milestone 3 (func 12) and Milestone 4 (func 24) are inside
    msleep(200); // leave some time for property handlers to run

#ifdef CONFIG_XF605
    uart_printf("hello from ML, after early tasks");
#endif

    #if defined(CONFIG_AUTOBACKUP_ROM) && !defined(CONFIG_QEMU)
    /* backup ROM first time to be prepared if anything goes wrong. choose low prio */
    /* On 5D3, this needs to run after init functions (after card tests) */
    task_create("ml_backup", 0x1f, 0x4000, backup_rom_task, 0 );
    #endif

    /* Read ML config. if feature disabled, nothing happens */
    config_load();
    
    debug_init_stuff();

    #ifdef FEATURE_GPS_TWEAKS
    gps_tweaks_startup_hook();
    #endif

    _hold_your_horses = 0; // config read, other overriden tasks may start doing their job
    dbg_blink(5); // Milestone 5: Config loaded and locks released

    // Create all of our auto-create tasks, defined via TASK_CREATE()
    extern struct task_create _tasks_start[];
    extern struct task_create _tasks_end[];
    struct task_create * task = _tasks_start;

    int ml_tasks = 0;
    for( ; task < _tasks_end ; task++ )
    {
#if defined(POSITION_INDEPENDENT)
        task->name = PIC_RESOLVE(task->name);
        task->entry = PIC_RESOLVE(task->entry);
        task->arg = PIC_RESOLVE(task->arg);
#endif
        task_create(
            task->name,
            task->priority,
            task->stack_size,
            task->entry,
            task->arg
        );
        ml_tasks++;
    }
    
    msleep(500);
#ifdef CONFIG_XF605
    uart_printf("hello from ML, after late tasks");
#endif

    ml_started = 1;
    dbg_blink(6); // Milestone 6: All tasks started, ML is fully LIVE!
}

/** Blocks execution until config is read */
void hold_your_horses()
{
    while (_hold_your_horses)
    {
        msleep( 100 );
    }
}

/**
 * Custom assert handler - intercept ERR70 and try to save a crash log.
 * Crash log should contain Canon error message.
 */
static char assert_msg[512] = "";
static int (*old_assert_handler)(char*,char*,int,int) = 0;
const char* get_assert_msg() { return assert_msg; }

static int my_assert_handler(char* msg, char* file, int line, int arg4)
{
    if (msg == NULL)
        msg = "nullptr";

    uint32_t lr = read_lr();

#ifdef CONFIG_DIGIC_678X
    // compiler warning on unused len
    snprintf(assert_msg, sizeof(assert_msg),
#else
    int len = snprintf(assert_msg, sizeof(assert_msg), 
#endif
        "ASSERT: %s\n"
        "at %s:%d, %s:%x\n"
        "lv:%d mode:%d\n\n", 
        msg, 
        file, line, get_current_task_name(), lr,
        lv, shooting_mode
    );
// SJE FIXME: assert handling is buggy on modern Digic.
// Disable some of it here and do quick hack output:
#ifdef CONFIG_DIGIC_678X
    uart_printf("[SJE] my_assert_msg: %s", assert_msg);
#else
    backtrace_getstr(assert_msg + len, sizeof(assert_msg) - len);
#endif
    request_crash_log(1);
    return old_assert_handler(msg, file, line, arg4);
}

void ml_assert_handler(char* msg, char* file, int line, const char* func)
{
    extern int current_init_func_idx;
    extern const char * current_init_func_name;

    int file_code = 6;
    if (file) {
        if (strstr(file, "lens")) file_code = 1;
        else if (strstr(file, "shoot")) file_code = 2;
        else if (strstr(file, "property")) file_code = 3;
        else if (strstr(file, "menu")) file_code = 4;
        else if (strstr(file, "fps") || strstr(file, "crop")) file_code = 5;
        else if (strstr(file, "movtweak") || strstr(file, "cbr")) file_code = 6;
    }

    int func_group = (current_init_func_idx > 0 && current_init_func_idx <= 24) ? ((current_init_func_idx - 1) / 6 + 1) : 6;
    int func_index = (current_init_func_idx > 0 && current_init_func_idx <= 24) ? ((current_init_func_idx - 1) % 6 + 1) : 6;

    int len = snprintf(assert_msg, sizeof(assert_msg), 
        "ML ASSERT:\n%s\n"
        "at %s:%d (%s), task %s\n"
        "init_func: %d (%s)\n"
        "lv:%d mode:%d\n\n", 
        msg ? msg : "none", 
        file ? file : "unknown", line, func ? func : "unknown", get_current_task_name(), 
        current_init_func_idx, current_init_func_name ? current_init_func_name : "none",
        lv, shooting_mode
    );

    // 1. Try saving to SD card across all drive conventions
    const char *targets[] = {
        "ASSERT.TXT",
        "A:/ASSERT.TXT",
        "B:/ASSERT.TXT"
    };
    for (int t = 0; t < 3; t++)
    {
        FIO_RemoveFile(targets[t]);
        FILE* f = FIO_CreateFile(targets[t]);
        if (f)
        {
            FIO_WriteFile(f, assert_msg, len);
            FIO_CloseFile(f);
            break;
        }
    }


    // 3. Diagnostic loop: 6 blinks (Assert) -> File (1-6) -> Group (1-4) -> Index (1-6)
    while (1)
    {
        dbg_blink(6);
        msleep(400);
        dbg_blink(file_code);
        msleep(400);
        dbg_blink(func_group);
        msleep(400);
        dbg_blink(func_index);
        msleep(1500);
    }
}

void ml_crash_message(char* msg)
{
    snprintf(assert_msg, sizeof(assert_msg), "%s", msg);
    request_crash_log(1);
}

#ifdef CONFIG_RPC
uint32_t is_cpu1_ready = 0;
static void cpu1_ready(void)
{
    is_cpu1_ready = 1;
}
#endif

/* called before Canon's init_task */
void boot_pre_init_task()
{
#if defined(CONFIG_HELLO_WORLD) || defined(CONFIG_DUMPER_BOOTFLAG)
    // don't hook
#else
    // normally, we create sems via INIT_FUNC macro,
    // but that happens via tasks, which is later than we need
    // for early MMU remapping.
    #if defined(CONFIG_RPC)
    RPC_sem = create_named_semaphore("RPC", SEM_CREATE_UNLOCKED);
    #endif
    #if defined(CONFIG_MMU_REMAP)
    if (mmu_init() < 0)
        DryosDebugMsg(0, 15, "ERROR doing mmu_init()");
    #endif
    #ifndef CONFIG_NO_TASK_HOOK
    // Install our task creation hooks
    qprint("[BOOT] installing task dispatch hook at "); qprintn((int)&task_dispatch_hook); qprint("\n");
    DryosDebugMsg(0, 15, "replacing task_dispatch_hook");
    task_dispatch_hook = my_task_dispatch_hook;
    #ifdef CONFIG_TSKMON
    tskmon_init();
    #endif
    #endif
#endif
}

/* called right after Canon's init_task, while their initialization continues in background */
void boot_post_init_task(void)
{
#ifdef CONFIG_HELLO_WORLD
    task_create("ml_heartbeat", 0x1e, 0x4000, (void*)hello_world, 0);
    return;
#endif

#if defined(CONFIG_PLATFORM_POST_INIT)
    platform_post_init();
#endif
#if defined(CONFIG_CRASH_LOG)
    // decompile TH_assert to find out the location
    old_assert_handler = (void*)MEM(DRYOS_ASSERT_HANDLER);
    *(void**)(DRYOS_ASSERT_HANDLER) = (void*)my_assert_handler;
#endif // (CONFIG_CRASH_LOG)

    DebugMsg( DM_MAGIC, 3, "Magic Lantern %s (%s)",
        build_version,
        build_id
    );

    DebugMsg( DM_MAGIC, 3, "Built on %s by %s",
        build_date,
        build_user
    );

// kitor: on D678 this gets executed before value
//        is updated with running fw version
#if defined(CONFIG_ADDITIONAL_VERSION)
    // Re-write the version string.
    // Don't use strcpy() so that this can be done
    // before strcpy() or memcpy() are located.
    extern char additional_version[];
    additional_version[0] = '-';
    additional_version[1] = 'm';
    additional_version[2] = 'l';
    additional_version[3] = '-';
    additional_version[4] = build_version[0];
    additional_version[5] = build_version[1];
    additional_version[6] = build_version[2];
    additional_version[7] = build_version[3];
    additional_version[8] = build_version[4];
    additional_version[9] = build_version[5];
    additional_version[10] = build_version[6];
    additional_version[11] = build_version[7];
    additional_version[12] = build_version[8];
    additional_version[13] = '\0';
#endif

#ifdef CONFIG_RPC
    // Get cpu1 to flag when it's fully active,
    // this proves SGI handlers are usable, so we can use request_RPC()
    task_create_ex(NULL, 0x10, 0x200, cpu1_ready, NULL, 1);
#endif

    #ifdef FEATURE_VRAM_RGBA
    while (!rgb_vram_preinit())
        msleep(100);
    #endif

    // wait for firmware to initialize
    while (!bmp_vram_raw())
        msleep(100);
    
    // wait for overriden gui_main_task (but use a timeout so it doesn't break if you disable that for debugging)
    for (int i = 0; i < 50; i++)
    {
        if (ml_gui_initialized)
            break;
        msleep(50);
    }
    msleep(50);

    task_create("ml_init", 0x1e, 0x4000, my_big_init_task, 0 );

    return;
}

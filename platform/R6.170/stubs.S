/** \file
 * Entry points into the firmware image.
 *
 * These are the functions that we can call from our tasks
 * in the Canon 1.5.0 firmware for the EOS R6
 *
 */
/*
 * Copyright (C) 2021 Magic Lantern Team
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

#include <stub.h>

/* All functions are Thumb. */


/** Startup **/
NSTUB(MAIN_FIRMWARE_ADDR,  firmware_entry)
THUMB_FN(0xE01000ec,  cstart)                               /* calls bzero32 and create_init_task(..., init_task, ...) */
THUMB_FN(0xE07E27B0,  bzero32)                              /* zeros out a data structure */
THUMB_FN(0xE0276AA4,  create_init_task)                     /* low-level DryOS initialization */
THUMB_FN(0xE07DC4A8,  dcache_clean)                         /* loop with MCR p15 c7,c10,1; DSB */
THUMB_FN(0xE07DC57C,  icache_invalidate)                    /* loop with MCR p15 c7,c5,1; c7,c1,6; c7,c1,0; ISB */
THUMB_FN(0xE010021e,  init_task)                            /* USER_MEM size checking, dmSetup, termDriverInit, stdlibSetup etc */
THUMB_FN(0xE0005B19,  fixup_boot_memory)                    /* CamIF reinitialization for Digic X */

/** Tasks **/
DATA_PTR(    0x413C,  task_dispatch_hook)                   /* DryOS task dispatcher hook */
THUMB_FN(0xE011E3E8,  task_trampoline)                      /* DryOS task trampoline (entry point runner) */
DATA_PTR(    0x1010,  current_interrupt)                    /* from interrupt handler (VBAR + 0x18); where the interrupt ID is stored */
DATA_PTR(    0x1028,  current_task)                         /* from task_create; pointer to the current task structure */
THUMB_FN(0xE07C94B2,  msleep)                               /* SleepTask, argument is multiple of 10 */
THUMB_FN(0xE07C5B92,  task_create)                          /* CreateTask, used to start TaskMain, GuiMainTask etc */
DATA_PTR(    0x4100,  task_max)                             /* entry in SysConfig struct */

/** Debug messages & printing **/
THUMB_FN(0xE07E1998,  DryosDebugMsg)                        /* lots of debug messages; format string is third argument */
THUMB_FN(0xE07CAAF0,  uart_printf)                          /* in init task, prints "%s READY\n" */

/** Dumper **/
THUMB_FN(0xE02A3E6C,  dump_file)                            /* tries to save a file to either "A:/%s" or "B:/%s"; calls FIO_RemoveFile/CreateFile/WriteFile/CloseFile/Flush */

THUMB_FN(0xE092BF04, _malloc)                               /* right before "can't malloc" */
THUMB_FN(0xE092BF3C, _free)                                   /* after malloc() in ROM */

THUMB_FN(0xE012325C,  vsnprintf)                            /* called by LogFile_printf(..., "*Data Lost*\n", ...); the one called by DebugMsg only knows %s */

/** Recursive locks **/
THUMB_FN(0xE07C980A,  CreateRecursiveLock)                  /* KerRLock.c */
THUMB_FN(0xE07C9752,  AcquireRecursiveLock)                 /* KerRLock.c */
THUMB_FN(0xE07C97AA,  ReleaseRecursiveLock)                 /* KerRLock.c */

/** Semaphores **/
THUMB_FN(0xE07E14FC,  create_named_semaphore)               /* SystemIF::KerSem.c */
THUMB_FN(0xE07E15F8,  take_semaphore)                       /* SystemIF::KerSem.c, line 307 (0x133) */
THUMB_FN(0xE07E1672,  give_semaphore)                       /* SystemIF::KerSem.c, line 354 (0x162) */

/** Queue **/
THUMB_FN(0xE07C48C4,  msg_queue_create)                     /* SystemIF::KerQueue.c */
THUMB_FN(0xE07C4988,  msg_queue_receive)                    /* SystemIF::KerQueue.c */
THUMB_FN(0xE07C4A1A,  msg_queue_post)                       /* SystemIF::KerQueue.c */

/** Timers **/
THUMB_FN(0xE058F4D4,  SetHPTimerAfterNow)                   /* System::HPTimer.c */
THUMB_FN(0xE058F55A,  SetHPTimerNextTick)                   /* System::HPTimer.c */

/** File I/O **/
THUMB_FN(0xE07BC4F2, _FIO_OpenFile)
THUMB_FN(0xE07BC5B2, _FIO_CreateFile)
THUMB_FN(0xE07BC724, _FIO_RemoveFile)
THUMB_FN(0xE07BC7B8, _FIO_ReadFile)
THUMB_FN(0xE07BCCA0, _FIO_WriteFile)
THUMB_FN(0xE07BD416,  FIO_CloseFile)
THUMB_FN(0xE07BD62E, _FIO_GetFileSize64)
THUMB_FN(0xE07BE484, _FIO_CreateDirectory)
THUMB_FN(0xE029F9CC,  dump_file)                            // Canon safe file dumper to A:/%s or B:/%s


/** Dialogs **/
DATA_PTR(   0x16214,  LiveViewApp_dialog)
THUMB_FN(0xE0795AE0,  dialog_redraw)

/** Storage & Boot **/
DATA_PTR(       0x0,  sd_device)

/** Compositor (CONFIG_COMPOSITOR_XCM) **/
DATA_PTR(  0x14B1A8, _pXCM)                                 // param1 to XCM_MakeContext() when called from RefreshVrmsSurface()
THUMB_FN(0xE0174960,  XCM_GetSourceSurface)                 // retrieves MARV for Layer 0; called in VMIX

/** General FEATURE_VRAM_RGBA stubs **/
THUMB_FN(0xE01A1D0C, XimrExe);                              // In RefreshVrmsSurface, just before "XimrExe" debug message
DATA_PTR(   0x183B8, winsys_sem);                           // Used in RefreshVrmsSurface around XimrExe call

/*
 * Pointer to first Canon RGBA buffer (GUI). On R this is set up by
 * RENDERER_InitializeScreen function, just after MARVs are created.
 */
DATA_PTR(    0x18374, _rgb_vram_info);

/** GUI **/
DATA_PTR(   0x2109C,  gui_task_list)                        // via WINSYS_CreateController()
THUMB_FN(0xE04E4666,  gui_enqueue_message)                  // via "warning! QueueLength="
THUMB_FN(0xE07A226A,  SetGUIRequestMode)                    // via "SetGUIRequestMode"

/** Eventprocs **/
THUMB_FN(0xE07CEA6C,  call)

/** Properties **/
THUMB_FN(0xE07CA508, _prop_cleanup)                         // after faChangeCBR / bindStageChangeAckCBR
THUMB_FN(0xE07C9F5E,  prop_register_slave)                  // PropertyMgr.c

/** Timers **/
THUMB_FN(0xE07CD9C0,  SetTimerAfter)                        // SystemIF::Timer1m.c

/** App handlers **/
THUMB_FN(0xE075099E,  PlayMain_handler)                     // via PlayMain_StartPlayMain
THUMB_FN(0xe9999999,  PlayMovieGuideApp_handler)            // dummy, safe comparison
THUMB_FN(0xE09EFDA2,  ShootOlcApp_handler)                  // via StartShootOlcApp

/** RTC **/
THUMB_FN(0xE0AE1D4C, _LoadCalendarFromRTC)                  // GUI_SetStartMovieRecDate

/** File I/O find & seek **/
THUMB_FN(0xE07BE978, _FIO_FindFirstEx)                      // Storage::Storage.c FirstEnt
THUMB_FN(0xE07BEA88,  FIO_FindNextEx)                       // Storage::Storage.c NextEnt
THUMB_FN(0xE07BEB48,  FIO_FindClose)                        // Storage::Storage.c CloseEnt
THUMB_FN(0xE07BCEE6,  FIO_SeekSkipFile)                     // Storage::Storage.c Search

/** Memory allocation **/
THUMB_FN(0xE07DCB1C, _AllocateMemory)                       // NG AllocateMemory
THUMB_FN(0xE07DCCD4, _FreeMemory)                           // uses the same heap
THUMB_FN(0xE07DC982,  GetMemoryInformation)                 // called from _AllocateMemory()
THUMB_FN(0xE07DC97A,  GetSizeOfMaxRegion)                   // called from _AllocateMemory()

/** Memory suites & resources **/
THUMB_FN(0xE09E2B4E,  AllocateMemoryResource)
THUMB_FN(0xE09E2B82,  AllocateContinuousMemoryResource)
THUMB_FN(0xE09E2BA0,  FreeMemoryResource)
THUMB_FN(0xE07C3CDA,  GetFirstChunkFromSuite)               // PackMem.c
THUMB_FN(0xE07C3B0C,  GetNextMemoryChunk)                   // PackMem.c
THUMB_FN(0xE07C38BE,  GetMemoryAddressOfMemoryChunk)        // PackMem.c

/** GUI Main & Timers **/
DATA_PTR(    0x5460,  gui_main_struct)                      // via "QueueClearFlg Clear"
THUMB_FN(0xE04E4530,  gui_main_task)                        // via task_create("GuiMainTask"
THUMB_FN(0xE0A81EDE,  CancelDateTimer)                      // via "CancelDateTimer"







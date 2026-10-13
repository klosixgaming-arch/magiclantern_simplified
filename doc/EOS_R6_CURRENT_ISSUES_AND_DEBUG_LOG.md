# Canon EOS R6 (DIGIC X) — Current Issues, Technical Log & Contributing Guide

**Target Hardware:** Canon EOS R6 (Mark I)  
**Firmware Version:** 1.7.0  
**SoC:** DIGIC X (Multi-core ARM Cortex-A53, AArch32 Thumb-2 execution mode)  
**Main Branch:** `dev` (based on `reticulatedpines/magiclantern_simplified`)

---

## 1. Executive Summary

This document serves as the comprehensive engineering log and issue tracker for the native Magic Lantern port to the Canon EOS R6 on Firmware 1.7.0. It documents all resolved hardware and software hurdles, exact reverse-engineered structures, and the current active issue blocking the full native build.

---

## 2. Active Issue: Native Boot Freeze in `_mem_init()`

### 2.1 Observed Hardware Behavior
- **Build Target:** Native Magic Lantern core (`autoexec.bin`, built from `platform/R6.170`).
- **Hardware Ground Truth:**
  - Upon power-on, the card access LED blinks **exactly 1 time** (`dbg_blink(1)`).
  - The camera freezes immediately with the screen remaining blank/unresponsive.
  - Exactly 8–10 seconds later, the hardware watchdog timer expires, rebooting the camera in a loop.
- **Milestone Reached:** The bootloader cleanly transfers control to `my_init_task()`, `bmp_vram_raw()` wait loop succeeds, and `task_create("ml_init", ...)` successfully starts `my_big_init_task()`.

### 2.2 Exact Code Location
In [`src/init.c`](file:///C:/Users/Mega/Downloads/magiclantern_simplified/src/init.c#L2316):
```c
static void my_big_init_task()
{
    dbg_blink(1);   /* 1: ml_init task started - REACHED & EXECUTED */
    _mem_init();    /* <-- FREEZE OCCURS STRICTLY HERE */
    dbg_blink(7);   /* 7: _mem_init passed - NEVER REACHED */
    _find_ml_card();
    dbg_blink(2);   /* 2: _find_ml_card passed */
    ...
```

### 2.3 What `_mem_init()` Executes
In [`src/mem.c:1083`](file:///C:/Users/Mega/Downloads/magiclantern_simplified/src/mem.c#L1083):
```c
void _mem_init()
{
    if (mem_sem == NULL)
        mem_sem = create_named_semaphore("mem_sem", SEM_CREATE_UNLOCKED);

    for (int a = 0; a < COUNT(allocators); a++)
    {
        if (allocators[a].init)
        {
            allocators[a].init();
        }
    }
}
```
- In `allocators[]` (lines 222–334 of `src/mem.c`), none of the 4 allocators (`malloc`, `AllocateMemory`, `shoot_malloc`, `srm_malloc`) define `.init`.
- Therefore, the **only executable operation** inside `_mem_init()` is:
  ```c
  mem_sem = create_named_semaphore("mem_sem", SEM_CREATE_UNLOCKED);
  ```

### 2.4 Technical Disassembly of `create_named_semaphore` (`0xE07E14FC`)
From `gang100.bin`:
```arm
e07e14fc: push    {r4, r5, r6, lr}
e07e14fe: mov     r5, r0               ; r5 = name string pointer ("mem_sem")
e07e1500: mov.w   r0, #4096            ; 0x1000 (KerState block)
e07e1504: mov     r4, r1               ; r4 = count (1 = SEM_CREATE_UNLOCKED)
e07e1506: ldr     r0, [r0, #12]        ; Read lock / ISR nesting counter at 0x100C
e07e1508: cmp     r0, #0
e07e150a: ble.n   0xe07e1516
e07e150c: movs    r2, #94              ; Error 94: Called from ISR / locked context!
e07e150e: add     r1, pc, #480
e07e1510: add     r0, pc, #496
e07e1512: bl      0xe08a8c38           ; DryOS kernel panic / abort
e07e1516: cbz     r4, 0xe07e152a
e07e1518: cmp     r4, #1
e07e151a: beq.n   0xe07e152a
e07e151c: movs    r2, #98              ; Error 98: Invalid initial count
e07e151e: bl      0xe08a8c38
e07e152a: movs    r1, #2               ; Mode / attributes = 2
e07e152c: mov     r2, r5               ; Name string
e07e152e: mov     r0, r4               ; Count = 1
e07e1530: bl      0xe07c7bb4           ; CreateSemaphore internal allocator
```

### 2.5 Diagnostic Hypotheses & Recommended Fixes for Contributors
1. **Hypothesis A: DryOS Semaphore Table Exhaustion or SMP Locking:**
   - On DIGIC X multi-core SMP, creating a named semaphore from `ml_init` (priority `0x1E`, running on CPU0) while `gui_main_task` or other Canon tasks are starting might trigger a spinlock timeout at `0xE07C7BD4`.
   - **Test:** Replace `create_named_semaphore` with a dummy semaphore or bypass `mem_sem` initialization to see if the boot sequence reaches `dbg_blink(7)` and `dbg_blink(2)`.
2. **Hypothesis B: MALLOC_STRUCT_ADDR & Memory Pool Query:**
   - In `platform/R6.170/stubs.S`, `DATA_PTR(0xFCE00, MALLOC_STRUCT_ADDR)` may differ between R6 firmware versions (e.g. 1.5.0 vs 1.7.0).
   - If any early allocation invokes `GetFreeMemForMalloc()` ([`src/mem.c:125`](file:///C:/Users/Mega/Downloads/magiclantern_simplified/src/mem.c#L125)), reading an unaligned or invalid `MALLOC_STRUCT_ADDR` triggers an immediate ARM Data Abort.
3. **Hypothesis C: Task Stack Overflow:**
   - `my_big_init_task()` is spawned with stack size `0x4000` (16 KB) at priority `0x1E`. On DIGIC X, functions with large local buffers might overflow early.
   - **Test:** Increase stack size to `0x8000` or `0x10000` in `src/init.c:2631`.

---

## 3. Comprehensive Log of Resolved Issues

| # | Issue / Milestone | Root Cause | Resolution |
| :--- | :--- | :--- | :--- |
| **1** | **Large SD Card Support** | Internal camera ROM bootflag disabled by default. | Permanently set `EnableBootDisk()` in internal flash via Canon Basic during Rung 0. Any SD card (64GB–512GB+) formatted bootable boots ML. |
| **2** | **Photo Review (PLAY Mode) Latch Bug** | Canon DryOS sets `0x8D74` (`req_mode = 0x2002`) on playback entry but does not clear it when tapping shutter. | Only check active `gui_mode` at `0x8D6C`, and release latch immediately upon half-shutter (`0x9F`), full shutter (`0xA1`), or dial rotation. |
| **3** | **Phantom Key Codes 0x44 & 0x45** | `SwMode_ButtonCBR` (`0xE0109133`) polls mode dial at high frequency, generating false events. | Added filter `!(ev_param >= 0x44 && ev_param <= 0x56)` to suppress background polling noise. |
| **4** | **SET Wheel & Joystick Navigation** | SET Wheel posts `0x04` (CCW) and `0x05` (CW), which were mistaken for button press/release. | Mapped `0x04`/`0x05` to dial navigation; mapped cross-pad (`0x3C`–`0x43`) and joystick (`0x77`–`0x7E`, `0xA7`) to UP/DOWN; mapped `0x7F` (Stick Push) and `0x2C` (Q) to Select. |
| **5** | **Runaway Menu Scrolling (`0x00 HLD`)** | Allowing `ev_type <= 8` accepted internal display refresh events (`ev_type = 2/3`) carrying `ev_param = 0x00` at 30–60 Hz. | Enforced strict `ev_type == 0` filtering (physical switches only); CCW click (`0x00`) requires `ev_arg > 0`. |
| **6** | **Manual Focus Popup Erasing OSD** | Canon renders MF distance scale directly to VRAM bitmap layer, clobbering ML OSD until next button press. | Implemented self-healing OSD refresh: repaints every 5 loop cycles (~6.6 fps) and triggers immediate redraw on event `0xB7` (`GMT_OLC_INFO_CHANGED`). |
| **7** | **Multi-Controller Joystick `0xA7` Decoded** | DIGIC X joystick is an analog sensor posting unified code `0xA7` with direction in `ev_arg` and `ev_obj`. | Added bitfield decoding for 8-way directional enum and 16-bit deltas. |
| **8** | **Exposure Dial Filtering** | Overly broad filter dropped Exposure Dial (`0x4E`/`0x4F`, `0x89`/`0x8A`, `0xB0`..`0xB4`) clicks. | Restricted suppression strictly to `0x44`/`0x45`, unblocking all physical dials. |
| **9** | **Fast 1-Second Boot Sequence** | Unnecessary debug pauses delayed startup. | Reduced boot delay to 1000ms with 3 quick confirmation blinks, reaching LiveView within 1.3 seconds. |
| **10** | **AF-Proof Continuous Menu Rendering** | Canon eye/subject AF tracking boxes continuously redraw directly to VRAM at 60 fps, erasing ML menus. | Enabled continuous rendering at ~33 fps while `menu_active == 1`, cleanly preventing AF box erasure. |
| **11** | **Option B: 32-Bit BMP Screenshot Engine** | Needed bit-exact screenshot capabilities. | Implemented native 32-bit ARGB little-endian BMP dumper for 960×540 LCD view (2 MB) and 2048×1080 canvas (8.8 MB) with dual-layer alpha compositing (`blend_argb`). |
| **12** | **Option C: MMIO Probing Safety** | Probing unclocked high-speed peripherals (PCIe `0xD0010000`, eMMC `0xD0040000`, USB `0xD0060000`) caused AXI bus data aborts. | Replaced dangerous silicon probes with static memory-map specifications reverse-engineered from ROMs. |
| **13** | **Option D: 14-Bit Bayer RAW Sensor Stream** | Needed ground-truth optical sensor frame buffer. | Verified live Bayer stream at `0x003C0000` (1888×1256, active crop 1832×1220, 14 bpp, 2,371,328 bytes); confirmed with live dumps `BOOT_SENSOR.RAW` and `SENS0001.RAW`. |
| **14** | **Option E: Property Database & Shutter Count** | Needed shutter count and hardware serials. | Successfully hooked `PROPAD_GetPropertyAddress` (`0xE07E0743`); extracted `CIG_DATA` (`0x418D98B0`), `SERVICE_DATA` (`0x418D98BC`), `ERROR_LOG` (`0x418D9880`), and SCS shutter return `0x05F5DDE0`. |
| **15** | **Option F: Audio Subsystem & Codec MMIO** | Needed audio registers and routing. | Mapped audio properties (`0x8003002B`, `0x80030055`, `0x80000080`), DRAM context at `0x00017B54`, MMIO controllers at `0xD2710000`/`0xD2720000`, and 13 `SoundDev` procedures at `0xE0522600`. |
| **16** | **`fio-ml.c` Empty-Slot Probing Hang** | `_find_ml_card()` called `is_dir("A:/ML")` first; probing empty Slot 1 (`A:`) locked the storage driver on an unmounted slot. | Added `CONFIG_DIGIC_X` override in `src/fio-ml.c:214` to bind directly to `CARD_B` (`B:/`). |

---

## 4. Hardware Memory & MMIO Reference

- **DRAM Base:** `0x00000000` (1024 MB total, `0x00000000`–`0x3FFFFFFF`)
- **Uncached DRAM Mirror:** `0x40000000` (`0x40000000`–`0x7FFFFFFF`)
- **On-chip SRAM0:** `0xDFFC0000` (256 KB)
- **Primary ROM (Firmware):** `0xE0000000` (64 MB)
- **Secondary ROM:** `0xF0000000` (32 MB)
- **Primary Display MARV (Layer 0 UI):** `0x0014B000` (active context `0x0014B1A8`, resolution 960 × 540)
- **Secondary Display MARV (Layer 1 LiveView):** `0x00018384` (resolution 960 × 540)
- **VRAM RGBA Pointer:** `0x00018374` (`_rgb_vram_info`)
- **Window System Semaphore:** `0x000183B8` (`winsys_sem`)
- **Card Access LED Register:** `0xD20B0A28` (GPIO pinmux at `0xD22390C0`; `0x2` = ON, `0x3` = OFF)
- **Sensor RAW Buffer:** `0x003C0000` (1888 × 1256 × 14bpp, 2,371,328 bytes)
- **Current Task Pointer:** `0x00001028` (`current_task`)
- **GICv2 CPU Interface:** `0xC1000000`
- **GICv2 Distributor:** `0xC1001000`
- **Marius Timers:** `0xC0208000` (Channels 0–4 at `+0x890`..`+0x8D0`)
- **Debug UART0 Base:** `0xC0800000` (TX Data `+0x00`, TX Flags `+0x14`)

---

## 5. Build & Deployment Instructions

### Prerequisites
- WSL2 (Ubuntu / Debian)
- `arm-none-eabi-gcc` toolchain

### Build Commands
```bash
# Build native Magic Lantern binary
cd /mnt/c/Users/Mega/Downloads/magiclantern_simplified/platform/R6.170
make

# Output files generated:
# build/autoexec.bin (Bootable camera payload)
# build/R6_170.sym   (Symbol table for modules)
```

### Installation to SD Card
1. Format SD card (FAT32 for cards <=32GB, exFAT for cards >=64GB).
2. Enable bootflag via EOScard or `python3 tools/card-flags/edit_card_flags.py /dev/sdX BOOTDISK`.
3. Copy `build/autoexec.bin` to root directory of SD card (`D:\autoexec.bin`).
4. Copy `build/R6_170.sym` to `D:\ML\modules\R6_170.sym`.

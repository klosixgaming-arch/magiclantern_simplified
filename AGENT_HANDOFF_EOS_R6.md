# Canon EOS R6 (DIGIC X) — Magic Lantern Agent Handoff Guide

> **NOTICE FOR INCOMING AGENT:**  
> This file is the primary handoff document for continuing the Magic Lantern port on the physical **Canon EOS R6 (Firmware 1.7.0, DIGIC X)**. It contains the exact hardware context, codebase paths, verified milestones, resolved bugs, diagnosis of the current state, and an actionable To-Do list.

---

## 1. Executive Summary

- **Target Camera:** Canon EOS R6 (Mark I) running physical Firmware **1.7.0**.
- **SoC Architecture:** Canon DIGIC X (Multi-core ARM Cortex-A53 / Cortex-R5, AArch32 Thumb-2 execution mode, DryOS kernel).
- **Major Milestone Achieved:** **Milestone 6 Boot Clean!**
  - Bootloader handoff, memory allocation, fonts, all 24 init functions, configuration loading, and all 17 auto-created background tasks run live in camera memory.
  - Diagnostic LED blinks on real hardware: `1. 2. 3. 4. 5. 6. no freeze`.
- **Fatal Freezes Resolved:**
  - Resolved `0xC048` GUI mode request crash in `src/menu.c`.
  - Resolved `take_semaphore(winsys_sem, 0)` infinite deadlock in `src/bmp.c`.
  - Resolved persistent `LOADING.LCK` crash recovery freeze in `src/module.c`.
- **Current Status:**
  - When the user presses the **TRASH** button in LiveView, **the camera does NOT freeze** (the previous fatal freeze is completely fixed!).
  - However, the Magic Lantern menu does not yet render on the LCD screen.
  - The goal for the next agent is to connect TRASH button interception to ML menu opening and render the menu onto the LCD screen.

---

## 2. Directory Layout & Repository Reference

### Local File System Paths (Windows & WSL)
| Purpose | Windows Path | WSL Path |
| :--- | :--- | :--- |
| **Main Repository Root** | `C:\Users\Mega\Downloads\magiclantern_simplified` | `/mnt/c/Users/Mega/Downloads/magiclantern_simplified` |
| **Platform Target Dir** | `C:\Users\Mega\Downloads\magiclantern_simplified\platform\R6.170` | `/mnt/c/Users/Mega/Downloads/magiclantern_simplified/platform/R6.170` |
| **Target SD Card** | `D:\` | `/mnt/d` |
| **Target Binaries on SD** | `D:\autoexec.bin`, `D:\ML\modules\R6_170.sym` | `/mnt/d/autoexec.bin`, `/mnt/d/ML/modules/R6_170.sym` |
| **ROM Dump Directory** | `C:\Users\Mega\Downloads\R6 Testing\2026 R6 Rom dumps\1.7.0` | `/mnt/c/Users/Mega/Downloads/R6 Testing/2026 R6 Rom dumps/1.7.0` |
| **Primary ROM Binary** | `C:\Users\Mega\Downloads\R6 Testing\2026 R6 Rom dumps\1.7.0\boot00.bin` (Base: `0xE0100000`, Size: ~38.5 MB) | `/mnt/c/Users/Mega/Downloads/R6 Testing/2026 R6 Rom dumps/1.7.0/boot00.bin` |

### GitHub Repositories & Remotes
- **Origin Remote (Active Work):** `https://github.com/klosixgaming-arch/magiclantern_simplified.git`
- **Upstream Remote:** `https://github.com/reticulatedpines/magiclantern_simplified.git`
- **Active Working Branch:** Check `git branch` before pushing.

---

## 3. Toolchains & Build Workflow

Compilation is performed via WSL using the ARM GCC cross-compiler:
```bash
# In WSL:
cd /mnt/c/Users/Mega/Downloads/magiclantern_simplified/platform/R6.170
make

# Deployment to SD Card (PowerShell from Windows):
Copy-Item "C:\Users\Mega\Downloads\magiclantern_simplified\platform\R6.170\build\autoexec.bin" "D:\autoexec.bin" -Force
Copy-Item "C:\Users\Mega\Downloads\magiclantern_simplified\platform\R6.170\build\R6_170.sym" "D:\ML\modules\R6_170.sym" -Force
```

---

## 4. Hardware Ground Truth & Key Addresses

Empirically extracted and verified from real EOS R6 hardware:

| Name | Address / Value | Description |
| :--- | :--- | :--- |
| **ROM Base Address** | `0xE0100000` | Start of `boot00.bin` mapped memory |
| **Card Activity LED** | `0xD22390C0` | `LEDON = 0x24D0002`, `LEDOFF = 0x24C0003` |
| **GUI Event Ring Buffer** | `0x0007F554` | Ring base (0x401 = 1025 slots, 16 bytes/slot: `type, param, obj, arg`). Backing store of `msg_queue_eosr` (see §9). *Not* `0x00135800` — that address has zero ROM references. |
| **GUI Event Ring Index** | `0x00005474` | `gui_main_struct + 0x14`, index of the *next* slot to be written. *Not* `0x001357F4`. |
| **TRASH (Erase) Button Code** | `0x14` (press), `0x15` (release) | Verified in `KEYLOG.TXT` on physical hardware |
| **Primary MARV Layer 0** | `0x0014B000` (fallback: `0x00018384`) | Holds pointer to Layer 0 MARV structure |
| **Layer 0 Bitmap DRAM Base** | `0x22788900` | 2048 x 1080 stride RGBA display canvas in DRAM |
| **Layer 1 (LiveView Video)** | `0x0014B00C` | Holds pointer to LiveView sensor video feed MARV |
| **GuiMainTask Entry** | `0xE04E4530` | Canon main GUI event loop task |
| **GuiMainStruct** | `0x00005460` | Offset `+0x1C` is `msg_queue_eosr`, offset `+0x04` is counter |
| **GMT_FUNCTABLE** | `0xE0CCC20C` | 7 function pointers for GUI message types 0 through 6 |

---

## 5. Critical User Constraints & Rules

1. **LED Burst Limit (Strictly ≤ 6 Blinks):**
   - The user has mandated: **All diagnostic burst counts must strictly be under 7 blinks (1 to 6 blinks)**.
   - Never use 7, 10, or 12 blinks. Keep diagnostic pulses in the 1–6 range with distinct pauses.
2. **CPU Yielding in Loops:**
   - Any loop polling hardware or waiting must yield the CPU (`msleep(10)`) to avoid starving Canon's coprocessor IPC tasks.
3. **No Unclocked MMIO:**
   - Never read from `0xD2710000`–`0xD2722000` (Audio DSP) during early boot; it is power-gated and will cause an unrecoverable AXI hardware bus abort.

---

## 6. Resolved Root Causes (Reference)

1. **`ASSERT` in `state_init` (Init Func #18):**
   - `src/state-object.c:266` previously crashed because `EVF_STATE` (`0xa600`) is invalid on DIGIC X.
   - Fixed by adding a defensive `streq` guard and disabling `CONFIG_STATE_OBJECT_HOOKS`.
2. **`SetGUIRequestMode(0xC048)`:**
   - `0xC048` is a legacy DIGIC 5 GUI mode. Calling it crashed Canon's `GuiMainTask`.
   - Fixed: Set `#define GUIMODE_ML_MENU 0` in `platform/R6.170/consts.h`, and bypassed mode switching in `src/menu.c`.
3. **`take_semaphore(winsys_sem, 0)` Deadlock:**
   - `0x183B8` is not a valid DryOS semaphore in R6 ROM. Calling `take_semaphore(winsys_sem, 0)` hung `refresh_yuv_from_rgb_task`.
   - Fixed: Bypassed `winsys_sem` and `XimrExe` on DIGIC X in `src/bmp.c`.
4. **Missing D-Cache Flushing:**
   - Writes to Layer 0 remained in Cortex-A53 L1/L2 cache; display DMA saw transparent black.
   - Fixed: Added `clean_dcache_range()` per written row in `refresh_yuv_from_rgb()` and `rgba_buffer_clear()`.
5. **Sticky Reboot Crash (`LOADING.LCK`):**
   - Fixed: Added `FIO_RemoveFile(module_lockfile)` upon crash recovery in `src/module.c`.

---

## 7. Diagnosis of Current Challenge (Why Menu Didn't Draw on TRASH)

The camera reached Milestone 6 and did not freeze when TRASH was pressed, but nothing appeared on screen. There are two primary suspects:

### Suspect A: Canon Does Not Route `0x14` (TRASH) to `msg_queue_eosr` in LiveView
- In `src/gui.c`:
  ```c
  msg_queue_receive(gui_main_struct.msg_queue_eosr, &event, 0);
  if (event->type == 0) handle_buttons(event);
  ```
- In Canon EOS R6 firmware, when the camera is in LiveView, the physical TRASH button may **not** be posted to Canon's `GuiMainTask` message queue (`0x5460 + 0x1C`) unless Erase mode is active.
- **Correction (see §9):** `hello_world()` actually reads index `0x5474` / base `0x7F554`. This ring is the storage behind `msg_queue_eosr`, so Suspect A as worded cannot be true. The real cause was `CONFIG_NO_TASK_HOOK`.
- **Solution:** Add a background listener task (or poll the hardware ring buffer in `menu_task`) that detects `ev_param == 0x14 && ev_arg == 1` and calls `give_semaphore(gui_sem)`.

### Suspect B: `menu_redraw_task` / `refresh_yuv_from_rgb` Not Triggered
- Verify whether `menu_shown` actually flipped to `1`.
- Verify whether `menu_redraw_queue` received `MENU_REDRAW`.
- Verify whether `rgb_vram_info` is properly initialized when `refresh_yuv_from_rgb()` runs.

---

## 8. Prioritized To-Do List for Next Agent

- [x] **Task 1: Add Diagnostic Ring Buffer Poller for TRASH Button** (implemented, awaiting hardware test)
  - **DONE (see §9):** `gui_ring_poll` task in `src/menu.c` polls `0x5474` / `0x7F554`.
  - When `ev_param == 0x14` (TRASH) and `ev_arg == 1` (press) is detected:
    - Blink LED 1 quick pulse (confirmation of TRASH button detection).
    - Call `give_semaphore(gui_sem)` to open the Magic Lantern menu.
- [ ] **Task 2: Verify `rgb_vram_info` Surface Pointer in `refresh_yuv_from_rgb()`**
  - Ensure `*(struct MARV **)0x14B000` is non-null and signature is `0x5652414D` before copying.
  - Confirm the rendered RGBA menu pixels are flushed to DRAM with `clean_dcache_range()`.
- [ ] **Task 3: Test Interactive Navigation on Hardware**
  - Once the ML menu is visible on the LCD, verify dial rotations (Main Dial `0x00`, Quick Control Dial 1 `0x02`, Quick Control Dial 2 `0x03`) and SET (`0x04`) navigate menu items.
- [ ] **Task 4: Maintain Repository & Documentation**
  - Commit progress regularly to `origin`.
  - Keep `doc/EOS_R6_DIGIC_X_FIRMWARE_REVERSE_ENGINEERING_REPORT.md` updated.

---

## 9. Session 2026-10-13: Root Cause of "TRASH Does Nothing" + Ring Poller

### 9.1 Root cause
`platform/R6.170/features.h` still had `#define CONFIG_NO_TASK_HOOK` (left over from a bisect).
`TASK_OVERRIDE(gui_main_task, ml_gui_main_task)` only takes effect via `my_task_dispatch_hook`
(`src/init.c`), which is never installed with that define. **So `ml_gui_main_task` never runs:**
Canon's own GuiMainTask drains every GUI event, and ML's `handle_buttons()` never sees *any* key.
This is why TRASH stopped freezing and also why nothing happens.

### 9.2 The GUI event ring (ROM-verified)
Canon's GUI event poster at `0xE04E4666` (right after GuiMainTask at `0xE04E4530`) does:
```
idx = gui_main_struct.off_0x14;                  // [0x5474]
gui_main_struct.off_0x14 = (idx + 1) % 0x401;    // index bumped BEFORE the slot is filled
ring[0x7F554 + idx*16] = { type, param, obj, arg };
msg_queue_post(gui_main_struct.msg_queue_eosr, &ring[idx]);
gui_main_struct.off_0x04++;
```
* The ring **is** the queue's storage: every event in it is also delivered to `msg_queue_eosr`.
* Buttons are `type 0`. Canon also posts `type 3` events with params that collide with button codes
  (`0x10, 0x11, 0x1B, 0x2C, 0x2D, 0x2E, 0x36, 0x38`). The `0x2C` lines with `obj=092F04xx` in older
  KEYLOGs were type-3 events, not the Q button. **Always filter on `type == 0`.**
* `0x00135800` / `0x001357F4` have **zero** references in `boot00.bin`; do not use them.

### 9.3 What was implemented (`CONFIG_GUI_EVENT_RING_POLL`, R6 only)
* `src/menu.c` → task `gui_ring_poll` (prio 0x1A, `msleep(10)` every cycle, lags one cycle behind the
  index to avoid half-written slots, resyncs if backlog > 64).
  * `type 0, param 0x14` → LED 1 blink, `give_semaphore(gui_sem)`.
  * other `type 0` events while the menu is open → `handle_ml_menu_keys()` (navigation).
    Canon still receives these too (we observe, we cannot block).
  * Stands down automatically if `ml_gui_task_active` (set in `src/gui.c`) becomes 1.
* `src/bmp.c`
  * On DIGIC X, `refresh_yuv_from_rgb()` now **always** uses the 2048-stride + `clean_dcache_range()`
    path. Previously, if `zebra_should_run()` was true it did a contiguous 960×540 copy with no stride
    and no cache clean.
  * `refresh_yuv_from_rgb_task` re-pushes the menu to Layer 0 every 50 ms while it is open, so
    Canon's own Layer 0 redraws can't erase it.

### 9.4 LED codes after a TRASH press (all ≤ 6)
| Burst | Meaning |
|---|---|
| 1 | TRASH (type 0) seen in ring, semaphore given |
| then 2 | menu is now OPEN (`menu_open()` ran) |
| then 3 | menu is now CLOSED |
| then 4 | menu state did not change (menu_task stuck, or it closed the menu immediately) |
| 5 | an event with param 0x14 but type ≠ 0 arrived (ignored) |

Each press also writes `RINGDIAG.TXT` (card root, max 8 writes per boot): gui_state, GUI mode,
menu_task heartbeat, menu draw count, Layer 0 refresh count, MARV pointers, and the last 24 events with type.

### 9.5 Next steps
1. Test: LiveView → press TRASH → read the LED codes → pull `RINGDIAG.TXT`.
2. If **1 then 2** but no menu is visible: look at `menu_redraw_do_count` / `ml_refresh_count` in RINGDIAG.
   If both increase, the pixels are reaching `*(0x14B000)` and the question becomes which compositor
   layer is actually on screen.
3. If **1 then 4**: check `gui_state` in RINGDIAG (menu_task closes the menu when `gui_state == 2`).
4. Longer term: re-enable the task hook (remove `CONFIG_NO_TASK_HOOK`) so ML can *block* keys from
   Canon. The poller will disable itself once `ml_gui_main_task` is running.

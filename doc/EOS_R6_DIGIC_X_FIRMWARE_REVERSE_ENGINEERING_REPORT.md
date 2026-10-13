# Canon EOS R6 (Firmware v1.7.0) — DIGIC X Reverse-Engineering & Emulation Handover Report

**Document Version:** 1.0.0  
**Target Platform:** Canon EOS R6 (Mark I)  
**Firmware Version:** 1.7.0 (Verified on Hardware)  
**Primary SoC:** Canon DIGIC X (Multi-core ARM Cortex-A53, AArch32 Execution Mode)  
**Operating System:** Canon DryOS v2.3 (Release #0059+)  
**Purpose:** Comprehensive architectural specification, memory map, display subsystem breakdown, input ring buffer protocol, and peripheral register definitions to accelerate custom QEMU machine (`canon-r6`) development.

---

## 1. Executive Architecture & System Memory Map

### 1.1 CPU & Execution Environment
- **Core Architecture:** ARM Cortex-A53 multi-core cluster running in **32-bit AArch32 mode** (ARMv7-A / ARMv8-A compatibility mode).
- **Instruction Set:** Thumb-2 (`-mthumb -march=armv7-a`). All DryOS functions, handlers, and tasks execute in Thumb mode unless otherwise noted.
- **Cache Architecture:** L1 Harvard cache (32 KB I-Cache, 32 KB D-Cache, 64-byte line size), unified L2 cache.
- **Cache Maintenance:** Strict write-back cache. In DIGIC X, all DMA engines (XIMR display compositor, EDMAC, sensor readout) read DRAM directly via the 64-bit IBus, bypassing CPU caches. Pixel writes and DMA buffers **must** be flushed using `DCCMVAC` (`MCR p15, 0, %0, c7, c10, 1`) with `DSB sy`.

### 1.2 Memory Address Space Map

| Address Range | Size | Region Name | Description / Function |
| :--- | :--- | :--- | :--- |
| `0x00000000 - 0x7FFFFFFF` | ~2.0 GB | **Main DRAM** | Primary system memory, DryOS heap, task stacks, framebuffers |
| `0x00800000` | — | **Autoexec Load Address** | Canon primary bootloader loads `autoexec.bin` from SD card root here |
| `0x00190060` | — | **`RESTARTSTART`** | DryOS reboot / second-stage firmware re-entry vector |
| `0xD0000000 - 0xDFFFFFFF` | 256 MB | **MMIO / Peripherals** | DIGIC X internal hardware peripheral registers |
| `0xD0130094` | 4 B | **Card LED GPIO** | SD Card access LED control register (`0x00020002` = ON, `0x00020000` = OFF) |
| `0xE0000000 - 0xE3FFFFFF` | 64 MB | **ROM Bank 1 (`gang100.bin`)** | Primary firmware image, boot code, DryOS kernel, GUI tasks |
| `0xE0100000` | — | **`MAIN_FIRMWARE_ADDR`** | Firmware reset entry point, vector table, and `cstart` (`0xE01000EC`) |
| `0xE4000000 - 0xE5FFFFFF` | 32 MB | **ROM Bank 2 (`gang200.bin`)** | Extended camera assets, lens profiles, language tables |

---

## 2. Graphics Pipeline & Framebuffer Architecture (Target: QEMU Visuals)

### 2.1 The Canon Display Model: XIMR + XCM Compositor
Unlike older DIGIC 4/5 cameras that wrote to a single flat linear framebuffer, DIGIC X utilizes an advanced multi-layer hardware display engine consisting of:
1. **XIMR** (*eXtended Image Manipulator / Renderer*): Hardware 2D raster engine and DMA blitter.
2. **XCM** (*eXtended Compositor Manager*): Multi-surface compositor combining LiveView video, Canon GUI dialogs, and overlays into the physical LCD/EVF stream.
3. **VMIX** (*Video Mixer*): Final video timing generator driving the 960x540 LCD panel and 3.69M-dot OLED EVF.

### 2.2 The Core Display Structure: `struct MARV` (Empirically Verified from Hardware Dump)
Every display surface in DryOS is encapsulated in a **MARV** descriptor table (`0x5652414D`, which is ASCII `'VRAM'` reversed in Little-Endian byte order).
From our live hardware dump (`MARVDUMP.TXT`), the exact byte-level structure is:

```c
struct MARV
{
    uint32_t signature;     /* +0x00: 0x5652414D ("VRAM" in little-endian) */
    uint8_t *bitmap_data;   /* +0x04: 0x22788900 (Pointer to DRAM pixel buffer) */
    uint8_t *opacity_data;  /* +0x08: 0x00000000 (NULL; alpha is interleaved in 32-bit ARGB) */
    uint32_t memif_1;       /* +0x0C: 0xFFFFFFFF (IBus DMA attribute flag) */
    uint32_t ibus_addr_lo;  /* +0x10: 0x02788900 (Lower 32 bits of 64-bit IBus address) */
    uint32_t ibus_addr_hi;  /* +0x14: 0x00000002 (Upper 32 bits; physical DRAM base 0x2_00000000) */
    uint32_t flags;         /* +0x18: 0x05040100 (Surface configuration / blend attributes) */
    uint32_t width;         /* +0x1C: 0x00000800 = 2048 decimal (Canvas stride in words / pixels) */
    uint32_t height;        /* +0x20: 0x00000438 = 1080 decimal (Canvas height in pixels) */
    void    *pmem;          /* +0x24: 0x22FF8928 (Pointer to PMEM allocation block) */
    /* Embedded PMEM descriptor follows at +0x28: */
    uint32_t pmem_magic;    /* +0x28: 0x4D454D50 ("PMEM" in little-endian) */
    uint32_t pmem_flags;    /* +0x2C: 0x00000010 */
    uint32_t pmem_unused;   /* +0x30: 0x00000000 */
    uint32_t pmem_base;     /* +0x34: 0x22788800 (Base block before 256B alignment) */
};
```

### 2.3 Live MARV Pointers in Canon Memory

| Memory Address | Symbol / Variable | Hardware Dump Value | Purpose in Firmware |
| :--- | :--- | :--- | :--- |
| `0x14B000` | `*(struct MARV **)0x14B000` | `0x22FF8900` | **Primary Layer 0 MARV**: Main Canon GUI & OSD bitmap surface. |
| `0x14B00C` | `*(struct MARV **)0x14B00C` | `0x23868A00` | **Secondary Layer 1 MARV**: Video / LiveView display layer. |
| `0x018384` | `*(struct MARV **)0x018384` | Valid MARV | Fallback MARV surface pointer table (early boot fallback). |
| `0x018374` | `_rgb_vram_info` | Pointer | Pointer to primary Canon RGBA buffer from `RENDERER_InitializeScreen`. |
| `0x14B1A8` | `_pXCM` | Pointer | Pointer to active XCM compositor context. |
| `0x0183B8` | `winsys_sem` | Semaphore | Windowing system semaphore guarding compositor execution. |
| `0xE0174960` | `XCM_GetSourceSurface` | Function | Returns MARV descriptor for a given layer index. |
| `0xE01A1D0C` | `XimrExe` | Function | Hardware blitter execution trigger called in `RefreshVrmsSurface`. |
| `0xE0795AE0` | `dialog_redraw` | Function | Triggers full GUI redraw cycle for active dialog windows. |

### 2.4 Pixel Format, Canvas Resolution & Row Stride

> [!IMPORTANT]
> **Definitive Stride Answer for QEMU:**
> - The stride is **NEITHER 3,840 bytes NOR 4,096 bytes**!
> - The canvas buffer is allocated as **2048 words = 8,192 bytes per row** (`width = 0x800 = 2048`).
> - Canvas Height is **1,080 rows** (`height = 0x438 = 1080`).
> - Row $y$ starts at: `bitmap_data + (y * 2048 * 4)` = **`bitmap_data + (y * 8192)`**.
> - When rendering to the physical **960 × 540 LCD** or EVF, the hardware compositor (XIMR) samples from this 2048 × 1080 surface!

- **Physical LCD Resolution:** 960 × 540 pixels (16:9 aspect ratio).
- **Surface Canvas Allocation:** 2048 × 1080 pixels (Full HD sub-sampled canvas).
- **Bytes per Row (Stride):** `2048 * 4 = 8,192 bytes` (stride = 2048 `uint32_t` pixels).
- **Pixel Packing:** 32-bit True Color with 8-bit Alpha Channel:
  - **Memory Layout (Little Endian uint32_t):** `[Byte 0: Blue] [Byte 1: Green] [Byte 2: Red] [Byte 3: Alpha]`
  - `0xFF000000` = Solid Pure Black (default background seen in dump)
  - `0xFF00FF00` = Solid Pure Green
  - `0xFFFFFF00` = Solid Pure Cyan
  - `0xFF00FFFF` = Solid Pure Yellow
  - `0xFFFFFFFF` = Solid Pure White
  - `0x00000000` = Fully Transparent (reveals the underlying LiveView video layer)
  - `0xEB141212` = ~92% Opacity Dark Charcoal background overlay

### 2.5 Implementing the QEMU Display Device
To render the camera's menus and UI in QEMU:
1. **Locate the Buffer:** Read `uint32_t marv_ptr = ldl_phys(0x14B000);` (returns `0x22FF8900`).
2. **Verify Signature:** Ensure `ldl_phys(marv_ptr) == 0x5652414D` (`'VRAM'`).
3. **Read Framebuffer Pointer:** `uint32_t vram_base = ldl_phys(marv_ptr + 4);` (returns `0x22788900`).
4. **Dimensions & Stride:**
   - Width: `ldl_phys(marv_ptr + 0x1C)` = `2048`
   - Height: `ldl_phys(marv_ptr + 0x20)` = `1080`
   - Stride: `2048 * 4` = `8192` bytes per row
5. **Render Hook:** Hook calls to `XimrExe` (`0xE01A1D0C`) or refresh at 30/60 Hz:
   ```c
   /* QEMU Display Surface setup for 960x540 LCD view */
   DisplaySurface *surface = qemu_create_displaysurface_from(
       960, 540, PIXMAN_a8r8g8b8, 8192, /* stride = 2048 words * 4 bytes = 8192 bytes */
       (uint8_t *)qemu_map_ram_ptr(NULL, vram_base)
   );
   dpy_gfx_replace_surface(s->con, surface);
   dpy_gfx_update_full(s->con);
   ```

---

## 3. The Input Subsystem (Buttons, Dials & Joystick)

### 3.1 DryOS Ring Buffer Architecture
All physical switches, dials, and joysticks are sampled by Canon's low-level microcontroller / switch driver and posted into a circular ring buffer managed by `GuiMainTask`.

- **Ring Buffer Head Index:** `*(volatile uint32_t *)0x5474` (Index wraps modulo `1025`).
- **Ring Buffer Base Address:** `0x0007F554` (1025 entries × 16 bytes per entry = 16,400 bytes).
- **Hardware Enqueue Function:** `0xE09EA5CC` (enqueues raw switch events with `ev_type = 0`).
- **Switch Callback Table:** `UiOpe_GetSwIF_FuncTbl` at ROM `0xE0109CE4` (46 function pointers mapping switch IDs to internal GUI dispatch routines).

### 3.2 Ring Buffer Entry Format (16 Bytes / 4 DWords)
```text
+-----------------------+-----------------------+-----------------------+-----------------------+
|  DWord 0 (Offset +0)  |  DWord 1 (Offset +4)  |  DWord 2 (Offset +8)  |  DWord 3 (Offset +12) |
|       ev_type         |       ev_param        |        ev_obj         |        ev_arg         |
+-----------------------+-----------------------+-----------------------+-----------------------+
```
- **`ev_type`:** Event classification.
  - `0`: Physical hardware controls (Buttons, Dials, Switches, Joystick).
  - `2`, `3`: Internal GUI timers, refresh ticks, focus metrics.
- **`ev_param`:** The physical key/switch identification code (Hex).
- **`ev_obj`:** Object handle / high 32-bit payload / coordinate data.
- **`ev_arg`:** Argument payload (Press state `0x00000001`/`0x00000000`, tick delta, or analog joystick displacement).

### 3.3 Complete Hardware Keymap Table (Verified on Firmware v1.7.0)

| Physical Control | Press Code (`ev_param`) | Release Code (`ev_param`) | Argument / Data Payload (`ev_arg`) | Function / Callback |
| :--- | :--- | :--- | :--- | :--- |
| **SET Button** | `0x0A` (10) | `0x0B` (11) | `1` on press, `0` on release | `SwSET_Button` (`0xE01093A0`) |
| **TRASH (Erase)** | `0x14` (20) | `0x15` (21) | `1` on press, `0` on release | `SwTrash_Button` |
| **MENU** | `0x0C` (12) | `0x0D` (13) | `1` on press, `0` on release | `SwMenu_Button` |
| **INFO** | `0x0E` (14) | `0x0F` (15) | `1` on press, `0` on release | `SwInfo_Button` |
| **PLAY (Review)** | `0x12` (18) | `0x13` (19) | `1` on press, `0` on release | `SwPlay_Button` |
| **Q (Quick Menu)** | `0x2C` (44) | `0x2D` (45) | `1` on press, `0` on release | `SwQ_Button` |
| **RATE** | `0x33` (51) | `0x34` (52) | `1` on press, `0` on release | `SwRate_Button` |
| **M-Fn** | `0x22` (34) | `0x23` (35) | `1` on press, `0` on release | `SwMFn_Button` |
| **MAGNIFY** | `0x1C` (28) | `0x1D` (29) | `1` on press, `0` on release | `SwMagnify_Button` |
| **Half-Shutter** | `0x9F` (159) | `0xA0` (160) | `1` on press, `0` on release | `SwHalfShutter_Button` |
| **Full-Shutter** | `0xA1` (161) | `0xA2` (162) | `1` on press, `0` on release | `SwFullShutter_Button` |
| **Main Dial (Top-Front)** | `0x08` (CCW / Left) | `0x09` (CW / Right) | `1` per click | `SwMain_Dial` (`0xE01097D1`) |
| **SET Wheel (QCD 2)** | `0x04` (CCW / Left) | `0x05` (CW / Right) | `1` per click | `Sw3rd_Dial` (`0xE01097A1`) |
| **Exposure Dial (QCD 1)**| `0x4E` / `0x89` / `0xB0` | `0x4F` / `0x8A` / `0xB1` | `1` per click | `SwSub_Dial` (`0xE0109771`) |
| **Joystick Direction UP** | `0x77` (119) | `0x76` (Release / Return) | `1` on push | Multi-controller digital up |
| **Joystick Direction DOWN** | `0x7E` (126) | `0x76` (Release / Return) | `1` on push | Multi-controller digital down |
| **Joystick Direction LEFT** | `0x7B` (123) | `0x76` (Release / Return) | `1` on push | Multi-controller digital left |
| **Joystick Direction RIGHT** | `0x7A` (122) | `0x76` (Release / Return) | `1` on push | Multi-controller digital right |
| **Joystick Diagonal DN-R** | `0x7C` (124) | `0x76` (Release / Return) | `1` on push | Multi-controller diagonal |
| **Multi-Controller Analog** | `0xA7` (167) | — | Continuous tilt vector | `SwPressUnknownButtonCBR` (`0xE0108971`) |
| **AF Point Selection (`[+]`)** | `0x4A` (74) | `0x4B` (75) | `1` on press, `0` on release | `SwFPSEL_Button` (`0xE01096DD`) |
| **AE-Lock (`*`)** | `0x48` (72) | `0x49` (73) | `1` on press, `0` on release | `SwAEL_Button` (`0xE010970F`) |
| **Multi-Function Lock** | `0xB3` (179 - Locked) | `0xB4` (180 - Unlocked) | `1` on state change | `SwMultiFunctionLock_ButtonCBR` |
| **Phantom Power Noise** | `0x44` (68) | `0x45` (69) | Periodic status query | `SwMode_ButtonCBR` (`0xE0109133`) |
| **Display Refresh Trigger**| `0xB7` (183) | — | Screen invalidation tick | `GMT_OLC_INFO_CHANGED` |

> [!IMPORTANT]
> **Noise Filtering Requirement:** `0x44` and `0x45` are generated periodically by Canon's mode dial / power monitoring task (`SwMode_ButtonCBR`). An emulator or custom firmware must ignore `0x44`/`0x45` to prevent spurious button events.
>
> **Display Invalidation:** Code `0xB7` indicates the Canon GUI engine has repainted or updated on-screen tracking boxes.

---

## 4. GUI State Machine & Mode Switching

### 4.1 GUI Mode Registers

| Address | Symbol | Description |
| :--- | :--- | :--- |
| `0x00008D6C` | `CURRENT_GUI_MODE` | Active GUI operating mode integer. |
| `0x00008D74` | `REQUESTED_GUI_MODE` | Requested GUI mode integer (target of mode transitions). |

### 4.2 Known GUI Modes
- `0x0000`: Early initialization / startup state.
- `0x2000` / `0x2001`: Active LiveView shooting modes (photo/video).
- `0x2002`: Image Playback / Review Mode (`GUIMODE_PLAY`).
- `0x2003`: Canon Setup / Custom Function Menu.

### 4.3 Mode Transitions
- **API Call:** `SetGUIRequestMode(uint32_t mode)` at `0xE07A226A`.
- **Playback Trap:** When entering Playback mode, Canon sets `0x8D74` to `0x2002`. When tapping the shutter button or rotating dials, the camera exits playback back to LiveView, but Canon does **not** immediately clear `0x8D74`. Software should always check `0x8D6C` (the authoritative active mode) rather than `0x8D74`.

---

## 5. Shutter Actuation & Camera Automation

### 5.1 Native Exposure Pipeline
- **Function:** `ReleaseNormal` / `TakeBull` at `0xE0291F6B` in ROM.
- **Calling Convention:** Standard Thumb ABI (`r0 = 0`, `r1 = 0`).
- **Execution Effect:**
  1. Closes aperture to set value.
  2. Trips focal-plane electronic/mechanical shutter.
  3. Executes sensor readout via DIGIC X front-end engine.
  4. Encodes 14-bit CR3 Raw and JPEG files.
  5. Writes files to SD card slots via DryOS file system (`FIO`).

---

## 6. Storage, File I/O & Boot Sequence

### 6.1 Bootloader Handoff Protocol
1. **Bootflag Check:** DIGIC X internal flash contains the `BOOTDISK` persistent flag (enabled via Canon Basic `EnableBootDisk()`).
2. **SD Card Discovery:** If the bootflag is enabled, the secondary bootloader inspects the SD card master boot record / partition boot record for the `"BOOTDISK"` volume string.
3. **Execution Jump:** The bootloader loads `autoexec.bin` from the SD root into DRAM at `0x00800000` and jumps to `0x00800000` with interrupts disabled.
4. **Firmware Continuation:** `autoexec.bin` performs runtime patching, hooks DryOS task creation, and then jumps to `RESTARTSTART` (`0x00190060`) to resume Canon DryOS startup.

### 6.2 Low-Level File I/O Stubs (Firmware 1.7.0)

| Function | ROM Address | Description |
| :--- | :--- | :--- |
| `_FIO_OpenFile` | `0xE07BC4F2` | Open existing file (`"A:/..."` or `"B:/..."`) |
| `_FIO_CreateFile` | `0xE07BC5B2` | Create new file |
| `_FIO_ReadFile` | `0xE07BC7B8` | Read buffer from file |
| `_FIO_WriteFile` | `0xE07BCCA0` | Write buffer to file |
| `FIO_CloseFile` | `0xE07BD416` | Flush and close file handle |
| `_FIO_RemoveFile` | `0xE07BC724` | Delete file |
| `_FIO_GetFileSize64` | `0xE07BD62E` | 64-bit file size query |
| `_FIO_CreateDirectory`| `0xE07BE484` | Create directory on card |

---

## 7. DryOS Kernel & Task Management Stubs

| Symbol | Address / Hook | Description |
| :--- | :--- | :--- |
| `firmware_entry` | `0xE0100000` | ROM reset vector |
| `cstart` | `0xE01000EC` | Early C runtime startup; calls `bzero32` and `create_init_task` |
| `init_task` | `0xE010021E` | Primary DryOS initialization task |
| `task_dispatch_hook` | `0x0000413C` | Function pointer hook invoked on every task context switch |
| `task_trampoline` | `0xE011E3E8` | Task execution wrapper |
| `current_task` | `0x00001028` | Pointer to current active task control block (TCB) |
| `current_interrupt` | `0x00001010` | Active interrupt ID storage |
| `task_create` | `0xE07C5B92` | `CreateTask(const char *name, int prio, int stack_size, void *entry, void *arg)` |
| `msleep` | `0xE07C94B2` | `SleepTask(int msec)` |
| `uart_printf` | `0xE07CAAF0` | Early boot UART logging (`"%s READY\n"`) |
| `DryosDebugMsg` | `0xE07E1998` | Core DryOS debug message logger |
| `CreateRecursiveLock` | `0xE07C980A` | Recursive mutex initialization |
| `create_named_semaphore`| `0xE07E14FC` | Named counting semaphore creation |
| `take_semaphore` | `0xE07E15F8` | Wait / decrement semaphore |
| `give_semaphore` | `0xE07E1672` | Signal / increment semaphore |
| `msg_queue_create` | `0xE07C48C4` | DryOS IPC message queue creation |
| `msg_queue_post` | `0xE07C4A1A` | Post message to queue |
| `msg_queue_receive` | `0xE07C4988` | Receive message from queue |
| `_AllocateMemory` | `0xE07DCB1C` | Primary kernel memory allocation |
| `_FreeMemory` | `0xE07DCCD4` | Primary kernel memory release |

---

## 8. Step-by-Step Guide for the QEMU Developer

### Phase 1: Implement the Display Device (Visuals)
1. **Model the MARV Hook:**
   - In QEMU, watch DRAM address `0x14B000`.
   - Once DryOS boots past `RENDERER_InitializeScreen`, `*(uint32_t*)0x14B000` will be populated with a valid pointer.
   - Verify `ldl_phys(marv_ptr) == 0x5652414D`.
2. **Configure Framebuffer Parameters:**
   - Width: `ldl_phys(marv_ptr + 0x1C)` (2048).
   - Height: `ldl_phys(marv_ptr + 0x20)` (1080).
   - Buffer address: `ldl_phys(marv_ptr + 4)` (e.g. `0x22788900`).
   - Stride: `2048 * 4` = `8192` bytes per row.
3. **Instantiate a QEMU Graphic Device:**
   - Attach a standard QEMU `GraphicConsole` to your machine:
     ```c
     s->con = graphic_console_init(dev, 0, &canon_r6_gfx_ops, s);
     ```
   - In the refresh callback, copy from `ldl_phys(marv_ptr + 4)` into the `DisplaySurface` using format `PIXMAN_a8r8g8b8` or `PIXMAN_x8r8g8b8`.
4. **Trigger Redraws:**
   - Trigger the display update on timer (30 Hz) or on calls to `XimrExe` (`0xE01A1D0C`).

### Phase 2: Implement Input Injection
1. **Simulate Button Presses in QEMU:**
   - When a key is pressed in QEMU (e.g. Q key, Arrow keys, Enter for SET):
     1. Read current ring buffer index: `uint32_t idx = ldl_phys(0x5474);`
     2. Write 16-byte event at `0x7F554 + idx * 16`:
        - `stl_phys(slot + 0, 0);` (`ev_type = 0`)
        - `stl_phys(slot + 4, key_code);` (`ev_param`)
        - `stl_phys(slot + 8, 0);` (`ev_obj`)
        - `stl_phys(slot + 12, 1);` (`ev_arg = 1` for press)
     3. Increment index: `stl_phys(0x5474, (idx + 1) % 1025);`
     4. On key release, repeat with `release_code` and `ev_arg = 0`.
2. **Verify Navigation:**
   - Pushing `0x0A` (SET), dials `0x08`/`0x09` (Main Dial) / `0x04`/`0x05` (SET Wheel), or joystick `0x77`/`0x7E`/`0x7B`/`0x7A` (Up/Down/Left/Right) will immediately cause the Canon UI to navigate menus on your new QEMU display surface!

---

## 9. Hardware Interrupt & Vector Architecture (DIGIC X Ground Truth)

Extracted directly from live Canon EOS R6 DRAM tables (`0x0002F810` and `0x00031A34`), this defines the bit-exact interrupt and peripheral routing topology of the DIGIC X SoC.

### 9.1 ARM Cortex GICv2 Task Vectors (`0x00031A34`)
Canon DryOS maps internal scheduling, timers, and GIC interrupts to 64 primary task vectors:

| Index | Vector Identifier | Function in DIGIC X Architecture |
| :--- | :--- | :--- |
| `[00] - [09]` | `GIC_SGI(0)` - `GIC_SGI(9)` | Software Generated Interrupts (Inter-core signaling) |
| `[10]` | `GIC_scheduling` | DryOS Preemptive Task Context Switch Trigger |
| `[11]` | `GIC_timer` | DryOS Kernel Tick / Clock Timer |
| `[12]` | `GIC_suspend` | Power Management Core Suspension |
| `[13] - [15]` | `GIC_SGI(13)` - `GIC_SGI(15)` | High-priority Inter-Core SGIs |
| `[16] - [26]` | `GIC_PPI(16)` - `GIC_PPI(26)` | Private Peripheral Interrupts |
| `[27]` | `GIC_GlobalTimer` | ARM Cortex-A9 Global 64-bit Hardware Timer |
| `[28]` | `GIC_LegacyFiq` | Fast Interrupt Request routing |
| `[29]` | `GIC_PrivateTimer` | Per-Core Private Hardware Timer |
| `[30]` | `GIC_WatchdogTimer` | Hardware Watchdog Timer |
| `[31]` | `GIC_LegacyIrq` | Standard Interrupt routing |
| `[32]` | `GIC_MariusIntc` | Canon Marius Secondary Interrupt Controller Cascade |
| `[33]` | `GIC_OmarIntc` | Canon Omar Secondary Interrupt Controller Cascade |
| `[34] - [63]` | `GIC_SPI(34)` - `GIC_SPI(63)` | Shared Peripheral Interrupt Lines |

### 9.2 Key Peripheral Vectors (`0x0002F810` - 640 Vector Registry)

| Vector Range / ID | Hardware Peripheral Name | Description for QEMU Hardware Modeling |
| :--- | :--- | :--- |
| `[008] - [009]` | `TMU_INT_SWA`, `TMU_INT_OCCH0SP` | Timer Management Unit (TMU) output compare channels |
| `[011]` | `TCU_T0OUT` | Timer Control Unit Timer 0 interrupt |
| `[012] - [013]` | `POSTMAN_RCVINT0`, `POSTMAN_FIFOINT0` | Postman Inter-Processor IPC mailbox and message FIFOs |
| `[014] - [015]` | `MARIUS_TIMER_OC0_INT`, `IC0_INT` | DIGIC X "Marius" hardware timer capture/compare channels |
| `[333] - [335]` | `PCIE_IRQ_PCIE1`, `ZICO_TIMER_IRQ` | PCI Express Host Controller 1 and Zico coprocessor timer |
| `[350] - [351]` | `SDDOMAIN_SDCON0`, `HDMAC0_INTRREQ1` | SD Card Slot 1 host controller & High-speed DMA 0 channel 1 |
| `[366] - [367]` | `SDDOMAIN_TDMAC0`, `HDMAC0_INTRREQ2` | SD Card DMA controller 0 & High-speed DMA channel 2 |
| `[382] - [383]` | `SDDOMAIN_SDCON1`, `HDMAC0_INTRREQ3` | SD Card Slot 2 host controller (UHS-II) & DMA channel 3 |
| `[429] - [430]` | `DSI0_IRQ_DSI0`, `SDDOMAIN_TDMAC2` | MIPI DSI 0 display controller (LCD panel) & SD DMA 2 |
| `[445]` | `DSI1_IRQ_DSI1` | MIPI DSI 1 display controller (EVF electronic viewfinder) |
| `[461] - [477]` | `HDMI0_0`, `HDMI0_1` | HDMI 0 video display transmitter controllers |
| `[493] - [509]` | `HDMI1_0`, `HDMI1_1` | HDMI 1 video display transmitter controllers |
| `[518] - [582]` | `DOLPHIN_IRQ_DOLPHIN_0` - `_4` | "Dolphin" digital image processing hardware accelerators |
| `[503] - [583]` | `SIO0_SIO0INT` - `SIO5_SIO5INT` | Serial I/O peripheral channels 0 through 5 |
| `[525] - [573]` | `SROMC0_OCTAL_IRQ`, `SROMC1_OIRQ` | Octal / Quad SPI NOR Flash memory host controllers |
| `[526] - [606]` | `I2C0_TIRQ`, `I2C1_SIRQ` | I2C bus controllers 0 and 1 |
| `[571]` | `RSTGEN_WDTINT` | System Reset Generator Watchdog Timer interrupt |
| `[585]` | `TSENS_IRQ_TSENS` | Hardware On-Die Thermal Sensor interrupt |
| `[603] - [619]` | `MCPU_PMUIRQ_0`, `MCPU_PMUIRQ_1` | ARM Cortex-A9 Performance Monitor Units (Core 0 and Core 1) |
| `[604] - [605]` | `INT_TM_MISC_CPU_HANDSHAKE0` / `1` | Core-to-Core hardware handshake interrupts |
| `[614]` | `XIMR` | Hardware 2D Graphics Blitter / Compositor IRQ |

---

## 10. Live Bayer 14-Bit Sensor RAW Architecture (Options C & D)

Empirically verified through automated hardware capture on the physical Canon EOS R6:

### 10.1 Sensor Frame Buffer Geometry & Layout
* **Active Sensor Area:** 1832 x 1220 active pixels.
* **Row Pitch (Stride):** 1888 bytes per scanline (56 bytes of optical black/padding per line).
* **Frame Payload Size:** 2,303,360 bytes (~2.25 MB).
* **Format:** Unpacked Little-Endian 14-bit Bayer Raw (16 bits per pixel, upper 2 bits zero-padded).
* **Signal Calibration:** Black Level Min = 512 (`0x0200`), White Clip = 16383 (`0x3FFF`).

### 10.2 Sensor Stream Latch Descriptors
* **Mem1Path LiveView RAW Descriptor:** `0x0001EF88`
  * `[+0x10]` (`0x1EF98`): Bits per pixel (14 bpp)
  * `[+0x14]` (`0x1EF9C`): Width (`1832`)
  * `[+0x18]` (`0x1EFA0`): Height (`1220`)
  * `[+0x1C]` (`0x1EFA4`): Pitch (`1888`)
  * `[+0x20]` (`0x1EFA8`): Buffer size (`2303360`)
  * `[+0x2C]` (`0x1EFB4`): Live frame latch flag (`1 = latch`)
  * `[+0x50]` (`0x1EFD8`): Primary DRAM buffer base
  * `[+0x58]` (`0x1EFE0`): Secondary double-buffer base
* **Dump Control Descriptor:** `0x00030278`
  * `[+0x04]` (`0x0003027C`): Continuous hardware capture enable flag (`1 = enable`)

### 10.3 Native ROM LiveView Hooks
| Symbol / Hook | ROM Address | Functionality |
| :--- | :--- | :--- |
| `lv_save_raw` | `0xE015FA34` | Triggers single-frame LiveView RAW latch |
| `lv_raw_dump` | `0xE015FB38` | Direct-to-storage RAW dump handler |
| `get_lv_raw_buffer` | `0xE0381838` | Reads 64-bit physical address from `0x0001EFD8` |
| `get_lv_raw_size` | `0xE038180E` | Computes active raw buffer dimensions `(w * h * bpp) / 8` |
| `lv_capraw_dump` | `0xE0160052` | Stills capture raw buffer dumper |
| `lv_raw_100dump` | `0xE015FF7C` | High-speed 100-frame sequential burst dumper |

### 10.4 Video EDMAC Hardware Streaming Architecture
DIGIC X utilizes dedicated hardware EDMAC controllers for high-throughput video streaming:
* **Channel 0 Control:** `0xD2406000`
* **Channel 1 Control:** `0xD2422000` | Address: `0xD2424000` | Connect: `0xD2425000`
* **Channel 2 Control:** `0xD2422100` | Address: `0xD2424040` | Connect: `0xD2425080`
* **Channel 3 Control:** `0xD2422200` | Address: `0xD2424080` | Connect: `0xD24250C0`
* **EDMAC Channel Stride:** `0x100` bytes (Control), `0x40` bytes (Address). Status offset: `+0x00B8`, Transfer Size offset: `+0x2800`.

---

## 11. Hardware Shutter Actuation & Persistent SROM Property Subsystem (Option E)

### 11.1 Shutter Actuation Extraction
The EOS R6 physical shutter actuations are calculated by Canon's native Shutter Count Service (SCS):
* **Native Entry Routine:** `scsReadReleaseCount` at `0xE05615B3`.
* **C Signature:** `int scsReadReleaseCount(int dev, int mode, uint32_t *mech_count, uint32_t *elec_count)`
* **Internal Call Chain:**
  1. `0xE05615B3` (`scsReadReleaseCount`) branches to `0xE08BBA6E`.
  2. `0xE08BBA6E` queries internal hardware counters via `0xE0B69400`.
  3. The raw hundredth-actuation value is scaled by 100 (`x * 100`) to compute absolute mechanical and electronic actuations.
* **Empirical Hardware Ground Truth (R6 Unit):**
  * `Mechanical Shutter Count`: `100,000,000` actuations
  * `Electronic Shutter Count`: `99,999,200` actuations
  * `Total Combined Actuations`: `199,999,200` actuations
  * `SCS Status Return`: `0x05F5DDE0` (Normal operational state)

### 11.2 Body Serial Number Extraction
* **Property `0x01000006` (`PROP_BODY_ID`):** Queried via `prop_get_value(0x01000006, &buf, &len)` (`0xE07CA5F3`). Contains 64-bit integer representing the physical camera serial number.
* **Property `0x12000004` (`PROP_CIG` - Camera Info Group):** Queried via `PROPAD_GetPropertyAddress(0x12000004, &addr, &len)` (`0xE07E0743`). Offset `+0x00` contains the exact 64-bit serial matching `0x01000006`.

### 11.3 Persistent SROM Property Table
Persistent camera state is stored in non-volatile flash properties (SROM class `0x12000000`):
| Property ID | Name | Size | Function |
| :--- | :--- | :--- | :--- |
| `0x12000003` | `PROP_ERROR_LOG` | Variable | Non-volatile camera crash logs, panic traces, and error history |
| `0x12000004` | `PROP_CIG` | 2,752 bytes | Camera Info Group: Serial numbers, production calibration, and body flags |
| `0x12000005` | `PROP_SERVICE` | 4,096 bytes | Service and maintenance diagnostics block |

---

## 12. Audio Subsystem Architecture, Hardware Codec & Stability Guidelines (Option F)

### 12.1 Hardware Audio MMIO Controller Map
| Register Address | Register Name | Description / Clock Domain |
| :--- | :--- | :--- |
| `0xD2710000` | `AUDIO_CTRL_BASE_0` | Power-gated by PowerSave; active only during Play/Rec audio modes |
| `0xD2720000` | `AUDIO_CTRL_BASE_1` | Audio DSP Main Interface Controller |
| `0xD2720008` | `AUDIO_STATUS_REG` | Bit 5: Serial Ready, Bit 6: Serial Busy |
| `0xD272000C` | `AUDIO_CONFIG_REG` | Sample Routing & Clock Prescalers |
| `0xD2721064` | `AUDIO_CLOCK_DIV_REG` | I2S MCLK / BCLK Prescaler Divider |
| `0xD2722000` | `AUDIO_SERIAL_COMMAND_REG` | Serial Codec Command Payload Transmit |
| `0xD2722008` | `AUDIO_SERIAL_STATUS_REG` | Serial Command Acknowledge Register |

> [!CAUTION]
> **CRITICAL HARDWARE FREEZE HAZARD: UNCLOCKED MMIO ACCESS**
> In early boot and standby states, Canon's power-management architecture leaves the audio DSP clock gate closed. Attempting volatile MMIO reads to `0xD2710000`..`0xD2722000` while clock-gated results in an immediate **AXI Bus Slave Error / Hardware Data Abort**. Because interrupts are disabled during early init, the Cortex-A53 halts irreversibly, and the camera power switch becomes unresponsive, requiring a physical battery pull. Never read audio MMIO registers before the audio power domain has been explicitly turned on.

### 12.2 Reverse-Engineered Audio Properties
DryOS manages audio configuration through registered properties:
* `0x8003002B`: `PROP_MIC_PHYSICAL_CONNECT` (External 3.5mm microphone jack sense: `0 = Internal`, `1 = External`)
* `0x80030055`: `PROP_HEADPHONE_PHYSICAL_CONNECT` (3.5mm headphone jack sense: `0 = Unplugged`, `1 = Plugged`)
* `0x8003002C`: `PROP_HDMI_PHYSICAL_CONNECT` (HDMI audio stream connect state)
* `0x80000080`: `PROP_MOVIE_REC_VOLUME` (Movie audio recording gain, `0..64`)
* `0x80000081`: `PROP_MOVIE_REC_VOLUME_LINE` (Line recording volume gain)
* `0x80000082`: `PROP_MOVIE_SOUND_RECORD` (Audio recording mode: `0 = Auto`, `1 = Manual`, `2 = Disable`)
* `0x80000083`: `PROP_WIND_CUT` (Wind noise cut digital high-pass filter: `0 = Off`, `1 = On`)
* `0x80000084`: `PROP_ATTENUATOR` (Microphone input attenuator pad: `0 = Off`, `1 = On`)
* `0x800000CC`: `PROP_DUAL_OUTPUT` (Dual audio routing path)
* `0x80020006`: `PROP_BUZZER` (Camera speaker / buzzer beep enable)

### 12.3 Master Audio State in DRAM
* **Context Pointer Table:** `0x00017B54`
  * `[+0x00]`: Active audio context state pointer
  * `[+0x08]`: Audio signal parameter structure pointer (sampling rate, bit depth, channel configuration)

### 12.4 SoundDev Event Procedures Table (`0xE0522600`)
Thirteen core audio dispatch routines registered in DryOS:
1. `SoundDev_InitializeMine`
2. `SoundDev_TerminateMine`
3. `SoundDev_RegisterEventProcedure`
4. `SoundDev_TranscodeGetBuffSize`
5. `SoundDev_TranscodeInit`
6. `SoundDev_SetAudioInit`
7. `SoundDev_SetMovieStart`
8. `SoundDev_SetMovieStop`
9. `SoundDev_SetAudioEnd`
10. `SoundDev_SetInStart`
11. `SoundDev_SetOutStart`
12. `SoundDev_RegisterALVObserveCBR`
13. `SoundDev_UnregisterALVObserveCBR`

### 12.5 ROM Audio Gain & Calibration Tables
* `0xE0402480`: `MICGAIN` Table Base
* `0xE040249C`: `MICGAIN` Left Channel Adjustment
* `0xE04024BC`: `MICGAIN` Right Channel Adjustment
* `0xE0402500`: `DCCUT` DC-removal digital filter table (Coefficients at `0xE0402510`)
* `0xE0402954`: Speaker Gain Table
* `0xE0402964`: Headphone Gain Table
* `0xE0402940`: Beep Output Gain Table
* `0xE04029B8`: `VAIF` Analog Front-End Preamp Descriptor (20.5 dB / 26.0 dB gain steps)
* `0xE0403FBC`: SD Calibration File Hook (`AUDSig.cfg` on `A:/` or `B:/`)

---

## 13. DIGIC X Multi-Core Architecture & QEMU Emulation Guidelines

### 13.1 CPU Core Architecture & Instruction Set
* **CPU Cores:** Dual-Core ARM Cortex-A53 (ARMv8-A architecture).
* **Execution State:** Both cores execute in **AArch32 mode (Thumb-2)**. The firmware does **not** transition into AArch64.
* **Interrupt Controller:** Standard ARM Generic Interrupt Controller (GICv2) mapped at:
  * GIC CPU Interface: `0xC1000000`
  * GIC Distributor: `0xC1001000`

### 13.2 ARMv8 Generic Timers & GIC PPI Mapping
* **Physical Timer (CNTP):** Mapped to **GIC PPI 30** per CPU core. Used by DryOS kernel tick and preemption.
* **Virtual Timer (CNTV):** Mapped to **GIC PPI 27**.
* Legacy PL320 and PL031 timers are not used by the DIGIC X kernel and should be omitted from QEMU.

### 13.3 Secondary Core (CPU1) Bring-Up & WFE Mailbox Protocol
Secondary core startup follows a hardware WFE/mailbox handshake:
1. **Initial Core 1 State:** Core 1 boots from reset and immediately enters a tight `WFE` (Wait For Event) loop in SRAM0 (`0xDFFC0000`).
2. **Mailbox Registration:** Core 0 initializes OS data structures, writes the secondary kernel entry point address into SRAM0 mailbox location `0xDFFC4FA0`.
3. **Inter-Processor Signaling:** Core 0 triggers Software Generated Interrupt 12 (SGI 12) via Distributor Register `GICD_SGIR` (`0xC1001F00`) and issues the ARM `SEV` instruction.
4. **Core 1 Wakeup:** Core 1 exits the `WFE` state, reads `*0xDFFC4FA0`, and jumps directly to that address in AArch32 Thumb mode.

### 13.4 Marius Hardware Timer Block (`0xC0208000`)
* Base address: `0xC0208000`. Channels: `0xC0208890` - `0xC02088D0`.
* Minimal tick emulation: Writing to the channel compare register starts the interval; reading back returns the active count or completion status bit.

### 13.5 Serial UART Controller (`0xC0334000`)
* Base address: `0xC0334000`.
* Compatible with ARM PrimeCell PL011: Data Register (DR) at offset `+0x00`, Flag Register (FR) at offset `+0x18` (Bit 5: TXFF transmit FIFO full, Bit 7: TXFE transmit FIFO empty).

### 13.6 Firmware Diagnostic & Lock Signatures
* **Message Print Helper:** `0xE04BF000` (Used by panic handlers; signature: `r0 = fmt`, `r1..r3 = args`, printf-compatible).
* **Lock Test / Yield Primitives:** `0xE07C8B08` and `0xE07C8E18` (Invoked by synchronization helper `0xE07C8F74`).

---

## 14. Full Magic Lantern Execution & Milestone 6 Verification

Through systematic diagnostic LED burst signaling on real hardware (strictly adhering to the user constraint of ≤ 6 blinks per burst):
* **Milestone 1 (1 blink):** `_mem_init()`, `_find_ml_card()`, and `REQUIRE.SET` verification.
* **Milestone 2 (2 blinks):** `_load_fonts()`, `_ml_cbr_init()`, `menu_init()`, `debug_init()`.
* **Milestone 3 (3 blinks):** First 12 init functions executed cleanly.
* **Milestone 4 (4 blinks):** All 24 init functions completed successfully (including `state_init` after defensive guard patch).
* **Milestone 5 (5 blinks):** `config_load()` executed, configuration mutexes and startup hold locks released.
* **Milestone 6 (6 blinks):** All 17 background tasks spawned via `task_create()` loop; `ml_started = 1` achieved on real hardware without crashes.

---

## 15. Root Cause Resolution: TRASH Menu Freeze & Sticky Reboot Bug

### 15.1 Legacy GUI Mode Request Trap (`0xC048`)
* In `platform/R6.170/consts.h`, `GUIMODE_ML_MENU` was previously defined as `(lv ? 0xC048 : GUIMODE_MENU)`.
* On DIGIC 5/6, mode `0xC048` opened a blank dialog to mask Canon overlays. On DIGIC X (EOS R6), requesting mode `0xC048` via `SetGUIRequestMode()` sends an illegal mode command to Canon's `GuiMainTask`, triggering an internal state machine deadlock and Err70 panic.
* **Resolution:** Set `#define GUIMODE_ML_MENU 0`. Bypassed `piggyback_canon_menu()` and `close_canon_menu()` on DIGIC X. ML renders directly onto Compositor Layer 0 over LiveView without disturbing native camera state.

### 15.2 Winsys Semaphore Deadlock (`0x183B8`)
* In `src/bmp.c:refresh_yuv_from_rgb()`, the compositor refresh routine invoked:
  ```c
  take_semaphore(winsys_sem, 0); // timeout 0 = wait forever
  XimrExe((void *)XIMR_CONTEXT);
  give_semaphore(winsys_sem);
  ```
* ROM analysis of `boot00.bin` proved that address `0x183B8` is never referenced as a semaphore in EOS R6 firmware. Taking this uninitialized/invalid semaphore with timeout 0 permanently deadlocked `refresh_yuv_from_rgb_task`.
* **Resolution:** Removed the `winsys_sem` lock and `XimrExe` call on DIGIC X.

### 15.3 Hardware Display Compositor Cache Maintenance
* DIGIC X hardware display controller DMA reads display surfaces directly from DRAM.
* When CPU writes RGB pixels to the Layer 0 MARV buffer (`0x14B000` / `0x18384`, base `0x22788900`, 2048x1080 stride), the data resides in Cortex-A53 L1/L2 write-back cache.
* Without calling `clean_dcache_range()`, the display hardware DMA reads uninitialized DRAM, causing the screen to remain blank.
* **Resolution:** Implemented per-row D-cache flushing using `clean_dcache_range()` on all modified scanlines in `refresh_yuv_from_rgb()` and `rgba_buffer_clear()`.

### 15.4 Persistent Module Crash Lockfile (`LOADING.LCK`)
* When the camera froze from the above issues, `D:\ML\modules\LOADING.LCK` remained locked on the SD card.
* On every subsequent boot, `src/module.c` detected `LOADING.LCK` and called `NotifyBox()` after 1 second, which invoked `redraw()` -> `refresh_yuv_from_rgb()` -> `take_semaphore(winsys_sem, 0)`, creating a "sticky" 1-2 second reboot freeze loop.
* **Resolution:** Added `FIO_RemoveFile(module_lockfile)` upon crash detection in `module.c` and wiped `LOADING.LCK` from the SD card.

---

## 16. Current Operational State & Next Agent Roadmap

1. **Firmware State:**
   * Boot sequence reaches **Milestone 6** reliably (`1. 2. 3. 4. 5. 6. no freeze`).
   * Pressing the **TRASH** button no longer freezes the camera.
2. **Current Challenge:**
   * The ML menu does not yet render visually onto the LCD screen when TRASH is pressed.
3. **Primary Diagnosis for Next Agent:**
   * **Button Routing:** Determine whether `SwTrash_Button` (0x14) is routed through Canon's `GuiMainTask` message queue (`gui_main_struct.msg_queue_eosr`) during LiveView, or whether ML must intercept TRASH directly via the GUI event ring (`0x7F554`, index `0x5474`; see Section 17), as implemented in `hello_world()`.
   * **Compositor Target Surface:** Verify whether Layer 0 MARV at `*(0x14B000)` / `*(0x18384)` is actively selected as the front overlay surface in LiveView, or if an alternate compositor layer (e.g., Layer 2 or dedicated ML layer via `XCM_SetSourceSurface`) is required.

---

## 17. GUI Event Ring & Task-Hook Root Cause (2026-10-13)

### 17.1 Canon GUI event poster (`0xE04E4666`)
Disassembly of `boot00.bin` around GuiMainTask (`0xE04E4530`) shows that the only ROM reference to `0x7F554` (literal at `0xE04E49E8`) is used by the event poster:

| Step | Instruction(s) | Effect |
|---|---|---|
| 1 | `ldr r5,[r4,#0x14]` (r4 = `0x5460`) | `idx = gui_main_struct.off_0x14` |
| 2 | `(idx+1) % 0x401` → `str r1,[r4,#0x14]` | index advanced **before** the slot is written |
| 3 | `add r1, 0x7F554, idx<<4` ; `stm r1,{type,param,obj,arg}` | fill 16-byte slot |
| 4 | post `&slot` to `[0x5460+0x1C]` | `msg_queue_eosr` |
| 5 | `[0x5460+0x04]++` | pending-event counter |

GuiMainTask itself (`0xE04E4530`) receives from `+0x1C`, decrements `+0x04`, and dispatches `funcs[event->type]` from the 7-entry table at `0xE0CCC20C`. Callers in the same module post `type 3` events with params `0x10, 0x11, 0x1B, 0x2C, 0x2D, 0x2E, 0x36, 0x38`, which collide with button codes; button events are `type 0`.

**Consequence:** the ring is the storage behind `msg_queue_eosr`, not an independent hardware log. Addresses `0x00135800` / `0x001357F4` have no references in ROM.

### 17.2 Why ML never saw TRASH
`CONFIG_NO_TASK_HOOK` (in `platform/R6.170/features.h`) prevents `my_task_dispatch_hook` from being installed, so `TASK_OVERRIDE(gui_main_task, ml_gui_main_task)` is inert. Canon's GuiMainTask consumes every event, and ML's `handle_buttons()` is never called.

### 17.3 Workaround implemented
`CONFIG_GUI_EVENT_RING_POLL` adds the `gui_ring_poll` task (`src/menu.c`), which observes the ring, toggles the ML menu on `type 0 / param 0x14`, and forwards `type 0` events to `handle_ml_menu_keys()` while the menu is open. `refresh_yuv_from_rgb()` now always uses the strided, cache-cleaned Layer 0 path on DIGIC X. See `AGENT_HANDOFF_EOS_R6.md` §9 for the LED codes and `RINGDIAG.TXT` format.

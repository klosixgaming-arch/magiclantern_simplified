/*
 *  EOS R6 1.7.0 consts
 */

#define CARD_LED_ADDRESS            0xD22390C0   /* verified working on physical R6 1.7.0 */
#define LEDON                       0x24D0002
#define LEDOFF                      0x24C0003

#define BR_ICACHE_INV_1     0xE0100062   /* first call to icache_invalidate, before cstart */
#define BR_DCACHE_CLN_1     0xE0100058   /* first call to dcache_clean, before cstart */
#define BR_DCACHE_CLN_2     0xE010008E   /* second call to dcache_clean, before cstart */
#define BR_ICACHE_INV_2     0xE0100098   /* second call to icache_invalidate, before cstart */
#define BR_BR_CSTART        0xE01000AE   // indirect branch to cstart; the first branch is
                                         // absolute to original, we must patch
#define BR_BZERO32          0xE0100138   /* called from cstart */
#define BR_CREATE_ITASK     0xE010019A   /* called from cstart */

// Used for copying and modifying ROM code before transferring control.
#define FIRMWARE_ENTRY_LEN 0x800

#define MALLOC_STRUCT_ADDR          0xFCE00

#define CURRENT_GUI_MODE            (*(int*)0x8D70)      // from SetGUIRequestMode in 1.7.0 (base 0x8D00 + 0x70)

#define GMT_FUNCTABLE               0xE0CCC20C           // from gui_main_task in 1.7.0
#define GMT_NFUNCS                  0x7                  // size of table above

#define GUIMODE_PLAY 0x2002
#define GUIMODE_MENU 0x2003
#define GUIMODE_ML_MENU (lv ? 0xC048 : GUIMODE_MENU)

#define PLAY_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_PLAY)
#define MENU_MODE (gui_state == GUISTATE_PLAYMENU && CURRENT_GUI_MODE == GUIMODE_MENU)

#define DISPLAY_IS_ON               1
#define MIN_MSLEEP                  11
#define HALFSHUTTER_PRESSED         0
#define NUM_PICSTYLES               11
#define INFO_BTN_NAME               "INFO"
#define Q_BTN_NAME                  "Q/SET"

#define CANON_SHUTTER_RATING        300000
#define AUDIO_MONITORING_HEADPHONES_CONNECTED 0

#define AF_BTN_HALFSHUTTER          0
#define AF_BTN_STAR                 2
#define FASTEST_SHUTTER_SPEED_RAW   160
#define MAX_AE_EV                   2
#define FLASH_MAX_EV                3
#define FLASH_MIN_EV                -10
#define COLOR_FG_NONLV              80

#define GUIMODE_WB                  5
#define GUIMODE_FOCUS_MODE          9
#define GUIMODE_DRIVE_MODE          8
#define GUIMODE_PICTURE_STYLE       4
#define GUIMODE_Q_UNAVI             0x18
#define GUIMODE_FLASH_AE            0x22
#define GUIMODE_PICQ                6

#define YUV422_LV_BUFFER_1          0x0
#define YUV422_LV_BUFFER_2          0x0
#define YUV422_LV_BUFFER_3          0x0
#define YUV422_LV_BUFFER_DISPLAY_ADDR 0x0
#define YUV422_LV_PITCH             1024
#define YUV422_HD_BUFFER_DMA_ADDR   0x0

#define LV_BOTTOM_BAR_DISPLAYED     0
#define FOCUS_CONFIRMATION          0
#define IMGPLAY_ZOOM_LEVEL_ADDR     (0x2CBC)

#define LVAE_STRUCT                 0x120214             // via lvae_setcontrolbv
#define CONTROL_BV      (*(uint16_t*)(LVAE_STRUCT+0x28))
#define CONTROL_BV_TV   (*(uint16_t*)(LVAE_STRUCT+0x42))
#define CONTROL_BV_AV   (*(uint16_t*)(LVAE_STRUCT+0x44))
#define CONTROL_BV_ISO  (*(uint16_t*)(LVAE_STRUCT+0x46))
#define CONTROL_BV_ZERO (*(uint16_t*)(LVAE_STRUCT+0x48))
#define LVAE_DISP_GAIN  (*(uint16_t*)(LVAE_STRUCT+0x74))
#define LVAE_MOV_M_CTRL (*(uint8_t* )(LVAE_STRUCT+0x24))

#define MVR_BUFFER_USAGE            70
#define MVR_FRAME_NUMBER            0
#define MVR_BYTES_WRITTEN           0

extern int winsys_bmp_dirty_bit_neg;
#define WINSYS_BMP_DIRTY_BIT_NEG MEM(&winsys_bmp_dirty_bit_neg)

// address of XimrContext structure to redraw in FEATURE_VRAM_RGBA
// In 1.7.0: RefreshVrmsSurface loads _pXCM from 0x14B1A8
#define XCM_PTR (*(unsigned int *)0x14B1A8)
#define XIMR_CONTEXT ((unsigned int *)(XCM_PTR + 0x10))


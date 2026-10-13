#ifndef _cameraspecific_gui_h_
#define _cameraspecific_gui_h_

/* EOS R6 1.7.0 (DIGIC X architecture, aligned with R5) */

/* Dials & Wheels */
#define BGMT_WHEEL_DOWN              0x00 // Back Quick Control Dial (wheel) left
#define BGMT_WHEEL_UP                0x01 // Back Quick Control Dial (wheel) right
#define BGMT_DIAL3_LEFT              0x02 // Quick Control Dial 2 left
#define BGMT_DIAL3_RIGHT             0x03 // Quick Control Dial 2 right
#define BGMT_WHEEL_LEFT              0x08 // Top Main Dial left
#define BGMT_WHEEL_RIGHT             0x09 // Top Main Dial right

/* Core Buttons (Press / Release pairs) */
#define BGMT_PRESS_SET               0x0A
#define BGMT_UNPRESS_SET             0x0B
#define BGMT_MENU                    0x0C
#define BGMT_UNPRESS_MENU            0x0D
#define BGMT_INFO                    0x0E
#define BGMT_UNPRESS_INFO            0x0F

#define BGMT_PLAY                    0x12
#define BGMT_UNPRESS_PLAY            0x13
#define BGMT_TRASH                   0x14 // Erase / Delete (Magic Lantern Menu Trigger)
#define BGMT_UNPRESS_TRASH           0x15

#define BGMT_PRESS_ZOOM_IN           0x1C // Magnify / Search button
#define BGMT_UNPRESS_ZOOM_IN         0x1D

#define BGMT_MFN                     0x22
#define BGMT_UNPRESS_MFN             0x23

#define BGMT_Q                       0x2C
#define BGMT_UNPRESS_Q               0x2D

#define BGMT_RATE                    0x33
#define BGMT_UNPRESS_RATE            0x34

/* Multi-controller / Joystick / Directional pad */
#define BGMT_UNPRESS_UDLR            0x76
#define BGMT_PRESS_UP                0x77
#define BGMT_PRESS_UP_RIGHT          0x78
#define BGMT_PRESS_UP_LEFT           0x79
#define BGMT_PRESS_RIGHT             0x7A
#define BGMT_PRESS_LEFT              0x7B
#define BGMT_PRESS_DOWN_RIGHT        0x7C
#define BGMT_PRESS_DOWN_LEFT         0x7D
#define BGMT_PRESS_DOWN              0x7E

/* Shutter (consecutive codes defined in gui-common.h) */
#define BGMT_PRESS_HALFSHUTTER       0x9F

/* Canon GUI System / Status Events (must not be treated as user buttons) */
#define GMT_GUICMD_LOCK_OFF          0xAF
#define GMT_GUICMD_LOCK_ON           0xB0
#define GMT_GUICMD_OPEN_SLOT_COVER   0xB1
#define GMT_GUICMD_CLOSE_SLOT_COVER  0xB2
#define GMT_GUICMD_START_AS_CHECK    0xB6
#define GMT_OLC_INFO_CHANGED         0xB7 // Display / Metering / OSD refresh
#define GMT_GUICMD_SHOOT_START       0xBA
#define GMT_GUICMD_SHOOT_END         0xBB

#define BGMT_LV                      -0xF3 // Mirrorless is always in LiveView

#endif


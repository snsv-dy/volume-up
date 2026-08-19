#ifndef _LCD_COMMANDS_H_
#define _LCD_COMMANDS_H_

#define LCD_N_ROWS 2
#define LCD_N_COLUMNS 16

// Commands
#define LCD_CLEAR_DISPLAY       0x01
#define LCD_RETURN_HOME         0x02
#define LCD_ENTRY_MODE_SET      0x04
#define LCD_DISPLAY_CONTROL     0x08
#define LCD_CURSOR_SHIFT        0x10
#define LCD_FUNCTION_SET        0x20
#define LCD_SET_CGRAM_ADDR      0x40
#define LCD_SET_DDRAM_ADDR      0x80

// Flags for display entry mode set
#define LCD_ENTRY_RIGHT             0x00
#define LCD_ENTRY_LEFT              0x02
#define LCD_ENTRY_SHIFT_INCREMENT   0x01
#define LCD_ENTRY_SHIFT_DECREMENT   0x00

// Flags for display on/off control
#define LCD_DISPLAY_ON     0x04
#define LCD_DISPLAY_OFF        0x00
#define LCD_CURSOR_ON      0x02
#define LCD_CURSOR_OFF     0x00
#define LCD_BLINK_ON       0x01
#define LCD_BLINK_OFF      0x00

// Flags for cursor or display shift
#define LCD_DISPLAY_MOVE       0x08
#define LCD_CURSOR_MOVE        0x00
#define LCD_MOVE_RIGHT     0x04
#define LCD_MOVE_LEFT      0x00

// Flags for function set
#define LCD_MODE_8_BIT     0x10
#define LCD_MODE_4_BIT     0x00
#define LCD_LINE_2     0x08
#define LCD_LINE_1     0x00
#define LCD_DOTS_5x10      0x04
#define LCD_DOTS_5x8       0x00

// Flags for backlight control
#define LCD_BACKLIGHT      0x08
#define LCD_NO_BACKLIGHT       0x00

// Special flags
#define LCD_ENABLE          0x04
#define LCD_READ_WRITE      0x02
#define LCD_REGISTER_SELECT 0x01
#define LCD_COMMAND         0x00
#define LCD_CHAR            0x01

#endif
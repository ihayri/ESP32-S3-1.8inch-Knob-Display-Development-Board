#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#define EXAMPLE_LCD_H_RES              360
#define EXAMPLE_LCD_V_RES              360

#define LCD_BIT_PER_PIXEL              16

#define EXAMPLE_PIN_NUM_LCD_CS      14
#define EXAMPLE_PIN_NUM_LCD_PCLK    13
#define EXAMPLE_PIN_NUM_LCD_DATA0   15
#define EXAMPLE_PIN_NUM_LCD_DATA1   16
#define EXAMPLE_PIN_NUM_LCD_DATA2   17
#define EXAMPLE_PIN_NUM_LCD_DATA3   18
#define EXAMPLE_PIN_NUM_LCD_RST     21
#define EXAMPLE_PIN_NUM_BK_LIGHT    47

#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 10)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2                          //Timer time
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500                        //LVGL Indicates the maximum time for a task to run
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1                          //LVGL Minimum time to run a task
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)                 //LVGL runs the task stack
#define EXAMPLE_LVGL_TASK_PRIORITY     2                          //LVGL Running task priority

#define EXAMPLE_TOUCH_ADDR                0x15
#define EXAMPLE_PIN_NUM_TOUCH_SCL 12
#define EXAMPLE_PIN_NUM_TOUCH_SDA 11


//#define Backlight_Testing
//#define EXAMPLE_Rotate_90
#endif

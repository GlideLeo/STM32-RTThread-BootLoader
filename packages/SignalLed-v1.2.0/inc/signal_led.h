#ifndef __SIGNAL_LED_H
#define __SIGNAL_LED_H

#include "drv_gpio.h"

#define LED_TICK_TIME 50    //心跳函数调用的时间间隔（单位:ms）
#define LOOP_PERMANENT 0XFF //永久循环

#define HANDLE_EXIST 1

#define LED_OFF 0 //灯灭状态
#define LED_ON 1  //灯亮状态

//led 软件包内存操作
typedef struct led_mem_operation
{
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
} led_mem_opreation_t;

typedef struct _led
{
    void *led_internal; //led 内部处理结构体
    void (*switch_on)(void);
    void (*switch_off)(void);
    struct _led *next;
} led_t;

typedef void (*led_blink_over_callback)(led_t *led_handler);

void    led_ticks(void);
led_t*  led_create(void (*switch_on)(void), void (*switch_off)(void));
int     led_set_mem_operation(led_mem_opreation_t *operation);
void    led_set_blink_over_callback(led_t *led_handler, led_blink_over_callback callback);
void    led_set_mode(led_t *handle, uint8_t loop, char *blinkMode);
uint8_t led_start(led_t *led_handle);
void    led_stop(led_t *led_handle);
void    led_toggle(led_t *led_handle);

#endif

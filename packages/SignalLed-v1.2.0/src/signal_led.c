/*************************************************
 Copyright (c) 2019
 All rights reserved.
 File name:     signal_led.c
 Description:   
 History:
 1. Version:    
    Date:       2019/03/14
    Author:     WKJay
*************************************************/

#include "signal_led.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

static led_t *first_handle = NULL;
static led_mem_opreation_t led_mem_operation = {
    malloc, free};

//led 内部处理结构体
typedef struct _led_internal
{
    uint8_t active_state; //信号灯亮时的引脚状态 (PIN_HIGH/PIN_LOW)
    uint8_t state;        //信号灯当前的状态
    uint8_t mode_pointer; //用于指明当前参照闪烁数组中的第几个成员
    uint16_t tick_cnt;
    uint16_t mode_cnt;    //一个周期内的闪烁次数
    uint16_t handle_cnt;  //handle函数计数
    uint16_t loop;        //周期
    uint16_t loop_temp;   //存储周期的临时变量，当重启led时会被重置
    uint32_t blink_point; //闪烁节点
    uint32_t *blink_arr;
    char *blink_mode;                                         //亮、灭、亮、灭、亮、灭………………（注意时间之间以“,”隔开，最后必须以“,”结尾）
    void (*led_blink_over_callback)(struct _led *led_handle); //led闪烁结束回调函数
} led_internal_t;

/**
 * Name:    led_get_internal
 * Brief:   获得led的内部信息
 * Input:
 *  @led_handle:    led句柄
 * Output:  led内部信息结构体
 */
static led_internal_t *led_get_internal(led_t *led_handle)
{
    return (led_internal_t *)led_handle->led_internal;
}

/**
 * Name:    led_malloc
 * Brief:   led动态分配内存
 * Input:
 *  @size:  内存大小
 * Output:  内存指针
 */
static void *led_malloc(size_t size)
{
    return led_mem_operation.malloc_fn(size);
}

/**
 * Name:    led_free
 * Brief:   释放led动态分配的内存
 * Input:
 *  @mem_ptr:    要释放的内存块指针
 * Output:  None
 */
static void led_free(void *mem_ptr)
{
    led_mem_operation.free_fn(mem_ptr);
}

/**
 * Name:    led_set_mem_operation
 * Brief:   自定义内存管理函数
 * Input:
 *  @operation:    内存管理接口结构体
 * Output:  成功返回0，失败返回-1
 */
int led_set_mem_operation(led_mem_opreation_t *operation)
{
    if (operation->free_fn && operation->malloc_fn)
    {
        led_mem_operation.free_fn = operation->free_fn;
        led_mem_operation.malloc_fn = operation->malloc_fn;
        return 0;
    }
    return -1;
}

/**
 * Name:    led_create
 * Brief:   创建一个led对象
 * Input:
 *  @switch_on:    点亮led函数
 *  @switch_off:   熄灭led函数
 * Output:  led句柄
 */
led_t *led_create(void (*switch_on)(void), void (*switch_off)(void))
{
    led_internal_t *led_internal_ptr = NULL;
    led_t *led_ptr = NULL;

    //创建对象前先关闭led
    switch_off();
    //动态创建led对象
    led_internal_ptr = led_malloc(sizeof(led_internal_t));
    led_ptr = led_malloc(sizeof(led_t));
    //清空结构体
    memset(led_internal_ptr, 0, sizeof(led_internal_t));
    memset(led_ptr, 0, sizeof(led_t));

    led_ptr->led_internal = led_internal_ptr;
    led_ptr->switch_on = switch_on;
    led_ptr->switch_off = switch_off;

    return led_ptr;
}

/**
 * Name:    led_get_blink_arr
 * Brief:    获得信号灯一个周期内闪烁时间的数组（仅内部调用，用户无需关注）
 * Input:
 *  @handle:   led 内部信息句柄
 * Output:  None
 */
static void led_get_blink_arr(led_internal_t *handle)
{
    char *blink_modeTemp = NULL;
    uint8_t blinkCntNum = 0;

    if (handle->blink_arr)
    {
        led_free(handle->blink_arr);
        handle->blink_arr = NULL;
    }

    //获取数组长度
    for (blink_modeTemp = handle->blink_mode; *blink_modeTemp != '\0'; blink_modeTemp++)
    {
        if (*blink_modeTemp == ',')
        {
            handle->mode_cnt++;
        }
    }
    if (*(blink_modeTemp - 1) != ',')
        handle->mode_cnt++;

    //创建数组
    handle->blink_arr = led_malloc(sizeof(uint32_t) * handle->mode_cnt);

    //存储数据
    for (blink_modeTemp = handle->blink_mode; *blink_modeTemp != '\0'; blink_modeTemp++)
    {
        handle->blink_arr[blinkCntNum] = atol(blink_modeTemp);
        //计算出计数变量的值（根据信号灯定时器定时时间）

        if ((handle->blink_arr[blinkCntNum] % LED_TICK_TIME))
        {
            handle->blink_arr[blinkCntNum] = handle->blink_arr[blinkCntNum] / LED_TICK_TIME + 1;
        }
        else
        {
            handle->blink_arr[blinkCntNum] /= LED_TICK_TIME;
        }
        blinkCntNum++;
        while (*blink_modeTemp != ',')
        {
            if (*blink_modeTemp == '\0')
            {
                blink_modeTemp--;
                break;
            }
            blink_modeTemp++;
        }
    }
}

/**
 * Name:    led_set_mode
 * Brief:   设置信号灯的工作模式
 * Input:
 *  @loop:        循环次数 (0xff 永久循环)
 *  @blink_mode:   一个周期的闪烁方式(字符串形式,如："200,200,200,2000,"表示亮200ms,灭200ms,亮200ms,灭2000ms)
 *                注意：blink_mode字符串必须以英文逗号分隔，且最后以英文逗号结尾
 * Output:  led内部信息结构体
 */
void led_set_mode(led_t *handle, uint8_t loop, char *blink_mode)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(handle);

    led_internal_ptr->loop = loop;
    led_internal_ptr->loop_temp = loop;
    led_internal_ptr->blink_mode = blink_mode;
    led_internal_ptr->handle_cnt = 0;
    led_internal_ptr->blink_point = 0;
    led_internal_ptr->mode_pointer = 0;
    led_internal_ptr->mode_cnt = 0;
    led_get_blink_arr(led_internal_ptr);
}

/**
 * Name:    led_switch
 * Brief:   信号灯状态翻转（内部调用）
 * Input:
 *  @led_handle:   led 句柄
 * Output:  None
 */
static void led_switch(led_t *led_handle)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(led_handle);

    if (led_internal_ptr->mode_pointer % 2)
    {
        led_handle->switch_off();
        led_internal_ptr->state = LED_OFF;
    }
    else
    {
        led_handle->switch_on();
        led_internal_ptr->state = LED_ON;
    }
}

/**
 * Name:    led_toggle
 * Brief:   信号灯状态翻转（用户调用）
 * Input:
 *  @led_handle:   led 句柄
 * Output:  None
 */
void led_toggle(led_t *led_handle)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(led_handle);

    if (led_internal_ptr->state == LED_OFF)
    {
        led_handle->switch_on();
        led_internal_ptr->state = LED_ON;
    }
    else
    {
        led_handle->switch_off();
        led_internal_ptr->state = LED_OFF;
    }
}

/**
 * Name:    led_handle
 * Brief:   核心工作函数
 * Input:
 *  @crt_handle:   led 句柄
 * Output:  None
 */
static void led_handle(led_t *crt_handle)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(crt_handle);

    if (led_internal_ptr->loop_temp)
    {
        led_internal_ptr->handle_cnt++;
    __repeat:
        if ((led_internal_ptr->handle_cnt - 1) == led_internal_ptr->blink_point)
        {
            if (led_internal_ptr->mode_pointer < led_internal_ptr->mode_cnt)
            {
                led_internal_ptr->blink_point += led_internal_ptr->blink_arr[led_internal_ptr->mode_pointer];
                if (led_internal_ptr->blink_arr[led_internal_ptr->mode_pointer] == 0) //时间为0的直接跳过
                {
                    led_internal_ptr->mode_pointer++;
                    goto __repeat;
                }
                else
                {
                    led_switch(crt_handle);
                }
                led_internal_ptr->mode_pointer++;
            }
            else
            {
                led_internal_ptr->mode_pointer = 0;
                led_internal_ptr->blink_point = 0;
                led_internal_ptr->handle_cnt = 0;
                if (led_internal_ptr->loop_temp != LOOP_PERMANENT)
                    led_internal_ptr->loop_temp--;
            }
        }
    }
    else
    {
        led_stop(crt_handle);
        if (led_internal_ptr->led_blink_over_callback)
        {
            led_internal_ptr->led_blink_over_callback(crt_handle);
        }
    }
}

/**
 * Name:    led_start
 * Brief:   信号灯开启（若没有调用此函数开启信号灯则信号灯不会工作）
 * Input:
 *  @led_handle:   led 句柄
 * Output:  0：             正常
 *          HANDLE_EXIST：  已处于工作状态
 */
uint8_t led_start(led_t *led_handle)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(led_handle);

    led_internal_ptr->loop_temp = led_internal_ptr->loop; //启动时将其重置

    led_t *temp_handle = first_handle;
    while (temp_handle)
    {
        if (temp_handle == led_handle)
            return HANDLE_EXIST;         //已存在，则退出
        temp_handle = temp_handle->next; //设置链表当前指针
    }

    led_handle->next = first_handle;
    first_handle = led_handle;

    return 0;
}

/**
 * Name:    led_stop
 * Brief:   信号灯关闭（关闭信号灯后仍可调用开启函数开启）
 * Input:
 *  @led_handle:   led 句柄
 * Output:  None
 */
void led_stop(led_t *led_handle)
{
    led_t **handle_pointer = NULL;
    led_t *handle_destory = NULL;

    led_handle->switch_off(); //关闭灯

    for (handle_pointer = &first_handle; *handle_pointer;)
    {
        handle_destory = *handle_pointer;

        if (handle_destory == led_handle)
        {
            *handle_pointer = handle_destory->next;
        }
        else
        {
            handle_pointer = &handle_destory->next;
        }
    }
}

/**
 * Name:    led_set_blink_over_callback
 * Brief:   设置led闪烁结束后的回调函数
 * Input:
 *  @led_handle:   led句柄
 *  @callback:      回调函数
 * Output:  None
 */
void led_set_blink_over_callback(led_t *led_handle, led_blink_over_callback callback)
{
    led_internal_t *led_internal_ptr = NULL;
    led_internal_ptr = led_get_internal(led_handle);

    led_internal_ptr->led_blink_over_callback = callback;
}

/*@brief    信号灯心跳函数
 *@note     必须循环调用该函数，否则信号灯将不会工作。
 *          可以将其放入线程或定时器中，保证每隔LED_TICK_TIME毫秒调用即可
 *          LED_TICK_TIME是在"signal_led.h"中定义的宏，信号灯的工作基于该宏
 *          保证该宏所定义的时间为信号灯心跳函数调用的周期！
 * */
void led_ticks(void)
{
    led_t *current_handle;

    for (current_handle = first_handle; current_handle != NULL; current_handle = current_handle->next)
    {
        led_handle(current_handle);
    }
}

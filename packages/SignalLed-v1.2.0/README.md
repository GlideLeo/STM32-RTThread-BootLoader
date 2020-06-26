# 信号灯LED V2.1

# 1、介绍

在我们的日常开发中，led虽小，但却起着不可忽视的作用。我们总是用小灯的闪烁来表示一个事件的发生或停止。单个小灯的闪烁直接调用设置高低电平的函数会非常方便，然而，当你有多个led闪烁事件的时候，一个个的循环，一个个的delay可能就让你非常烦躁了。<br>
这个时候，快来试试这款简单的小工具吧，抛开所有的循环和延时，解放你的双手，只需几个设置，便可轻松实现各种不同的信号闪烁！

### 特性

- 支持字符串设置信号灯闪烁模式，简单易懂，操作方便。
- 支持非固定频率闪烁，闪烁方式可任意设置。
- 支持指定次数循环闪烁模式、无限循环闪烁模式、常亮模式、常灭模式。
- 支持中途修改闪烁模式，最少仅需一条语句即可改变模式。
- 支持设置信号灯循环结束的回调函数。
- 支持自定义软件包内存操作函数，告别内存碎片。

# 2、获取方式

使用SignalLed软件包需要在RT-Thread的包管理中选中它，具体路径如下：

```
RT-Thread online packages
    peripheral libraries and drivers  --->
        [*] SignalLed:a signal led package for rt-thread
```

# 3、使用方式

1. 首先根据自己的硬件平台初始化信号灯硬件引脚

```C
 rt_pin_mode(GET_PIN(A,8),PIN_MODE_OUTPUT);
```

2. 声明一个信号灯对象句柄

```C
led_t *led0 =  NULL;
```

3. 定义信号灯的开关函数

```C
//定义开灯函数
void led0_switch_on(void)
{
    rt_pin_write(LED0_PIN, PIN_LOW);
}
//定义关灯函数
void led0_switch_off(void)
{
    rt_pin_write(LED0_PIN, PIN_HIGH);
}
```

4. 设置闪烁模式（字符串形式，以亮、灭、亮、灭……的顺序，依次写入持续时间，单位：ms）

```C
char *led0BlinkMode = "200,200,200,200,200,1000,";
```

注意：时间参数必须以英文逗号间隔开，并且整个字符串必须以英文逗号结尾！
 
5. 动态创建一个信号灯对象，并给对应的句柄赋值

```C
led0 = led_create(led0_switch_on, led0_switch_off);
```

6. 设置信号灯工作模式（循环10次，闪烁方式为 `led0BlinkMode` 中的设定）

```C
led_set_mode(&led0, 10, led0BlinkMode);
```

7. 设置信号灯循环结束后的回调函数（若不需要回调则可以不设置）

```C
led_set_blink_over_callback(led0,blink_over_callback);
```

8. 开启信号灯

```C
led_start(&led0);
```

9. 创建一个信号灯线程循环调用信号灯心跳函数

```C
//每隔 LED_TICK_TIME（50） 毫秒循环调用心跳函数
while (1)
{
    led_ticks();
    rt_thread_mdelay(LED_TICK_TIME);
}
```

注意：LED_TICK_TIME 为 signal_led.h 中的宏，默认值为50，单位为ms。信号灯内核依赖该宏，不可删除，该宏作为信号灯闪烁计时的时基，因此信号灯处于亮或灭状态的时间一定大于该宏所定义的时间，即若该宏定义为50，则信号灯最快50毫秒变化一次状态，用户可根据自己工程的需求改变该值得大小。

# 4、API简介
本软件包给用户提供的API接口有：

###	创建信号灯对象
```C
led_t *led_create(void (*switch_on)(void), void (*switch_off)(void))
```

|参数|描述|
|----|----|
|switch_on|开灯函数（用户自定义）|
|switch_off|关灯函数（用户自定义）|

该函数用于动态创建一个信号灯对象，若创建成功则返回一个信号灯对象句柄，若失败则返回NULL。

开关灯函数由用户自定义，格式参照如下：

```C
void switch_on (void) //待传入的函数
{
    rt_pin_write(LED0_PIN, PIN_LOW);//实现具体操作的函数
}
```

### 设置信号灯工作模式

```C
void led_set_mode(led_t* handle,uint8_t loop,char* blinkMode);
```

|参数|描述|
|----|----|
|handle|信号灯句柄|
|loop|循环的次数|
|blinkMode|一个周期内的闪烁方式|

该函数定义了信号灯的闪烁模式以及按照固定的模式循环多少次，若要设置为永久循环则loop的参数为 `LOOP_PERMANENT` 。该宏在`signal_led.h`中定义，其数值为oxff。

###### loop 接受的参数
|loop值|描述|
|----|----|
|0-65535|循环0-65535次|
|LOOP_PERMANENT|永久循环|

###### blinkMode

"blinkMode" 参数为用户设置的信号灯闪烁方式，以字符串类型进行设置，用户仅需输入信号灯一个循环周期内对应的亮灭时间数即可，单位为毫秒。
例如：我想设置一个亮300ms，灭200ms,亮300ms,灭200ms,亮300ms,灭1000ms，并以这个方式循环工作下去的信号灯，那我的blinkMode设置如下：

```C
char *binkMode = "300,200,300,200,300,1000,"
```

注意：时间参数必须以英文逗号间隔开，并且整个字符串必须以英文逗号结尾！

###   !部署信号灯主心跳

本信号灯软件的工作机制完全基于一个核心处理函数，即信号灯心跳函数：

```C
void led_ticks (void);
```

使用该软件包定义的所有信号灯对象都由上述心跳函数统一管理，无需用户关注。用户需要将该函数部署在一个按固定时基触发的定时器内，在 `signal_led.h` 文件中定义有心跳时基宏 `LED_TICK_TIME`，单位为毫秒，用户可以直接使用该宏配置定时器的时基，也可自行写入定时器时基并修改该宏。但需要注意的时，必须保持该宏和调用心跳函数的时基保持相等，否则信号灯将无法正常工作！

同时，心跳函数的时基决定了信号灯的最快闪烁频率，若时基为50ms，则信号灯闪烁频率最高为 1000ms/50ms = 20Hz，且信号灯在一个状态保持的时间也必须为时基的整数倍，时基为50ms，则可以设置的保持时间为 50ms、100ms、150ms…………不是整数倍的内核自动向上取整。

最后需注意，信号灯主心跳函数必须部署，否则信号灯将无法工作。

### 设置信号灯循环结束回调函数

对于有限循环次数的信号灯，用户可以通过设置其循环结束回调函数来进行信号灯循环结束事件的捕获。

```C
void led_set_blink_over_callback(led_t *led_handle, led_blink_over_callback callback);
```

|参数|描述|
|----|----|
|led_handle|信号灯句柄|
|led_blink_over_callback|用户定义的回调函数|

当信号灯循环工作结束后，若用户设置了回调函数，内核会自动调用该回调函数。其中回调函数的格式如下：

```C
void (*led_blink_over_callback)(led_t *led_handler);
```

|参数|描述|
|----|----|
|led_handle|结束循环的信号灯句柄|

内核在调用该函数的同时会把结束循环的信号灯句柄传入该函数的参数，用户可以通过该参数识别与操作结束循环的信号灯。

**需要注意的是，若用户想要在回调函数中改变信号灯的工作模式，必须得保证信号灯心跳 `led_ticks()` 部署在非中断程序中，因为该操作会进行动态内存的分配，若在中断中进行任何动态内存操作，会出现不可预料的错误。**


### 开启信号灯

当对信号灯的设置都完成之后，我们就可以通过下面的函数开启信号灯：

```C
uint8_t led_start(led_t* led_handle);
```

|参数|描述|
|----|----|
|handle|信号灯句柄|

开启信号灯后信号灯将按照用户的配置循环工作，直至循环次数用完。当循环完成后或用户手动关闭后，用户仍可以通过该函数重启信号灯。

### 关闭信号灯

信号灯开启后，用户可根据自己的需求关闭信号灯：

```C
void led_stop(led_t *led_handle);
```

|参数|描述|
|----|----|
|handle|信号灯句柄|

关闭信号灯后信号灯配置不会消失，用户仍可调用开启函数重启信号灯。

### 信号灯常亮、常灭

若想实现信号灯的常亮/常灭，可以如下设置模式：

```C

/*常灭*/
char *alwaysOn = "0,100,";   //100可以为任意整数（小于long型最大值），保证亮的时间为0即可
led_set_mode(&led0, LOOP_PERMANENT, alwaysOn);

/*常亮*/
char *alwaysOff = "100,0,";   //100可以为任意整数（小于long型最大值），保证灭的时间为0即可
led_set_mode(&led0, LOOP_PERMANENT, alwaysOff);

```

当然，你仍然可以使用以下接口直接进行信号灯的开启/关闭。需要注意的是若想使用以下接口，则需要保证操作时信号灯当前不工作在其他模式内。

```C

/*其中 led 为你所定义的信号灯对象*/
led.switch_on();//开灯
led.switch_off();//关灯

```

### 自定义动态内存管理

该软件包内核多次使用了动态内存分配和释放，原始的 `malloc` 与 `free` 会带来不可避免的内存碎片。若用户对于内存管理的要求比较高，可以自定义动态内存管理的接口。若对于内存管理要求不高，则可以不进行自定义。

```C
int led_set_mem_operation(led_mem_opreation_t *operation)
```
|参数|描述|
|----|----|
|operation|内存接口结构体指针|

其中内存接口结构体的定义如下：

```C
typedef struct led_mem_operation
{
    void *(*malloc_fn)(size_t sz);
    void (*free_fn)(void *ptr);
} led_mem_opreation_t;
```

在自定义动态内存管理接口时，必须将内存接口结构体内的两个接口全部实现才能配置成功，否则自定义将不生效，并自动使用原始的内存管理函数。

**需要注意的是，若使用自定义动态内存管理，必须在调用任何该软件包内接口之前进行操作，因为在调用软件包接口时内核会自动进行内存分配，若提前调用信号灯操作函数（尤其是信号灯创建、模式设置），会导致原始内存分配和自定义内存分配同时存在，将出现不可预料的错误！具体调用时机可参考下文案例**


# 5、使用案例

```C

#include <rtthread.h>
#include <rtdevice.h>
#include "signal_led.h"

/* defined the LED pin */
#define LED0_PIN    GET_PIN(A, 8)

//定义信号灯对象句柄
led_t *led0 =  NULL;
//定义内存操作函数接口
led_mem_opreation_t led_mem_opreation;

/*  设置信号灯一个周期内的闪烁模式
 *  格式为 “亮、灭、亮、灭、亮、灭 …………” 长度不限
 *  注意：  该配置单位为毫秒，且必须大于 “LED_TICK_TIME” 宏，且为整数倍（不为整数倍则向下取整处理）
 *          必须以英文逗号为间隔，且以英文逗号结尾，字符串内只允许有数字及逗号，不得有其他字符出现
 */
char *led_blink_mode_0 = "500,500,"; //1Hz闪烁
char *led_blink_mode_1 = "50,50,";   //10Hz闪烁
char *led_blink_mode_2 = "0,100,";   //常灭
char *led_blink_mode_3 = "100,0,";   //常亮
char *led_blink_mode_4 = "100,100,100,1000,";//非固定时间
char *led_blink_mode_5 = "500,100,";
//定义开灯函数
void led0_switch_on(void)
{
    rt_pin_write(LED0_PIN, PIN_HIGH);
}


//定义关灯函数
void led0_switch_off(void)
{
    rt_pin_write(LED0_PIN, PIN_LOW);
}

//中途切换模式测试
void led_switch (void *param)
{
    rt_thread_mdelay(5000);
    led_set_mode(led0, LOOP_PERMANENT, led_blink_mode_1);   
    rt_thread_mdelay(5000);
    led_set_mode(led0, 10, led_blink_mode_5);  
}

void blink_over_callback(led_t *led_handler)
{
    led_set_mode(led_handler, LOOP_PERMANENT, led_blink_mode_4);
    led_start(led_handler);
}

static void led_run(void *parameter)
{
    while(1)
    {
        led_ticks();
        rt_thread_mdelay(LED_TICK_TIME);
    }
}


int rt_led_timer_init(void)
{
    rt_pin_mode(GET_PIN(A,8),PIN_MODE_OUTPUT);
    
/*自定义内存操作接口
 *注意：若要进行自定义内存操作，必须要在调用任何软件包内接口之前作设置，
 *      否则会出现不可意料的错误！！！
 */
    led_mem_opreation.malloc_fn = (void* (*)(size_t))rt_malloc;
    led_mem_opreation.free_fn = rt_free;
    led_set_mem_operation(&led_mem_opreation);
    
    //初始化信号灯对象
    led0 = led_create(led0_switch_on, led0_switch_off);
  
    //设置信号灯工作模式，循环十次
    led_set_mode(led0, LOOP_PERMANENT, led_blink_mode_0);
    //设置信号灯闪烁结束回调函数
    led_set_blink_over_callback(led0,blink_over_callback);
    
    //开启信号灯
    led_start(led0);
    
     
    rt_thread_t tid = RT_NULL;
    tid = rt_thread_create("signal_led",
                            led_run, 
                            RT_NULL,
                            512,
                            RT_THREAD_PRIORITY_MAX/2,
                            100);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    
    /* 创建线程1 */
    tid = rt_thread_create("led_switch_test",
                            led_switch, 
                            RT_NULL,
                            512,
                            RT_THREAD_PRIORITY_MAX/2,
                            100);
    if (tid != RT_NULL)
        rt_thread_startup(tid);
    return RT_EOK;
}
INIT_APP_EXPORT(rt_led_timer_init);

```

# 6、联系方式&感谢


- 作者：WKJay
- 主页：https://github.com/WKJay/SignalLed
- email: 1931048074@qq.com
- 如有意见或者建议请与作者取得联系，该说明文档会不定期更新。

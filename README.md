<!--
 * @Description: 
 * @Version: 1.0
 * @Autor: JunQi Liu
 * @Date: 2020-06-26 06:29:58
 * @LastEditors: JunQi Liu
 * @LastEditTime: 2020-06-26 23:19:48
 * @FilePath: \STM32-RTThread-BootLoader\README.md
--> 

# STM32-RTThread-BootLoader
[![Build Status](https://travis-ci.com/JassyL/STM32-RTThread-BootLoader.svg?branch=master)](https://travis-ci.com/JassyL/STM32-RTThread-BootLoader)
## 简介


## 外设支持

本 BootLoader 目前对外设的支持情况如下：

| **板载外设**      | **备注** |
| -|-|
| UART1        | PA9\PA10|
| SPI1 FLASH    | PB3\PB4\PB5\PB14|

## 使用说明


#### 硬件连接

USART1 GPIO Configuration 
```   
    PA9     ------> USART1_TX
    PA10    ------> USART1_RX 
```

SPI1 GPIO Configuration    
```
    PB3     ------> SPI1_SCK
    PB4     ------> SPI1_MISO
    PB5     ------> SPI1_MOSI 
    PB14    ------> SPI1_CS
```


#### 编译下载

双击 project.uvprojx 文件，打开 MDK5 工程，编译并下载程序到开发板。

> 工程默认配置使用 xxx 仿真器下载程序，在通过 xxx 连接开发板的基础上，点击下载按钮即可下载程序到开发板

#### 运行结果

下载程序成功之后，系统会自动运行，【这里写开发板运行起来之后的现象，如：LED 闪烁等】。

连接开发板对应串口到 PC , 在终端工具里打开相应的串口（115200-8-1-N），复位设备后，可以看到 RT-Thread 的输出信息:

```bash
 \ | /
- RT -     Thread Operating System
 / | \     4.0.3 build Jun 26 2020
 2006 - 2020 Copyright by rt-thread team
msh >
```

## 注意事项

- xxx

## 联系人信息

维护人:

-  [xxx](https://个人主页), 邮箱：<xxx@xxx.com>
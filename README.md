
# STM32-RTThread-BootLoader
[![Build Status](https://travis-ci.com/JassyL/STM32-RTThread-BootLoader.svg?branch=master)](https://travis-ci.com/JassyL/STM32-RTThread-BootLoader)
## 简介


## 外设支持

本 BSP 目前对外设的支持情况如下：

| **板载外设**      | **支持情况** | **备注**                              |
| :----------------- | :----------: | :------------------------------------- |
| 串口1        |     支持                                       |

## 使用说明


#### 硬件连接

使用数据线连接开发板到 PC，打开电源开关。

#### 编译下载

双击 project.uvprojx 文件，打开 MDK5 工程，编译并下载程序到开发板。

> 工程默认配置使用 xxx 仿真器下载程序，在通过 xxx 连接开发板的基础上，点击下载按钮即可下载程序到开发板

#### 运行结果

下载程序成功之后，系统会自动运行，【这里写开发板运行起来之后的现象，如：LED 闪烁等】。

连接开发板对应串口到 PC , 在终端工具里打开相应的串口（115200-8-1-N），复位设备后，可以看到 RT-Thread 的输出信息:

```bash
 \ | /
- RT -     Thread Operating System
 / | \     3.1.1 build Nov 19 2018
 2006 - 2018 Copyright by rt-thread team
msh >
```

## 注意事项

- xxx

## 联系人信息

维护人:

-  [xxx](https://个人主页), 邮箱：<xxx@xxx.com>
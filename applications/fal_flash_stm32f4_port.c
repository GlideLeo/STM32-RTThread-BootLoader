/*
 * File      : fal_flash_stm32f2_port.c
 * This file is part of FAL (Flash Abstraction Layer) package
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-08-03     warfalcon    the first version
 */

#include <fal.h>
#include <drv_flash.h>

static int init(void)
{
    /* do nothing now */
	return RT_EOK;
}

static int read(long offset, uint8_t *buf, size_t size)
{
	return stm32_flash_read(stm32f4_onchip_flash.addr + offset, buf, size);
}

static int write(long offset, const uint8_t *buf, size_t size)
{	
	return stm32_flash_write(stm32f4_onchip_flash.addr + offset, buf, size);
}

static int erase(long offset, size_t size)
{	
	return stm32_flash_erase(stm32f4_onchip_flash.addr + offset, size);
}

const struct fal_flash_dev stm32f4_onchip_flash = {"onchip_flash", 0x08000000, 512 * 1024, 128 * 1024, {init, read, write, erase}};

/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-09-22     Warfalcon    first version
 */

#include <rtthread.h>

static rt_uint32_t crc_tab[256];

static rt_uint32_t bitrev(rt_uint32_t poly, rt_uint32_t width)
{
	rt_uint32_t i;
	rt_uint32_t var = 0;

	for (i = 0; i < width; i++)
	{
		if (poly & 0x00000001)
			var |= 1 << (width - 1 - i);
		
		poly >>= 1;
	}
	return var;
}

static void crc32_init(rt_uint32_t poly)
{
	rt_uint32_t i, j;
	rt_uint32_t c;
	
	poly = bitrev(poly, 32);
	
	for (i = 0; i < 256; i++)
	{
	    c = i;
		
		for (j = 0; j < 8; j++)
		{
			if(c & 0x00000001)
				c = poly ^ (c >> 1);
			else
				c >>= 1;
		}
		
		crc_tab[i] = c;
    }
}

static rt_uint32_t crc32(rt_uint32_t crc_init, rt_uint8_t *buf, rt_uint32_t len)
{
	rt_uint32_t i;
	rt_uint8_t index;

	for (i = 0; i < len; i++)
	{
		index = (rt_uint8_t)(crc_init ^ buf[i]);
		crc_init = (crc_init >> 8) ^ crc_tab[index];
	}
	
	return crc_init;
}

rt_uint32_t rt_fota_crc(rt_uint8_t *buf, rt_uint32_t len)
{
	rt_uint32_t crc_init = 0xffffffff;

	crc_init = crc32(crc_init, buf, len) ^ 0xffffffff;
	
	return crc_init;
}

rt_uint32_t rt_fota_step_crc(rt_uint32_t crc, rt_uint8_t *buf, rt_uint32_t len)
{
	return crc32(crc, buf, len);
}

int rt_fota_crc_init(void)
{
#define POLYNOMIAL		0x04c11db7

	crc32_init(POLYNOMIAL);
	return RT_EOK;
}
INIT_PREV_EXPORT(rt_fota_crc_init);



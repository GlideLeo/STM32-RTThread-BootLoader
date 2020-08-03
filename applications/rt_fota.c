/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-09-22     Warfalcon    first version
 * 2020-06-27     JunQi Liu	   modify the factory reset button pin & LED pin & wait time
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include <fal.h>
#include <tinycrypt.h>
#include <fastlz.h>
#include <quicklz.h>
#include <rt_fota.h>
#include <signal_led.h>

#if defined(RT_USING_FINSH) && defined(FINSH_USING_MSH)
#include <finsh.h>
#include <shell.h>
#endif

#define DBG_ENABLE
#define DBG_SECTION_NAME                    "fota"

#ifdef RT_FOTA_DEBUG
#define DBG_LEVEL                           DBG_LOG
#else
#define DBG_LEVEL                           DBG_INFO
#endif

#define DBG_COLOR
#include <rtdbg.h>

#ifndef RT_FOTA_THREAD_STACK_SIZE			
#define RT_FOTA_THREAD_STACK_SIZE			4096
#endif

#ifndef RT_FOTA_THREAD_PRIORITY			
#define RT_FOTA_THREAD_PRIORITY				(RT_THREAD_PRIORITY_MAX - 3)
#endif

#ifndef RT_FOTA_ALGO_BUFF_SIZE
#define RT_FOTA_ALGO_BUFF_SIZE				4096
#endif

/**
 * AES256 encryption algorithm option
 */
#ifndef RT_FOTA_ALGO_AES_IV
#define RT_FOTA_ALGO_AES_IV  				"0123456789ABCDEF"
#endif

#ifndef RT_FOTA_ALGO_AES_KEY
#define RT_FOTA_ALGO_AES_KEY 				"0123456789ABCDEF0123456789ABCDEF"
#endif

#ifndef RT_FOTA_BLOCK_HEADER_SIZE
#define RT_FOTA_BLOCK_HEADER_SIZE			4
#endif

#ifndef RT_FOTA_CMPRS_BUFFER_SIZE			
#define RT_FOTA_CMPRS_BUFFER_SIZE			4096
#endif

#ifndef RT_FOTA_FASTLZ_BUFFER_PADDING
#define RT_FOTA_FASTLZ_BUFFER_PADDING 		FASTLZ_BUFFER_PADDING(RT_FOTA_CMPRS_BUFFER_SIZE)
#endif

#ifndef RT_FOTA_QUICKLZ_BUFFER_PADDING
#define RT_FOTA_QUICKLZ_BUFFER_PADDING		QLZ_BUFFER_PADDING
#endif

#ifndef RT_FOTA_ENTER_SHELL_KEY
#define RT_FOTA_ENTER_SHELL_KEY				0x0d
#endif

#ifndef RT_FOTA_GET_CHAR_WAITTIGN
#define RT_FOTA_GET_CHAR_WAITTIGN			(RT_TICK_PER_SECOND * 1)
#endif

#ifndef RT_FOTA_SIGNAL_LED_PIN
#define RT_FOTA_SIGNAL_LED_PIN				GET_PIN(C,13)
#endif

#ifndef RT_FOTA_DEFAULT_KEY_PIN
#define RT_FOTA_DEFAULT_KEY_PIN				68
#endif

#ifndef RT_FOTA_DEFAULT_KEY_CHK_TIME
#define RT_FOTA_DEFAULT_KEY_CHK_TIME		10
#endif

#ifndef RT_FOTA_SIGNAL_LED_THREAD_STACK_SIZE			
#define RT_FOTA_SIGNAL_LED_THREAD_STACK_SIZE	1024
#endif

#ifndef RT_FOTA_SIGNAL_LED_THREAD_PRIORITY			
#define RT_FOTA_SIGNAL_LED_THREAD_PRIORITY		(RT_THREAD_PRIORITY_MAX - 4)
#endif

/* For signal led */
static led_t *signal_led =  NULL;
static led_mem_opreation_t signal_led_mem_op;
const char *led_shell_mode   = "500,500,"; /* 1Hz  闪烁 */
const char *led_upgrade_mode = "50,50,";   /* 10Hz 闪烁 */
const char *led_off_mode     = "0,100,";   /* 常灭 */
const char *led_on_mode      = "100,0,";   /* 常亮 */


/* For default key */
#define RT_FOTA_DEFAULT_KEY_PIN				GET_PIN(B, 7)

/* For shell */
static rt_sem_t shell_sem = RT_NULL;
static rt_device_t shell_dev = RT_NULL;

typedef struct {
	char type[4];
	rt_uint16_t fota_algo;
	rt_uint8_t fm_time[6];
	char app_part_name[16];
	char download_version[24];
	char current_version[24];
	rt_uint32_t code_crc;
	rt_uint32_t hash_val;
	rt_uint32_t raw_size;
	rt_uint32_t com_size;
	rt_uint32_t head_crc;
} rt_fota_part_head, *rt_fota_part_head_t;

typedef void (*rt_fota_app_func)(void);	
static rt_fota_app_func app_func = RT_NULL;

static rt_fota_part_head fota_part_head;


static void rt_fota_signal_led_on(void)
{
	rt_pin_write(RT_FOTA_SIGNAL_LED_PIN, PIN_LOW);
}

static void rt_fota_signal_led_off(void)
{
	rt_pin_write(RT_FOTA_SIGNAL_LED_PIN, PIN_HIGH);
}

static void rt_fota_signal_led_entry(void *arg)
{
	while(1)
    {
        led_ticks();
        rt_thread_mdelay(LED_TICK_TIME);
    }
}

static void rt_fota_signal_led_init(void)
{
	rt_pin_mode(RT_FOTA_SIGNAL_LED_PIN, PIN_MODE_OUTPUT);

	signal_led_mem_op.malloc_fn = (void* (*)(size_t))rt_malloc;
    signal_led_mem_op.free_fn = rt_free;
    led_set_mem_operation(&signal_led_mem_op);

	signal_led = led_create(rt_fota_signal_led_on, rt_fota_signal_led_off);

	/* Config signal led mode */
    led_set_mode(signal_led, LOOP_PERMANENT, (char *)led_on_mode);
    led_set_blink_over_callback(signal_led, RT_NULL);   
    led_start(signal_led);

	rt_thread_t tid;
	tid = rt_thread_create("sig_led", rt_fota_signal_led_entry, RT_NULL, RT_FOTA_SIGNAL_LED_THREAD_STACK_SIZE, RT_FOTA_SIGNAL_LED_THREAD_PRIORITY, 10);
	if (tid)
		rt_thread_startup(tid);
}

static void rt_fota_signal_led_mode(const char *led_cfg)
{
	RT_ASSERT(led_cfg != RT_NULL);
	
	led_set_mode(signal_led, LOOP_PERMANENT, (char *)led_cfg);
}

static int rt_fota_boot_verify(void)
{
	int fota_res = RT_FOTA_NO_ERR;

	rt_memset(&fota_part_head, 0x0, sizeof(rt_fota_part_head));
	
	/* partition initial */
	fal_init(); 

	extern int fal_init_check(void);
	/* verify partition */
	if (fal_init_check() != 1)
    {
    	LOG_D("Partition initialized failed!");
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_boot_verify;
    }

__exit_boot_verify:
	return fota_res;
}

int rt_fota_part_fw_verify(const char *part_name)
{
#define RT_FOTA_CRC_BUFF_SIZE		4096
#define RT_FOTA_CRC_INIT_VAL		0xffffffff

	int fota_res = RT_FOTA_NO_ERR;
	const struct fal_partition *part;
	rt_fota_part_head part_head;
	rt_uint8_t *body_buf = RT_NULL;
	rt_uint32_t body_crc = RT_FOTA_CRC_INIT_VAL;
	rt_uint32_t hdr_crc;

	if (part_name == RT_NULL)
	{
		LOG_D("Invaild paramenter input!");
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_partition_verify;
	}

	part = fal_partition_find(part_name);
	if (part == RT_NULL)
	{		
		LOG_D("Partition[%s] not found.", part_name);
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_partition_verify;
	}

	/* read the head of RBL files */
	if (fal_partition_read(part, 0, (rt_uint8_t *)&part_head, sizeof(rt_fota_part_head)) < 0)
	{
		LOG_D("Partition[%s] read error!", part->name);
		fota_res = RT_FOTA_PART_READ_ERR;
		goto __exit_partition_verify;
	}

	extern rt_uint32_t rt_fota_crc(rt_uint8_t *buf, rt_uint32_t len);
	hdr_crc = rt_fota_crc((rt_uint8_t *)&part_head, sizeof(rt_fota_part_head) - 4);
	if (hdr_crc != part_head.head_crc)
	{
		LOG_D("Partition[%s] head CRC32 error!", part->name);
		fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __exit_partition_verify;
	}
	
	if (rt_strcmp(part_head.type, "RBL") != 0)
	{
		LOG_D("Partition[%s] type[%s] not surport.", part->name, part_head.type);
		fota_res = RT_FOTA_CHECK_FAILED;
		goto __exit_partition_verify;
	}

	if (fal_partition_find(part_head.app_part_name) == RT_NULL)
	{
		LOG_D("Partition[%s] not found.", part_head.app_part_name);
		fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __exit_partition_verify;
	}

	body_buf = rt_malloc(RT_FOTA_CRC_BUFF_SIZE);
	if (body_buf == RT_NULL)
	{
		LOG_D("Not enough memory for body CRC32 verify.");	
		fota_res = RT_FOTA_NO_MEM_ERR;
		goto __exit_partition_verify;
	}

	for (int body_pos = 0; body_pos < part_head.com_size;)
	{	
		int body_read_len = fal_partition_read(part, sizeof(rt_fota_part_head) + body_pos, body_buf, RT_FOTA_CRC_BUFF_SIZE);      
		if (body_read_len > 0) 
		{
            if ((body_pos + body_read_len) > part_head.com_size)
            {
                body_read_len = part_head.com_size - body_pos;
            }
            
			extern rt_uint32_t rt_fota_step_crc(rt_uint32_t crc, rt_uint8_t *buf, rt_uint32_t len);
			body_crc = rt_fota_step_crc(body_crc, body_buf, body_read_len);	
			body_pos = body_pos + body_read_len;
		}
		else
		{
			LOG_D("Partition[%s] read error!", part->name);		
			fota_res = RT_FOTA_PART_READ_ERR;
			goto __exit_partition_verify;
		}
	}
	body_crc = body_crc ^ RT_FOTA_CRC_INIT_VAL;
	
	if (body_crc != part_head.code_crc)
	{
		LOG_D("Partition[%s] firmware integrity verify failed.", part->name);		
		fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __exit_partition_verify;
	}

__exit_partition_verify:
	if (fota_res == RT_FOTA_NO_ERR)
	{
		rt_enter_critical();
		rt_memcpy(&fota_part_head, &part_head, sizeof(rt_fota_part_head));
		rt_exit_critical();

		LOG_D("partition[%s] verify success!", part->name);
	}
	else
	{
		rt_enter_critical();
		rt_memset(&fota_part_head, 0x0, sizeof(rt_fota_part_head));
		rt_exit_critical();
		
		LOG_D("Partition[%s] verify failed!", part->name);
	}

	if (body_buf)
		rt_free(body_buf);
	
	return fota_res;
}

int rt_fota_check_upgrade(void)
{
	int is_upgrade = 0;

	if (rt_strcmp(fota_part_head.download_version, fota_part_head.current_version) != 0)
	{
		is_upgrade = 1;
		LOG_D("Application need upgrade.");
		goto __exit_check_upgrade;
	}

__exit_check_upgrade:
	return is_upgrade;
}

int rt_fota_copy_version(const char *part_name)
{
#define THE_NOR_FLASH_GRANULARITY		4096

	int fota_res = RT_FOTA_NO_ERR;
	const struct fal_partition *part;
    
    rt_fota_part_head_t part_head = RT_NULL;
    rt_uint8_t *cache_buf = RT_NULL;

	part = fal_partition_find(part_name);
	if (part == RT_NULL)
	{
		LOG_D("Find partition[%s] not found.", part_name);
		fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __exit_copy_version;
	}
	    
    cache_buf = rt_malloc(THE_NOR_FLASH_GRANULARITY);
    if (cache_buf == RT_NULL)
    {
        LOG_D("Not enough memory for head erase.");
        fota_res = RT_FOTA_NO_MEM_ERR;
        goto __exit_copy_version;
    }
    part_head = (rt_fota_part_head_t)cache_buf;
	
	if (fal_partition_read(part, 0, cache_buf, THE_NOR_FLASH_GRANULARITY) < 0)
	{
		LOG_I("Read partition[%s] failed.", part_name);
		fota_res = RT_FOTA_PART_READ_ERR;
		goto __exit_copy_version;
	}
	
	rt_memcpy(part_head->current_version, part_head->download_version, sizeof(part_head->current_version));
	extern rt_uint32_t rt_fota_crc(rt_uint8_t *buf, rt_uint32_t len);
	part_head->head_crc = rt_fota_crc((rt_uint8_t *)part_head, sizeof(rt_fota_part_head) - 4);
	
    if (fal_partition_erase(part, 0, THE_NOR_FLASH_GRANULARITY) < 0)
    {
		LOG_D("Erase partition[%s] failed.", part_name);
		fota_res = RT_FOTA_PART_ERASE_ERR;
		goto __exit_copy_version;
    }
	
	if (fal_partition_write(part, 0, (const rt_uint8_t *)cache_buf, THE_NOR_FLASH_GRANULARITY) < 0)
	{
		LOG_I("Write partition[%s] failed.", part_name);
		fota_res = RT_FOTA_PART_WRITE_ERR;
		goto __exit_copy_version;
	}
__exit_copy_version:
	if (cache_buf)
		rt_free(cache_buf);
	
	if (fota_res != RT_FOTA_NO_ERR)
		LOG_I("Copy firmware version failed!");
	else
		LOG_I("Copy firmware version Success!");
	
	return fota_res;
}


int rt_fota_erase_app_part(void)
{
	int fota_res = RT_FOTA_NO_ERR;
	const struct fal_partition *part;

	part = fal_partition_find(fota_part_head.app_part_name);
	if (part == RT_NULL)
	{
		LOG_D("Erase partition[%s] not found.", fota_part_head.app_part_name);
		fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __exit_partition_erase;
	}
    
    LOG_I("Partition[%s] erase start:", part->name);
	if (fal_partition_erase(part, 0, fota_part_head.raw_size) < 0)
	{
		LOG_D("Partition[%s] erase failed!", part->name);
		fota_res = RT_FOTA_PART_ERASE_ERR;
		goto __exit_partition_erase;
	}

__exit_partition_erase:
	if (fota_res == RT_FOTA_NO_ERR)
	{
		LOG_D("Partition[%s] erase %d bytes success!", part->name, fota_part_head.raw_size);
	}
	return fota_res;
}

int rt_fota_write_app_part(int fw_pos, rt_uint8_t *fw_buf, int fw_len)
{
	int rt_fota_res = RT_FOTA_NO_ERR;
	const struct fal_partition *part;

	part = fal_partition_find(fota_part_head.app_part_name);
	if (part == RT_NULL)
	{
		LOG_D("Erase partition[%s] not found.", fota_part_head.app_part_name);
		rt_fota_res = RT_FOTA_FW_VERIFY_FAILED;
		goto __partition_write_exit;
	}

	if (fal_partition_write(part, fw_pos, fw_buf, fw_len) < 0)
	{
		LOG_D("Partition[%s] write failed!", part->name);
		rt_fota_res = RT_FOTA_PART_WRITE_ERR;
		goto __partition_write_exit;
	}
__partition_write_exit:
	if (rt_fota_res == RT_FOTA_NO_ERR)
	{
		LOG_D("Partition[%s] write %d bytes success!", part->name, fw_len);
	}
	return rt_fota_res;
}

static int rt_fota_read_part(const struct fal_partition *part, int read_pos, tiny_aes_context *aes_ctx, rt_uint8_t *aes_iv, rt_uint8_t *decrypt_buf, rt_uint32_t decrypt_len)
{
	int fota_err = RT_FOTA_NO_ERR;
	rt_uint8_t *encrypt_buf = RT_NULL;

	if ((part == RT_NULL) || (decrypt_buf == RT_NULL) 
		|| (decrypt_len % 16 != 0) || (decrypt_len > RT_FOTA_ALGO_BUFF_SIZE))
	{
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_read_decrypt;
	}

	rt_memset(decrypt_buf, 0x0, decrypt_len);

	/* Not use AES256 algorithm */
	if (aes_ctx == RT_NULL || aes_iv == RT_NULL)
	{
		fota_err = fal_partition_read(part, sizeof(rt_fota_part_head) + read_pos, decrypt_buf, decrypt_len);
		if (fota_err <= 0)
		{
			fota_err = RT_FOTA_PART_READ_ERR;
		}
		goto __exit_read_decrypt;
	}

	encrypt_buf = rt_malloc(decrypt_len);
	if (encrypt_buf == RT_NULL)
	{
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_read_decrypt;
	}
	rt_memset(encrypt_buf, 0x0, decrypt_len);

	fota_err = fal_partition_read(part, sizeof(rt_fota_part_head) + read_pos, encrypt_buf, decrypt_len);
	if (fota_err <= 0 || fota_err % 16 != 0)
	{
		fota_err = RT_FOTA_PART_READ_ERR;
		goto __exit_read_decrypt;
	}

	tiny_aes_crypt_cbc(aes_ctx, AES_DECRYPT, fota_err, aes_iv, encrypt_buf, decrypt_buf);
__exit_read_decrypt:
	if (encrypt_buf)
		rt_free(encrypt_buf);
	
	return fota_err;
}

int rt_fota_upgrade(const char *part_name)
{
	int fota_err = RT_FOTA_NO_ERR;
	
	const struct fal_partition *part;
	rt_fota_part_head_t part_head = RT_NULL;
	
	tiny_aes_context *aes_ctx = RT_NULL;
	rt_uint8_t *aes_iv = RT_NULL;
	rt_uint8_t *crypt_buf = RT_NULL;
	
	int fw_raw_pos = 0;
	int fw_raw_len = 0;
	rt_uint32_t total_copy_size = 0;

	rt_uint8_t block_hdr_buf[RT_FOTA_BLOCK_HEADER_SIZE];	
	rt_uint32_t block_hdr_pos = RT_FOTA_ALGO_BUFF_SIZE;
	rt_uint32_t block_size = 0;
	rt_uint32_t dcprs_size = 0;
	
	qlz_state_decompress *dcprs_state = RT_NULL;
	rt_uint8_t *cmprs_buff = RT_NULL;
	rt_uint8_t *dcprs_buff = RT_NULL;
	rt_uint32_t padding_size = 0;

	if (part_name == RT_NULL)
	{
		LOG_D("Invaild paramenter input!");
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_upgrade;
	}

	part = fal_partition_find(part_name);
	if (part == RT_NULL)
	{		
		LOG_D("Upgrade partition[%s] not found.", part_name);
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_upgrade;
	}
	
	/* Application partition erase */
	fota_err = rt_fota_erase_app_part();
	if (fota_err != RT_FOTA_NO_ERR)
	{
		goto __exit_upgrade;
	}

	/* rt_fota_erase_app_part() has check fota_part_head vaild already */
	part_head = &fota_part_head;

	crypt_buf = rt_malloc(RT_FOTA_ALGO_BUFF_SIZE);
	if (crypt_buf == RT_NULL)
	{
		LOG_D("Not enough memory for firmware buffer.");
		fota_err = RT_FOTA_NO_MEM_ERR;
		goto __exit_upgrade;
	}

	/* AES256 algorithm enable */
	if ((part_head->fota_algo & RT_FOTA_CRYPT_STAT_MASK) == RT_FOTA_CRYPT_ALGO_AES256)
	{
		aes_ctx = rt_malloc(sizeof(tiny_aes_context));	
		aes_iv = rt_malloc(rt_strlen(RT_FOTA_ALGO_AES_IV) + 1);		
		if (aes_ctx == RT_NULL || aes_iv == RT_NULL)
		{
			LOG_D("Not enough memory for firmware hash verify.");
			fota_err = RT_FOTA_NO_MEM_ERR;
			goto __exit_upgrade;
		}

		rt_memset(aes_iv, 0x0, rt_strlen(RT_FOTA_ALGO_AES_IV) + 1);
		rt_memcpy(aes_iv, RT_FOTA_ALGO_AES_IV, rt_strlen(RT_FOTA_ALGO_AES_IV));
		tiny_aes_setkey_dec(aes_ctx, (rt_uint8_t *)RT_FOTA_ALGO_AES_KEY, 256);
	}
	else if ((part_head->fota_algo & RT_FOTA_CRYPT_STAT_MASK) == RT_FOTA_CRYPT_ALGO_XOR)
	{
		LOG_I("Not surpport XOR.");
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_upgrade;
	}
	
	/* If enable fastlz compress function */	
	if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_FASTLZ) 
	{
		cmprs_buff = rt_malloc(RT_FOTA_CMPRS_BUFFER_SIZE + RT_FOTA_FASTLZ_BUFFER_PADDING);
		dcprs_buff = rt_malloc(RT_FOTA_CMPRS_BUFFER_SIZE);	
		if (cmprs_buff == RT_NULL || dcprs_buff == RT_NULL)
		{
			LOG_D("Not enough memory for firmware hash verify.");
			fota_err = RT_FOTA_NO_MEM_ERR;
			goto __exit_upgrade;
		}

		padding_size = RT_FOTA_FASTLZ_BUFFER_PADDING;
	}
	else if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_QUICKLZ) 
	{
		cmprs_buff = rt_malloc(RT_FOTA_CMPRS_BUFFER_SIZE + RT_FOTA_QUICKLZ_BUFFER_PADDING);
		dcprs_buff = rt_malloc(RT_FOTA_CMPRS_BUFFER_SIZE);	
		dcprs_state = rt_malloc(sizeof(qlz_state_decompress));
		if (cmprs_buff == RT_NULL || dcprs_buff == RT_NULL || dcprs_state == RT_NULL)
		{
			LOG_D("Not enough memory for firmware hash verify.");
			fota_err = RT_FOTA_NO_MEM_ERR;
			goto __exit_upgrade;
		}

		padding_size = RT_FOTA_QUICKLZ_BUFFER_PADDING;
		rt_memset(dcprs_state, 0x0, sizeof(qlz_state_decompress));
	}
	else if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_GZIP) 
	{
		LOG_I("Not surpport GZIP.");
		fota_err = RT_FOTA_GENERAL_ERR;
		goto __exit_upgrade;
	}

	LOG_I("Start to copy firmware from %s to %s partition:", part->name, part_head->app_part_name);
	while (fw_raw_pos < part_head->com_size)
	{
		if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) != RT_FOTA_CRYPT_ALGO_NONE) 
		{		
			if (block_hdr_pos >= RT_FOTA_ALGO_BUFF_SIZE)
			{
				fw_raw_len = rt_fota_read_part(part, fw_raw_pos, aes_ctx, aes_iv, crypt_buf, RT_FOTA_ALGO_BUFF_SIZE);
				if (fw_raw_len < 0)
				{
					LOG_D("AES256 algorithm failed.");
					fota_err = RT_FOTA_PART_READ_ERR;
					goto __exit_upgrade;
				}
				fw_raw_pos += fw_raw_len;

				rt_memcpy(block_hdr_buf, crypt_buf, RT_FOTA_BLOCK_HEADER_SIZE);
				block_size = block_hdr_buf[0] * (1 << 24) + block_hdr_buf[1] * (1 << 16) + block_hdr_buf[2] * (1 << 8) + block_hdr_buf[3];
				rt_memset(cmprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE + padding_size);
				rt_memcpy(cmprs_buff, &crypt_buf[RT_FOTA_BLOCK_HEADER_SIZE], block_size);

				block_hdr_pos = RT_FOTA_BLOCK_HEADER_SIZE + block_size;
			}
			else
			{
				rt_uint8_t hdr_tmp_pos = 0;
				while (block_hdr_pos < RT_FOTA_ALGO_BUFF_SIZE)
				{
					if (hdr_tmp_pos < RT_FOTA_BLOCK_HEADER_SIZE)
					{
						block_hdr_buf[hdr_tmp_pos++] = crypt_buf[block_hdr_pos++];
					}
					else
					{
						block_size = block_hdr_buf[0] * (1 << 24) + block_hdr_buf[1] * (1 << 16) + block_hdr_buf[2] * (1 << 8) + block_hdr_buf[3];
						
						rt_memset(cmprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE + padding_size);
						if (block_size > (RT_FOTA_ALGO_BUFF_SIZE - block_hdr_pos))
						{								
							rt_memcpy(cmprs_buff, &crypt_buf[block_hdr_pos], (RT_FOTA_ALGO_BUFF_SIZE - block_hdr_pos));
							fw_raw_len = rt_fota_read_part(part, fw_raw_pos, aes_ctx, aes_iv, crypt_buf, RT_FOTA_ALGO_BUFF_SIZE);
							if (fw_raw_len < 0)
							{
								LOG_D("AES256 algorithm failed.");
								fota_err = RT_FOTA_PART_READ_ERR;
								goto __exit_upgrade;
							}
							fw_raw_pos += fw_raw_len;

							rt_memcpy(&cmprs_buff[RT_FOTA_ALGO_BUFF_SIZE - block_hdr_pos], &crypt_buf[0], (block_size +  block_hdr_pos) - RT_FOTA_ALGO_BUFF_SIZE);
							block_hdr_pos = (block_size +  block_hdr_pos) - RT_FOTA_ALGO_BUFF_SIZE;
						}
						else
						{
							rt_memcpy(cmprs_buff, &crypt_buf[block_hdr_pos], block_size);
							block_hdr_pos = block_hdr_pos + block_size;
						}						
						break;
					}
				}
				
				if (hdr_tmp_pos < RT_FOTA_BLOCK_HEADER_SIZE)
				{				
					fw_raw_len = rt_fota_read_part(part, fw_raw_pos, aes_ctx, aes_iv, crypt_buf, RT_FOTA_ALGO_BUFF_SIZE);
					if (fw_raw_len < 0)
					{
						LOG_D("AES256 algorithm failed.");
						fota_err = RT_FOTA_PART_READ_ERR;
						goto __exit_upgrade;
					}
					fw_raw_pos += fw_raw_len;

					block_hdr_pos = 0;
					while (hdr_tmp_pos < RT_FOTA_BLOCK_HEADER_SIZE)
					{
						block_hdr_buf[hdr_tmp_pos++] = crypt_buf[block_hdr_pos++];
					}
					block_size = block_hdr_buf[0] * (1 << 24) + block_hdr_buf[1] * (1 << 16) + block_hdr_buf[2] * (1 << 8) + block_hdr_buf[3];

					rt_memset(cmprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE + padding_size);
					rt_memcpy(cmprs_buff, &crypt_buf[block_hdr_pos], block_size);

					block_hdr_pos = (block_hdr_pos + block_size) % RT_FOTA_ALGO_BUFF_SIZE;
				}
			}

			rt_memset(dcprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE);		
			if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_FASTLZ) 
			{
				dcprs_size = fastlz_decompress((const void *)&cmprs_buff[0], block_size, &dcprs_buff[0], RT_FOTA_CMPRS_BUFFER_SIZE);
			}
			else if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_QUICKLZ) 
			{
				dcprs_size = qlz_decompress((const char *)&cmprs_buff[0], &dcprs_buff[0], dcprs_state);
			}
			
			if (dcprs_size <= 0)
			{
				LOG_D("Decompress failed: %d.", dcprs_size);
				fota_err = RT_FOTA_GENERAL_ERR;
				goto __exit_upgrade;
			}

			if (rt_fota_write_app_part(total_copy_size, dcprs_buff, dcprs_size) < 0)
			{
				fota_err = RT_FOTA_COPY_FAILED;
				goto __exit_upgrade;
			}

			total_copy_size += dcprs_size;
			rt_kprintf("#");
		}
		/* no compress option */
		else
		{
			fw_raw_len = rt_fota_read_part(part, fw_raw_pos, aes_ctx, aes_iv, crypt_buf, RT_FOTA_ALGO_BUFF_SIZE);
			if (fw_raw_len < 0)
			{
				LOG_D("AES256 algorithm failed.");
				fota_err = RT_FOTA_PART_READ_ERR;
				goto __exit_upgrade;
			}		
			fw_raw_pos += fw_raw_len;

			if (rt_fota_write_app_part(total_copy_size, crypt_buf, fw_raw_len) < 0)
			{
				fota_err = RT_FOTA_COPY_FAILED;
				goto __exit_upgrade;
			}
			
			total_copy_size += fw_raw_len;
			rt_kprintf("#");
		}
	}

	/* it has compress option */
	if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) != RT_FOTA_CRYPT_ALGO_NONE)
	{
		if ((block_hdr_pos < fw_raw_len) && ((fw_raw_len - block_hdr_pos) > RT_FOTA_BLOCK_HEADER_SIZE))
		{
			rt_memcpy(block_hdr_buf, &crypt_buf[block_hdr_pos], RT_FOTA_BLOCK_HEADER_SIZE);
			block_size = block_hdr_buf[0] * (1 << 24) + block_hdr_buf[1] * (1 << 16) + block_hdr_buf[2] * (1 << 8) + block_hdr_buf[3];
			if ((fw_raw_len - block_hdr_pos - RT_FOTA_BLOCK_HEADER_SIZE) >= block_size)
			{
				rt_memset(cmprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE + padding_size);				
				rt_memcpy(cmprs_buff, &crypt_buf[block_hdr_pos + RT_FOTA_BLOCK_HEADER_SIZE], block_size);
				rt_memset(dcprs_buff, 0x0, RT_FOTA_CMPRS_BUFFER_SIZE);

				if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_FASTLZ) 
				{
					dcprs_size = fastlz_decompress((const void *)&cmprs_buff[0], block_size, &dcprs_buff[0], RT_FOTA_CMPRS_BUFFER_SIZE);
				}
				else if ((part_head->fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_QUICKLZ) 
				{
					dcprs_size = qlz_decompress((const char *)&cmprs_buff[0], &dcprs_buff[0], dcprs_state);
				}
			
				if (dcprs_size <= 0)
				{
					LOG_D("Decompress failed: %d.", dcprs_size);
					fota_err = RT_FOTA_GENERAL_ERR;
					goto __exit_upgrade;
				}

				if (rt_fota_write_app_part(total_copy_size, dcprs_buff, dcprs_size) < 0)
				{
					fota_err = RT_FOTA_COPY_FAILED;
					goto __exit_upgrade;
				}

				total_copy_size += dcprs_size;
				rt_kprintf("#");
			}
		}
	}
    rt_kprintf("\r\n");

	/* 有可能两个值不相等,因为AES需要填充16字节整数,但最后的解密解压值的代码数量必须是大于等于raw_size */
	/* 比较好的方法是做一个校验,目前打包软件的HASH_CODE算法不知道 */
	if (total_copy_size < part_head->raw_size)
	{
		LOG_D("Decompress check failed.");
		fota_err = RT_FOTA_GENERAL_ERR;
	}

__exit_upgrade:
	if (aes_ctx)
		rt_free(aes_ctx);

	if (aes_iv)
		rt_free(aes_iv);

	if (crypt_buf)
		rt_free(crypt_buf);

	if (cmprs_buff)
		rt_free(cmprs_buff);

	if (dcprs_buff)
		rt_free(dcprs_buff);

	if (dcprs_state)
		rt_free(dcprs_state);

	if (fota_err == RT_FOTA_NO_ERR)
	{
    	LOG_I("Upgrade success, total %d bytes.", total_copy_size);
	}
	return fota_err;
}

static int rt_fota_start_application(void)
{
	int fota_res = RT_FOTA_NO_ERR;
	const struct fal_partition *part;
	rt_uint32_t app_addr;

	part = fal_partition_find(RT_FOTA_APP_PART_NAME);
	if (part == RT_NULL)
	{		
		LOG_D("Partition[%s] not found.", fota_part_head.app_part_name);
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_start_application;
	}

	app_addr = part->offset + 0x08000000;
	//判断是否为0x08XXXXXX.
	if (((*(__IO uint32_t *)(app_addr + 4)) & 0xff000000) != 0x08000000)
	{
		LOG_I("Illegal Flash code.");
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_start_application;
	}
	// 检查栈顶地址是否合法.
	if (((*(__IO uint32_t *)app_addr) & 0x2ffe0000) != 0x20000000)	
	{
		LOG_I("Illegal Stack code.");
		fota_res = RT_FOTA_GENERAL_ERR;
		goto __exit_start_application;
	}

	LOG_I("Implement application now.");
    
    __disable_irq();
    HAL_DeInit();
	__set_CONTROL(0);
	//用户代码区第二个字为程序开始地址(复位地址)
	app_func = (rt_fota_app_func)*(__IO uint32_t *)(app_addr + 4);
	/* Configure main stack */
	__set_MSP(*(__IO uint32_t *)app_addr);
	/* jump to application */
	app_func();
	
__exit_start_application:
	LOG_I("Implement application failed.");
	return fota_res;
}

static rt_err_t rt_fota_get_shell_key(void)
{
	char ch;
	rt_err_t res = RT_EOK;
	rt_uint32_t timeout = RT_FOTA_GET_CHAR_WAITTIGN;
	rt_tick_t tick_start, tick_stop;

	RT_ASSERT(shell_dev != RT_NULL);
	RT_ASSERT(shell_sem != RT_NULL);

	rt_tick_set(0);
	tick_start = rt_tick_get();	
	while (1)
	{
		if (rt_device_read(shell_dev, -1, &ch, 1) != 1)
		{			
			if (rt_sem_take(shell_sem, timeout) != RT_EOK)
			{
				res = RT_ERROR;
				goto __exit_get_shell_key;
			}
		}		
		
		if (ch == 0x0d)
			goto __exit_get_shell_key;

		tick_stop = rt_tick_get();
		if ((tick_stop - tick_start) > RT_FOTA_GET_CHAR_WAITTIGN)
		{
			res = RT_ERROR;
			goto __exit_get_shell_key;
		}

		timeout = RT_FOTA_GET_CHAR_WAITTIGN - tick_stop + tick_start;
	}

__exit_get_shell_key:
	return res;
}

static rt_err_t rt_fota_rx_ind(rt_device_t dev, rt_size_t size)
{
	RT_ASSERT(shell_sem != RT_NULL);
    /* release semaphore to let fota thread rx data */
    rt_sem_release(shell_sem);

    return RT_EOK;
}

static rt_err_t rt_fota_set_device(const char *device_name)
{
	rt_device_t dev;

	RT_ASSERT(device_name != RT_NULL);
	
	dev = rt_device_find(device_name);
	if (dev == RT_NULL)
	{
		LOG_D("Can not find device: %s.", device_name);
		return RT_ERROR;
	}

	if (shell_sem)
		rt_sem_delete(shell_sem);
    
    shell_sem = rt_sem_create("shell_sem", 0, RT_IPC_FLAG_FIFO);
	if (shell_sem == RT_NULL)
		return RT_ERROR;

	if (dev == shell_dev)
		return RT_EOK;
	
	if (rt_device_open(dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX | RT_DEVICE_FLAG_STREAM) == RT_EOK)
    {
        if (shell_dev != RT_NULL)
        {
            /* close old finsh device */
            rt_device_close(shell_dev);
            rt_device_set_rx_indicate(shell_dev, RT_NULL);
        }

        shell_dev = dev;
        rt_device_set_rx_indicate(dev, rt_fota_rx_ind);
        
        LOG_D("Shell device %s open success.", device_name);
        return RT_EOK;
    }
	
	LOG_D("Shell device %s open failed.", device_name);
	return RT_ERROR;
}

static rt_err_t rt_fota_check_defalut_key(void)
{   
    int chk_idx;
    
	/* GPIO initialized */
	rt_pin_mode(RT_FOTA_DEFAULT_KEY_PIN, PIN_MODE_INPUT_PULLUP);
	/* Delay for power up */
	rt_thread_mdelay(500);

	if (rt_pin_read(RT_FOTA_DEFAULT_KEY_PIN) == PIN_LOW)
		rt_kprintf("Default firmware key pressed:\n");
	
	/* Check GPIO status */
	for (chk_idx = 0; (rt_pin_read(RT_FOTA_DEFAULT_KEY_PIN) == PIN_LOW) && (chk_idx < RT_FOTA_DEFAULT_KEY_CHK_TIME); chk_idx++)
	{
		rt_thread_mdelay(RT_TICK_PER_SECOND);
		rt_kprintf(">");
	}

	if (chk_idx < RT_FOTA_DEFAULT_KEY_CHK_TIME)
		return RT_ERROR;
	else
		return RT_EOK;
}

void rt_fota_thread_entry(void *arg)
{
	int fota_err = RT_FOTA_NO_ERR;
    extern int finsh_system_init(void);

	/* Signal led initialized */
	rt_fota_signal_led_init();

	/* Partition initialized */
	fota_err = rt_fota_boot_verify();
	if (fota_err != RT_FOTA_NO_ERR)
	{
		LOG_I("Partition initialized failed.");
	}

	/* Default key check */
	if (rt_fota_check_defalut_key() == RT_EOK)
	{
		/* enter to defalut key mode */
		rt_fota_signal_led_mode(led_upgrade_mode);
		goto __exit_default_entry;
	}

	/* Shell initialized */
	if (rt_fota_set_device(RT_CONSOLE_DEVICE_NAME) == RT_EOK)
	{
		rt_kprintf("Please press [Enter] key into shell mode in %d secs:\r\n", RT_FOTA_GET_CHAR_WAITTIGN / RT_TICK_PER_SECOND);
		if (rt_fota_get_shell_key() == RT_EOK)
		{	
			goto __exit_shell_entry;
		}
	}
	else
	{
		LOG_I("Shell device config failed.");
	}	

	/* Firmware partition verify */
	fota_err = rt_fota_part_fw_verify(RT_FOTA_FM_PART_NAME);
	if (fota_err != RT_FOTA_NO_ERR)
		goto __exit_boot_entry;

	/* Check upgrade status */
	if (rt_fota_check_upgrade() <= 0)
		goto __exit_boot_entry;

	/* enter to upgrade mode */
	rt_fota_signal_led_mode(led_upgrade_mode);

	/* Implement upgrade, copy firmware partition to app partition */
	fota_err = rt_fota_upgrade(RT_FOTA_FM_PART_NAME);
	if (fota_err != RT_FOTA_NO_ERR)
		goto __exit_boot_entry;

	/* Update new application verison in RBL file of firmware partition */
	fota_err = rt_fota_copy_version(RT_FOTA_FM_PART_NAME);	
	if (fota_err != RT_FOTA_NO_ERR)
		goto __exit_boot_entry;

__exit_boot_entry:
	/* Implement application */
	rt_fota_start_application();

__exit_default_entry:
	/* Implement upgrade, copy default partition to app partition */
	if (rt_fota_part_fw_verify(RT_FOTA_DF_PART_NAME) == RT_FOTA_NO_ERR)
	{
		if (rt_fota_upgrade(RT_FOTA_DF_PART_NAME) == RT_FOTA_NO_ERR)
		{		
			rt_fota_start_application();
		}
	}
	LOG_I("Boot application failed, entry shell mode.");
	
__exit_shell_entry:	
	/* enter to shell mode */
	rt_fota_signal_led_mode(led_shell_mode);
	/* Implement shell */
	finsh_system_init();
}

void rt_fota_init(void)
{
	rt_thread_t tid;

	tid = rt_thread_create("rt-boot", rt_fota_thread_entry, RT_NULL, RT_FOTA_THREAD_STACK_SIZE, RT_FOTA_THREAD_PRIORITY, 10);
	if (tid != RT_NULL)
	{
		rt_thread_startup(tid);
	}
	else
	{
		LOG_I("rt-fota thread create failed.");
	}
}

void rt_fota_info(rt_uint8_t argc, char **argv)
{
	char put_buf[24];
	char part_name[2][FAL_DEV_NAME_MAX] = 
    {
        {RT_FOTA_FM_PART_NAME}, 
        {RT_FOTA_DF_PART_NAME}
    };
		
	const char* help_info[] =
    {
            [0]     = "fota probe                       - probe RBL file of partiton",
            [1]     = "fota show partition addr size    - show 'size' bytes starting at 'addr'",
            [2]     = "fota clone des_part src_part     - clone src partition to des partiton",
            [3]     = "fota exec                        - execute application program",
    };

	if (argc < 2)
    {
        rt_kprintf("Usage:\n");
        for (int i = 0; i < sizeof(help_info) / sizeof(char*); i++)
        {
            rt_kprintf("%s\n", help_info[i]);
        }
        rt_kprintf("\n");
    }
    else
    {    	
    	const char *operator = argv[1];		
		if (!rt_strcmp(operator, "probe"))
		{
	    	for (int i = 0; i < 2; i++)
	    	{
	    		if (rt_fota_part_fw_verify(&part_name[i][0]) == RT_FOTA_NO_ERR)
		    	{
		    		LOG_I("===== RBL of %s partition =====", &part_name[i][0]);
		    		LOG_I("| App partition name | %*.s |", 11, fota_part_head.app_part_name);

					rt_memset(put_buf, 0x0, sizeof(put_buf));
					if ((fota_part_head.fota_algo & RT_FOTA_CRYPT_STAT_MASK) == RT_FOTA_CRYPT_ALGO_AES256)
					{
						rt_strncpy(put_buf, " AES", 4);
					}
					else if ((fota_part_head.fota_algo & RT_FOTA_CRYPT_STAT_MASK) == RT_FOTA_CRYPT_ALGO_XOR)
					{
						rt_strncpy(put_buf, " XOR", 4);
					}
                    else
                    {
                        rt_strncpy(put_buf, "NONE", 4);
                    }

					if ((fota_part_head.fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_GZIP)
					{
						rt_strncpy(&put_buf[rt_strlen(put_buf)], " && GLZ", 7);
					}
					else if ((fota_part_head.fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_QUICKLZ)
					{
						rt_strncpy(&put_buf[rt_strlen(put_buf)], " && QLZ", 7);
					}
					else if ((fota_part_head.fota_algo & RT_FOTA_CMPRS_STAT_MASK) == RT_FOTA_CMPRS_ALGO_FASTLZ)
					{
						rt_strncpy(&put_buf[rt_strlen(put_buf)], " && FLZ", 7);
					}

					if (rt_strlen(put_buf) <= 0)
					{
						rt_strncpy(put_buf, "None", 4);
					}
					LOG_I("| Algorithm mode     | %*.s |", 11, put_buf);
					LOG_I("| Firmware version   | %*.s |", 11, fota_part_head.download_version);
					LOG_I("| Code raw size      | %11d |", fota_part_head.raw_size);
	                LOG_I("| Code package size  | %11d |", fota_part_head.com_size);
					LOG_I("| Build Timestamp    | %11d |", *((rt_uint32_t *)(&fota_part_head.fm_time[2])));			
		    	}
	    	} 			
		}
       	else if (!rt_strcmp(operator, "show"))
       	{
       		const struct fal_partition *part;
       		const char *part_name = argv[2];
			
			rt_uint32_t addr = strtol(argv[3], NULL, 0);
			rt_uint32_t size = strtol(argv[4], NULL, 0);
			rt_uint8_t buf[16];
			
			
			part = fal_partition_find(part_name);
			if (part != RT_NULL)
			{
				while (size > 16)
				{
					fal_partition_read(part, addr, buf, 16);					
					
					rt_kprintf("%08X: ", addr);
					for (int i = 0; i < 16; i++)
					{
						rt_kprintf("%02X ", buf[i]);
					}
					rt_kprintf("\n");

					size -= 16;
					addr += 16;
				}

				fal_partition_read(part, addr, buf, size);
				rt_kprintf("%08X: ", addr);
				for (int i = 0; i < size; i++)
				{
					rt_kprintf("%02X ", buf[i]);
				}
				rt_kprintf("\n");
			}       
			else
			{
				rt_kprintf("%s partition is not exist!\n", part_name);
			}
       	}
		else if (!rt_strcmp(operator, "clone"))
		{
       		const char *dst_part_name = argv[2];
			const char *src_part_name = argv[3];
			const struct fal_partition *dst_part;
			const struct fal_partition *src_part;

			dst_part = fal_partition_find(dst_part_name);
			src_part = fal_partition_find(src_part_name);
			if (dst_part == RT_NULL || src_part == RT_NULL)
			{
				if (dst_part == RT_NULL)
					rt_kprintf("%s partition is not exist!\n", dst_part_name);

				if (src_part == RT_NULL)
					rt_kprintf("%s partition is not exist!\n", src_part_name);
			}
			else
			{
				rt_kprintf("Clone %s partition to %s partition:\n", src_part_name, dst_part_name);
				if (fal_partition_erase(dst_part, 0, dst_part->len) >= 0)
				{
					int clone_pos = 0;
					int clone_len = 0, clone_tol_len;
					rt_uint8_t *buf = rt_malloc(4096);

					if (dst_part->len < src_part->len)
						clone_tol_len = dst_part->len;
					else
						clone_tol_len = src_part->len;
					
					while ((clone_pos < clone_tol_len) && (buf != RT_NULL))
					{
						clone_len = fal_partition_read(src_part, clone_pos, buf, 4096);
						if (clone_len < 0)
						{
							rt_kprintf("\nread %s partition failed, clone stop!\n", src_part_name);
							break;
						}
						
						if (fal_partition_write(dst_part, clone_pos, buf, clone_len) < 0)
						{
							rt_kprintf("\nwrite %s partition failed, clone stop!\n", dst_part_name);
							break;
						}

						rt_kprintf("#");
						clone_pos += clone_len;
					}
					
					if (clone_pos >= clone_tol_len)
						rt_kprintf("\nClone partition success, total %d bytes!\n", clone_tol_len);	
					else
						rt_kprintf("\nClone partition failed!\n");

					if (buf)
						rt_free(buf);
				}
			}
		}
		else if (!rt_strcmp(operator, "exec"))
		{
			rt_fota_start_application();
		}
		else
		{
			rt_kprintf("Usage:\n");
	        for (int i = 0; i < sizeof(help_info) / sizeof(char*); i++)
	        {
	            rt_kprintf("%s\n", help_info[i]);
	        }
	        rt_kprintf("\n");
		}
    }
}
/**
 * rt-fota />ymodem_ota
*/
MSH_CMD_EXPORT_ALIAS(rt_fota_info, fota, Check RBL file of partition);


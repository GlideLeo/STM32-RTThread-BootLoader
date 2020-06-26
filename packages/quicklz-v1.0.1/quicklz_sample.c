/*
 * File : quicklz_test.c
 * this example is a very simple test program for the quicklz library,
 * using non-stream compress and decompress. If you want to use stream compress,
 * you need at least 100K of ROM for history buffer(not recommend), or you can custom  
 * header to storage the compress block size, and carry out stream compress by non-stream.
 *
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
 * Date          Author          Notes
 * 2018-02-05    chenyong     first version
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>

#include <dfs_posix.h>

#include "quicklz.h"

#define malloc     rt_malloc
#define free       rt_free

#define BLOCK_HEADER_SIZE              4

#define COMPRESS_BUFFER_SIZE           4096
#define DCOMPRESS_BUFFER_SIZE          4096

/* Buffer padding for destination buffer, least size + 400 bytes large because incompressible data may increase in size. */
#define BUFFER_PADDING                 QLZ_BUFFER_PADDING

#if QLZ_STREAMING_BUFFER != 0
    #error Define QLZ_STREAMING_BUFFER to a zero value for this demo
#endif

static int quicklz_compress_file(int fd_in, int fd_out)
{
    /* Start to compress file  */
    qlz_state_compress *state_compress = RT_NULL;
    rt_uint8_t *cmprs_buffer = RT_NULL, *buffer = RT_NULL;
    rt_uint8_t buffer_hdr[BLOCK_HEADER_SIZE] = { 0 };
    size_t cmprs_size = 0, block_size = 0, totle_cmprs_size = 0;
    size_t file_size = 0, i = 0;
    int ret = 0;

    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    cmprs_buffer = (rt_uint8_t *) malloc(COMPRESS_BUFFER_SIZE + BUFFER_PADDING);
    buffer = (rt_uint8_t *) malloc(COMPRESS_BUFFER_SIZE);
    if (!cmprs_buffer || !buffer)
    {
        rt_kprintf("[qlz] No memory for cmprs_buffer or buffer!\n");
        ret = -1;
        goto _exit;
    }

    state_compress = (qlz_state_compress *) malloc(sizeof(qlz_state_compress));
    if (!state_compress)
    {
        rt_kprintf("[qlz] No memory for state_compress struct, need %d byte, or you can change QLZ_HASH_VALUES to 1024 !\n",
                sizeof(qlz_state_compress));
        ret = -1;
        goto _exit;
    }
    memset(state_compress, 0x00, sizeof(qlz_state_compress));

    rt_kprintf("[qlz]compress start : ");
    for (i = 0; i < file_size; i += COMPRESS_BUFFER_SIZE)
    {
        if ((file_size - i) < COMPRESS_BUFFER_SIZE)
        {
            block_size = file_size - i;
        }
        else
        {
            block_size = COMPRESS_BUFFER_SIZE;
        }

        memset(buffer, 0x00, COMPRESS_BUFFER_SIZE);
        memset(cmprs_buffer, 0x00, COMPRESS_BUFFER_SIZE + BUFFER_PADDING);

        read(fd_in, buffer, block_size);

        /* The destination buffer must be at least size + 400 bytes large because incompressible data may increase in size. */
        cmprs_size = qlz_compress(buffer, (char *) cmprs_buffer, block_size, state_compress);

        /* Store compress block size to the block header (4 byte). */
        buffer_hdr[3] = cmprs_size % (1 << 8);
        buffer_hdr[2] = (cmprs_size % (1 << 16)) / (1 << 8);
        buffer_hdr[1] = (cmprs_size % (1 << 24)) / (1 << 16);
        buffer_hdr[0] = cmprs_size / (1 << 24);

        write(fd_out, buffer_hdr, BLOCK_HEADER_SIZE);
        write(fd_out, cmprs_buffer, cmprs_size);

        totle_cmprs_size += cmprs_size + BLOCK_HEADER_SIZE;
        rt_kprintf(">");
    }

    rt_kprintf("\n");
    rt_kprintf("[qlz]compressed %d bytes into %d bytes , compression ratio is %d%!\n", file_size, totle_cmprs_size,
            (totle_cmprs_size * 100) / file_size);
_exit:
    if (cmprs_buffer)
    {
        free(cmprs_buffer);
    }

    if (buffer)
    {
        free(buffer);
    }

    if (state_compress)
    {
        free(state_compress);
    }

    return ret;
}


static int quicklz_decompress_file(int fd_in, int fd_out)
{
    /* Start to decompress file  */
    qlz_state_decompress *state_decompress = RT_NULL;
    rt_uint8_t *dcmprs_buffer = RT_NULL, *buffer = RT_NULL;
    rt_uint8_t buffer_hdr[BLOCK_HEADER_SIZE] = { 0 };
    size_t dcmprs_size = 0, block_size = 0, total_dcmprs_size = 0;
    size_t file_size = 0, i = 0;
    int ret = 0;

    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    if (file_size <= BLOCK_HEADER_SIZE)
    {
        rt_kprintf("[qlz] decomprssion file size : %d error!\n", file_size);
        ret = -1;
        goto _dcmprs_exit;
    }

    dcmprs_buffer = (rt_uint8_t *) malloc(DCOMPRESS_BUFFER_SIZE);
    buffer = (rt_uint8_t *) malloc(DCOMPRESS_BUFFER_SIZE + BUFFER_PADDING);
    if (!dcmprs_buffer || !buffer)
    {
        rt_kprintf("[qlz] No memory for dcmprs_buffer or buffer!\n");
        ret = -1;
        goto _dcmprs_exit;
    }

    state_decompress = (qlz_state_decompress *) malloc(sizeof(qlz_state_decompress));
    if (!state_decompress)
    {
        rt_kprintf("[qlz] No memory for state_decompress struct!\n");
        ret = -1;
        goto _dcmprs_exit;
    }
    memset(state_decompress, 0x00, sizeof(qlz_state_decompress));

    rt_kprintf("[qlz]decompress start : ");
    for (i = 0; i < file_size; i += BLOCK_HEADER_SIZE + block_size)
    {
        /* Get the decompress block size from the block header. */
        read(fd_in, buffer_hdr, BLOCK_HEADER_SIZE);
        block_size = buffer_hdr[0] * (1 << 24) + buffer_hdr[1] * (1 << 16) + buffer_hdr[2] * (1 << 8) + buffer_hdr[3];

        memset(buffer, 0x00, COMPRESS_BUFFER_SIZE + BUFFER_PADDING);
        memset(dcmprs_buffer, 0x00, DCOMPRESS_BUFFER_SIZE);

        read(fd_in, buffer, block_size);

        dcmprs_size = qlz_decompress((const char *) buffer, dcmprs_buffer, state_decompress);
        write(fd_out, dcmprs_buffer, dcmprs_size);

        total_dcmprs_size += dcmprs_size;
        rt_kprintf(">");
    }
    rt_kprintf("\n");
    rt_kprintf("decompressed %d bytes into %d bytes !\n", file_size, total_dcmprs_size);

_dcmprs_exit:
    if (dcmprs_buffer)
    {
        free(dcmprs_buffer);
    }

    if(buffer)
    {
        free(buffer);
    }

    if (state_decompress)
    {
        free(state_decompress);
    }

    return ret;
}

int quicklz_test(int argc, char ** argv)
{
    int fd_in = -1 , fd_out = -1;
    int ret  = 0;

    if (argc != 4)
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("qlz_test -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
        rt_kprintf("qlz_test -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n");
        
        ret = -1;
        goto _exit;
    }

    fd_in = open(argv[2], O_RDONLY, 0);
    if (fd_in < 0)
    {
        rt_kprintf("[qlz] open the input file : %s error!\n", argv[2]);
        ret = -1;
        goto _exit;
    }

    fd_out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0);
    if (fd_out < 0)
    {
        rt_kprintf("[qlz] open the output file : %s error!\n", argv[3]);
        ret = -1;
        goto _exit;
    }

    if(memcmp("-c", argv[1], strlen(argv[1])) == 0)
    {

        if(quicklz_compress_file(fd_in, fd_out) < 0)
        {
            rt_kprintf("[qlz] quciklz compress file error!\n");
        }

    }
    else if(memcmp("-d", argv[1], strlen(argv[1])) == 0)
    {

        if(quicklz_decompress_file(fd_in, fd_out) < 0)
        {
            rt_kprintf("[qlz] quciklz decompress file error!\n");
        }
    }
    else
    {
        rt_kprintf("Usage:\n");
        rt_kprintf("qlz_test -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
        rt_kprintf("qlz_test -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n");
        
        ret = -1;
        goto _exit;
    }

_exit:
    if(fd_in >= 0)
    {
        close(fd_in);
    }

    if(fd_out >= 0)
    {
        close(fd_out);
    }

    return ret;
}
 
#ifdef RT_USING_FINSH
#ifdef FINSH_USING_MSH

#include <finsh.h>

MSH_CMD_EXPORT_ALIAS(quicklz_test, qlz_test, quicklz compress and decompress test);
#endif
#endif

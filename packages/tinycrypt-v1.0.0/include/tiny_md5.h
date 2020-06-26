/**
 * \file md5.h
 *
 *  Based on TropicSSL: Copyright (C) 2017 Shanghai Real-Thread Technology Co., Ltd
 * 
 *  Based on XySSL: Copyright (C) 2006-2008  Christophe Devine
 *
 *  Copyright (C) 2009  Paul Bakker <polarssl_maintainer at polarssl dot org>
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the names of PolarSSL or XySSL nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef TINY_CRYPT_MD5_H__
#define TINY_CRYPT_MD5_H__

/**
 * \brief          MD5 context structure
 */
typedef struct {
	unsigned long total[2];	/*!< number of bytes processed  */
	unsigned long state[4];	/*!< intermediate digest state  */
	unsigned char buffer[64];	/*!< data block being processed */

	unsigned char ipad[64];	/*!< HMAC: inner padding        */
	unsigned char opad[64];	/*!< HMAC: outer padding        */
} tiny_md5_context;

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * \brief          MD5 context setup
	 *
	 * \param ctx      context to be initialized
	 */
	void tiny_md5_starts(tiny_md5_context * ctx);

	/**
	 * \brief          MD5 process buffer
	 *
	 * \param ctx      MD5 context
	 * \param input    buffer holding the  data
	 * \param ilen     length of the input data
	 */
	void tiny_md5_update(tiny_md5_context * ctx, unsigned char *input, int ilen);

	/**
	 * \brief          MD5 final digest
	 *
	 * \param ctx      MD5 context
	 * \param output   MD5 checksum result
	 */
	void tiny_md5_finish(tiny_md5_context * ctx, unsigned char output[16]);

	/**
	 * \brief          Output = MD5( input buffer )
	 *
	 * \param input    buffer holding the  data
	 * \param ilen     length of the input data
	 * \param output   MD5 checksum result
	 */
	void tiny_md5(unsigned char *input, int ilen, unsigned char output[16]);

	/**
	 * \brief          MD5 HMAC context setup
	 *
	 * \param ctx      HMAC context to be initialized
	 * \param key      HMAC secret key
	 * \param keylen   length of the HMAC key
	 */
	void tiny_md5_hmac_starts(tiny_md5_context * ctx, unsigned char *key, int keylen);

	/**
	 * \brief          MD5 HMAC process buffer
	 *
	 * \param ctx      HMAC context
	 * \param input    buffer holding the  data
	 * \param ilen     length of the input data
	 */
	void tiny_md5_hmac_update(tiny_md5_context * ctx, unsigned char *input, int ilen);

	/**
	 * \brief          MD5 HMAC final digest
	 *
	 * \param ctx      HMAC context
	 * \param output   MD5 HMAC checksum result
	 */
	void tiny_md5_hmac_finish(tiny_md5_context * ctx, unsigned char output[16]);

	/**
	 * \brief          Output = HMAC-MD5( hmac key, input buffer )
	 *
	 * \param key      HMAC secret key
	 * \param keylen   length of the HMAC key
	 * \param input    buffer holding the  data
	 * \param ilen     length of the input data
	 * \param output   HMAC-MD5 result
	 */
	void tiny_md5_hmac(unsigned char *key, int keylen,
		      unsigned char *input, int ilen, unsigned char output[16]);

#ifdef __cplusplus
}
#endif
#endif				/* md5.h */

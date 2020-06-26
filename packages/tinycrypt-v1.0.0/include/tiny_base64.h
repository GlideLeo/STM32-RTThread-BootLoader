/**
 * \file base64.h
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
#ifndef TINY_CRYPT_BASE64_H__
#define TINY_CRYPT_BASE64_H__

#define ERR_BASE64_BUFFER_TOO_SMALL               -0x0010
#define ERR_BASE64_INVALID_CHARACTER              -0x0012

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * \brief          Encode a buffer into base64 format
	 *
	 * \param dst      destination buffer
	 * \param dlen     size of the buffer
	 * \param src      source buffer
	 * \param slen     amount of data to be encoded
	 *
	 * \return         0 if successful, or TROPICSSL_ERR_BASE64_BUFFER_TOO_SMALL.
	 *                 *dlen is always updated to reflect the amount
	 *                 of data that has (or would have) been written.
	 *
	 * \note           Call this function with *dlen = 0 to obtain the
	 *                 required buffer size in *dlen
	 */
	int tiny_base64_encode(unsigned char *dst, int *dlen,
			  unsigned char *src, int slen);

	/**
	 * \brief          Decode a base64-formatted buffer
	 *
	 * \param dst      destination buffer
	 * \param dlen     size of the buffer
	 * \param src      source buffer
	 * \param slen     amount of data to be decoded
	 *
	 * \return         0 if successful, TROPICSSL_ERR_BASE64_BUFFER_TOO_SMALL, or
	 *                 TROPICSSL_ERR_BASE64_INVALID_DATA if the input data is not
	 *                 correct. *dlen is always updated to reflect the amount
	 *                 of data that has (or would have) been written.
	 *
	 * \note           Call this function with *dlen = 0 to obtain the
	 *                 required buffer size in *dlen
	 */
	int tiny_base64_decode(unsigned char *dst, int *dlen,
			  unsigned char *src, int slen);

#ifdef __cplusplus
}
#endif
#endif				/* base64.h */

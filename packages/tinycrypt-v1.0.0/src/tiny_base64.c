/*
 *	RFC 1521 base64 encoding/decoding
 *
 *  Based on TropicSSL: Copyright (C) 2017 Shanghai Real-Thread Technology Co., Ltd
 * 
 *	Based on XySSL: Copyright (C) 2006-2008	 Christophe Devine
 *
 *	Copyright (C) 2009	Paul Bakker <polarssl_maintainer at polarssl dot org>
 *
 *	All rights reserved.
 *
 *	Redistribution and use in source and binary forms, with or without
 *	modification, are permitted provided that the following conditions
 *	are met:
 *
 *	  * Redistributions of source code must retain the above copyright
 *		notice, this list of conditions and the following disclaimer.
 *	  * Redistributions in binary form must reproduce the above copyright
 *		notice, this list of conditions and the following disclaimer in the
 *		documentation and/or other materials provided with the distribution.
 *	  * Neither the names of PolarSSL or XySSL nor the names of its contributors
 *		may be used to endorse or promote products derived from this software
 *		without specific prior written permission.
 *
 *	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *	"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *	FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *	OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *	SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *	TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "tinycrypt_config.h"

#if defined(TINY_CRYPT_BASE64)

#include "tinycrypt.h"

static const unsigned char base64_enc_map[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
	'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', '+', '/'
};

static const unsigned char base64_dec_map[128] = {
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 62, 127, 127, 127, 63, 52, 53,
	54, 55, 56, 57, 58, 59, 60, 61, 127, 127,
	127, 64, 127, 127, 127, 0, 1, 2, 3, 4,
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 127, 127, 127, 127, 127, 127, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
	39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 127, 127, 127, 127, 127
};

/*
 * Encode a buffer into base64 format
 */
int tiny_base64_encode(unsigned char *dst, int *dlen, unsigned char *src, int slen)
{
	int i, n;
	int C1, C2, C3;
	unsigned char *p;

	if (slen == 0)
		return (0);

	n = (slen << 3) / 6;

	switch ((slen << 3) - (n * 6)) {
	case 2:
		n += 3;
		break;
	case 4:
		n += 2;
		break;
	default:
		break;
	}

	if (*dlen < n + 1) {
		*dlen = n + 1;
		return (ERR_BASE64_BUFFER_TOO_SMALL);
	}

	n = (slen / 3) * 3;

	for (i = 0, p = dst; i < n; i += 3) {
		C1 = *src++;
		C2 = *src++;
		C3 = *src++;

		*p++ = base64_enc_map[(C1 >> 2) & 0x3F];
		*p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
		*p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
		*p++ = base64_enc_map[C3 & 0x3F];
	}

	if (i < slen) {
		C1 = *src++;
		C2 = ((i + 1) < slen) ? *src++ : 0;

		*p++ = base64_enc_map[(C1 >> 2) & 0x3F];
		*p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];

		if ((i + 1) < slen)
			*p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
		else
			*p++ = '=';

		*p++ = '=';
	}

	*dlen = p - dst;
	*p = 0;

	return (0);
}

/*
 * Decode a base64-formatted buffer
 */
int tiny_base64_decode(unsigned char *dst, int *dlen, unsigned char *src, int slen)
{
	int i, j, n;
	unsigned long x;
	unsigned char *p;

	for (i = j = n = 0; i < slen; i++) {
		if ((slen - i) >= 2 && src[i] == '\r' && src[i + 1] == '\n')
			continue;

		if (src[i] == '\n')
			continue;

		if (src[i] == '=' && ++j > 2)
			return (ERR_BASE64_INVALID_CHARACTER);

		if (src[i] > 127 || base64_dec_map[src[i]] == 127)
			return (ERR_BASE64_INVALID_CHARACTER);

		if (base64_dec_map[src[i]] < 64 && j != 0)
			return (ERR_BASE64_INVALID_CHARACTER);

		n++;
	}

	if (n == 0)
		return (0);

	n = ((n * 6) + 7) >> 3;

	if (*dlen < n) {
		*dlen = n;
		return (ERR_BASE64_BUFFER_TOO_SMALL);
	}

	for (j = 3, n = x = 0, p = dst; i > 0; i--, src++) {
		if (*src == '\r' || *src == '\n')
			continue;

		j -= (base64_dec_map[*src] == 64);
		x = (x << 6) | (base64_dec_map[*src] & 0x3F);

		if (++n == 4) {
			n = 0;
			if (j > 0)
				*p++ = (unsigned char)(x >> 16);
			if (j > 1)
				*p++ = (unsigned char)(x >> 8);
			if (j > 2)
				*p++ = (unsigned char)(x);
		}
	}

	*dlen = p - dst;

	return (0);
}

#endif

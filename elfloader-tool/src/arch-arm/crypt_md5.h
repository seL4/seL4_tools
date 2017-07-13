/*
 * Copyright 2017, DornerWorks
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(DORNERWORKS_GPL)
 *
 * This data was produced by DornerWorks, Ltd. of Grand Rapids, MI, USA under
 * a DARPA SBIR, Contract Number D16PC00107.
 *
 * Approved for Public Release, Distribution Unlimited.
 *
 */

#ifndef _CRYPT_MD5_H
#define _CRYPT_MD5_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint64_t len;    /* processed message length */
	uint32_t h[4];   /* hash state */
	uint8_t buf[64]; /* message block buffer */
} md5_t;

void md5_init(md5_t *s);
void md5_sum(md5_t *s, uint8_t *md);
void md5_update(md5_t *s, const void *m, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif

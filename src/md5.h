/**
 * MD5
 *
 * $Id$
 */

#ifndef __XS_MD5_20090527_H__
#define	__XS_MD5_20090527_H__

#ifdef __cplusplus
extern "C" {
#endif

char *md5(const char *str); // non-thread safe
char *md5_r(const char *str, char *buf); // thread safe

#ifdef __cplusplus
}
#endif

#endif	/* __XS_MD5_20090527_H__ */

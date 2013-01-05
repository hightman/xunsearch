/**
 * Header file for pinyin convertor
 *
 * $Id: $
 */

#ifndef __XS_PINYIN_20110731_H__
#define	__XS_PINYIN_20110731_H__

#ifdef __cplusplus
extern "C" {
#endif

/* pinyin list struct */
typedef struct py_list
{
	int flag;
	struct py_list *next;
	char py[0];
} py_list;

#define	PY_ILLEGAL(p)	((p)->flag & 0x01)
#define	PY_ZEROSM(p)	((p)->flag & 0x02)
#define	PY_CHINESE(p)	((p)->flag & 0x04)

/* pinyin segment */
py_list *py_segment(const char *str, int len);

/* load pinyin dict for conversion */
int py_dict_load(const char *fpath);

/* unload pinyin dict for conversion */
void py_dict_unload();

/* pinyin convert */
py_list *py_convert(const char *str, int len);

/* pinyin fuzzy fix */
py_list *py_fuzzy_fix(py_list *pl);

/* free pinyin list */
void py_list_free(py_list *pl);

#ifdef __cplusplus
}
#endif

#endif	/* __XS_PINYIN_20110731_H__ */

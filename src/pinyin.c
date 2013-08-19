/**
 * Convert chinese characters to pinyin
 * Pinyin database is XDB format:
 * key is chinese (max: 4words), value is pinyin
 * 
 * $Id: $
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <string.h>
#include <stdlib.h>
#include <scws/xdb.h>

#include "pinyin.h"

#define	CHINESE_CHAR_SIZE		3
#define	PINYIN_WORD_MAXSIZE		12

/**
 * Pinyin xdb
 */
static xtree_t px;
/**
 * SM: +zh, ch, sh
 */
static char *sm_table = "bpmfdtnlgkhjqxzcsryw"; // zh,ch,sh

/**
 * YM: sorted, find use binary-search
 */
struct yunmu
{
	const char *ym;
	int flag;
};
static struct yunmu ym_table[] = {
	{"a", 7},
	{"ai", 5},
	{"an", 7},
	{"ang", 5},
	{"ao", 5},
	{"e", 7},
	{"ei", 5},
	{"en", 7},
	{"eng", 1},
	{"er", 5},
	{"i", 3},
	{"ia", 3},
	{"ian", 3},
	{"iang", 1},
	{"iao", 1},
	{"ie", 1},
	{"in", 3},
	{"ing", 1},
	{"io", 2},
	{"ion", 2},
	{"iong", 1},
	{"iou", 1},
	{"iu", 1},
	{"o", 7},
	{"on", 2},
	{"ong", 1},
	{"ou", 5},
	{"u", 3},
	{"ua", 3},
	{"uai", 1},
	{"uan", 3},
	{"uang", 1},
	{"ue", 3},
	{"uei", 1},
	{"uen", 3},
	{"ueng", 1},
	{"ui", 1},
	{"un", 1},
	{"uo", 1},
	{"v", 3},
	{"ve", 1}
};

/**
 * Get ym flag
 * @return zero if not found
 */
static int ym_flag(const char *ym, int len)
{
	int high, low, mid, cmp;

	high = sizeof(ym_table) / sizeof(struct yunmu) - 1;
	low = 0;
	while (low <= high) {
		mid = (high + low) >> 1;
		cmp = strncmp(ym, ym_table[mid].ym, len);
		if (cmp == 0) {
			cmp = len - strlen(ym_table[mid].ym);
		}

		if (cmp > 0) {
			low = mid + 1;
		} else if (cmp < 0) {
			high = mid - 1;
		} else {
			return ym_table[mid].flag;
		}
	}
	return 0;
}

/**
 * Append new py list
 */
static inline void py_list_append(py_list **pl, py_list *join)
{
	if (*pl == NULL) {
		*pl = join;
	} else {
		py_list *tail = *pl;
		while (tail->next != NULL) {
			tail = tail->next;
		}
		tail->next = join;
	}
}

/**
 * Make new py list item
 * size: +3 (keep enough size for py_fixed, +h +ng)
 */
static inline py_list *py_list_item(const char *py, int len)
{
	py_list *cur = (py_list *) malloc(sizeof(struct py_list) +len + 4);
	if (cur != NULL) {
		memset(cur, 0, sizeof(struct py_list) +len + 4);
		memcpy(cur->py, py, len);
		if (!strchr(sm_table, *py)) {
			cur->flag |= 0x02; // zero sm
		}
	}
	return cur;
}

/**
 * Pure pinyin segment 
 */
py_list *py_segment(const char *str, int len)
{
	int i, j, k, yf, yf0;
	py_list *cur, *ret = NULL;

	for (i = 0; i < len; i++) {
		// back slash
		if (str[i] == '\'' || str[i] <= ' ') {
			continue;
		}

		// fetch SM: i~j
		j = i;
		if (strchr(sm_table, str[i]) != NULL) {
			j++;
			if ((str[i] == 'z' || str[i] == 'c' || str[i] == 's') && j != len && str[j] == 'h') {
				j++;
			}
		}

		// fetch YM: j~k
		for (k = j + 1, yf0 = 0; k <= len; k++) {
			yf = ym_flag(str + j, k - j);

			// invaid ym
			if (yf == 0 || (j == i && !(yf & 0x04))) {
				k--;
				break;
			}
			// end with SM char, & next is not SM char
			if (strchr(sm_table, str[k - 1]) && k != len /* && (yf0 & 0x01) */
					&& !strchr(sm_table, str[k]) && ym_flag(str + k, 1) != 0) {
				k--;
				break;
			}
			yf0 = yf;
			if (!(yf & 0x02) || k == len) {
				break;
			}
		}

		// save PY
		if (j >= len || k == j /* || !(yf0 & 0x01) */) {
			k = len;
			yf0 = 0;
		}
		cur = py_list_item(str + i, k - i);
		py_list_append(&ret, cur);
		if (yf0 == 0) {
			cur->flag = 0x01;
		}

		// reset offset
		i = k - 1;
	}
	return ret;
}

/**
 * Unload pinyin dict
 */
void py_dict_unload()
{
	if (px != NULL) {
		xtree_free(px);
		px = NULL;
	}
}

/**
 * Load pinyin dict
 * @return zero on success, -1 on failure
 */
int py_dict_load(const char *fpath)
{
	xdb_t xx;

	py_dict_unload();
	if ((xx = xdb_open(fpath, 'r')) != NULL) {
		px = xdb_to_xtree(xx, NULL);
		xdb_close(xx);
	}
	return px == NULL ? -1 : 0;
}

/**
 * Query pinyin from py_dict
 */
const char *py_dict_find(const char *key, int len, int *vlen)
{
	return px == NULL ? NULL : (const char *) xtree_nget(px, key, len, vlen);
}

/**
 * Pinyin convert 
 */
py_list *py_convert(const char *str, int len)
{
	unsigned char *buf = (unsigned char *) str;
	py_list *cur, *ret = NULL;
	int i, j = 0;

	for (i = 0; i < len; i++) {
		// dirty chars
		if (buf[i] <= ' ' || ((buf[i] & 0x80) && (buf[i] & 0xe0) != 0xe0)) {
			continue;
		}
		if ((buf[i] & 0x80) == 0) {
			py_list *tail;

			// single bytes
			for (j = i + 1; j < len; j++) {
				if (buf[j] <= ' ' || (buf[j] & 0x80)) {
					break;
				}
			}

			// save data (try to check full pinyin or not)
			cur = py_segment(str + i, j - i);
			if (cur == NULL) {
				continue;
			}
			for (tail = cur; tail->next != NULL; tail = tail->next);
			if (tail->flag & 0x01) {
				py_list_free(cur);
				cur = py_list_item(str + i, j - i);
				cur->flag = 0x01;
			}
			py_list_append(&ret, cur);
		} else if ((buf[i] & 0xe0) == 0xe0) {
			// multi bytes			
			const char *p;
			int k, v;

			j = i + CHINESE_CHAR_SIZE;
			while (j < len && (buf[j] & 0xe0) == 0xe0) {
				j += CHINESE_CHAR_SIZE;
			}
			// get pinyin
			for (; i < j; i += CHINESE_CHAR_SIZE) {
				k = (i + PINYIN_WORD_MAXSIZE) > j ? j : (i + PINYIN_WORD_MAXSIZE);
				do {
					p = py_dict_find(str + i, k - i, &v);
					k = k - CHINESE_CHAR_SIZE;
					if (p == NULL) {
						continue;
					}
					if (k == i) {
						cur = py_list_item(p, v);
					} else {
						cur = py_segment(p, v);
						i = k;
					}
					py_list_append(&ret, cur);
					while (cur != NULL) {
						cur->flag |= 0x04; // chinese
						cur = cur->next;
					}
				} while (k > i);
			}
		}
		i = j - 1;
	}
	return ret;
}

#define	PY_FUZZY_TYPE1(x)	((x[0] == 'z' || x[0] == 'c' || x[0] == 's') && x[1] != 'h')
#define	PY_FUZZY_TYPE2(x,t)	(t > x && *t == 'n' && (t[-1] == 'a' || t[-1] == 'e' || t[-1] == 'i'))
#define	PY_FUZZY_TYPE30(x)	(x[1] == 'a' || x[1] == 'o' || x[1] == 'i')
#define	PY_FUZZY_TYPE3(x)	(x[0] == 'b')
#define	PY_FUZZY_TYPE83(x)	(x[0] == 'p')
#define	PY_FUZZY_TYPE4(x)	(x[0] == 'l')
#define	PY_FUZZY_TYPE84(x)	(x[0] == 'n')
#define	PY_FUZZY_TYPE5(x)	((x[0] == 'z' || x[0] == 'c' || x[0] == 's') && x[1] == 'h')
#define	PY_FUZZY_TYPE6(x,t)	((t - x) > 1 && *t == 'g' && t[-1] == 'n' && t[-2] != 'o')

static int get_fuzzy_type(const char *py)
{
	if (PY_FUZZY_TYPE1(py)) {
		return 1;
	} else {
		char *tail = (char *) py + strlen(py) - 1;

		if (PY_FUZZY_TYPE2(py, tail)) {
			return 2;
		}
		if (PY_FUZZY_TYPE30(py)) {
			if (PY_FUZZY_TYPE3(py)) {
				return 3;
			}
			if (PY_FUZZY_TYPE83(py)) {
				return 0x83;
			}
		}
		if (PY_FUZZY_TYPE4(py)) {
			return 4;
		}
		if (PY_FUZZY_TYPE84(py)) {
			return 0x84;
		}
		if (PY_FUZZY_TYPE5(py)) {
			return 5;
		}
		if (PY_FUZZY_TYPE6(py, tail)) {
			return 6;
		}
	}
	return 0;
}

static int do_fuzzy_fix(char *py, int fuzzy)
{
	int fixed = 0;
	char *tail = py + strlen(py) - 1;

	// force: io->iong, on->ong
	if (tail > py) {
		if (*tail == 'n' && tail[-1] == 'o') {
			*++tail = 'g';
			tail[1] = '\0';
			return 1;
		} else if (*tail == 'o' && tail[-1] == 'i') {
			*++tail = 'n';
			*++tail = 'g';
			tail[1] = '\0';
			return 1;
		}
	}
	// fixed fuzzy	
	switch (fuzzy) {
		case 1:
			if (PY_FUZZY_TYPE1(py)) {
				do {
					tail[1] = *tail;
				} while (--tail > py);
				tail[1] = 'h';
				fixed = 1;
			}
			break;
		case 2:
			if (PY_FUZZY_TYPE2(py, tail)) {
				tail[1] = 'g';
				fixed = 1;
			}
			break;
		case 3:
			if (PY_FUZZY_TYPE3(py) && PY_FUZZY_TYPE30(py)) {
				*py = 'p';
				fixed = 1;
			}
			break;
		case 0x83:
			if (PY_FUZZY_TYPE83(py) && PY_FUZZY_TYPE30(py)) {
				*py = 'b';
				fixed = 1;
			}
			break;
		case 4:
			if (PY_FUZZY_TYPE4(py)) {
				*py = 'n';
				fixed = 1;
			}
			break;
		case 0x84:
			if (PY_FUZZY_TYPE84(py)) {
				*py = 'l';
				fixed = 1;
			}
			break;
		case 5:
			if (PY_FUZZY_TYPE5(py)) {
				char *sec = py + 1;
				do {
					*sec = sec[1];
				} while (++sec <= tail);
				fixed = 1;
			}
			break;
		case 6:
			if (PY_FUZZY_TYPE6(py, tail)) {
				*tail = '\0';
				fixed = 1;
			}
			break;
	}
	return fixed;
}

/**
 * Py list fuzzy fix
 * 1.z-zh, c-ch, s-sh
 * 2.an-ang, en-eng, in-ing
 * 3.b-p/(p-l) [req: +o, +a, +i] 
 * 4.l-n/(n-l)
 * 5.zh-z, ch-c, sh-c
 * 6.ang-an, eng-en, ing-in
 * TODO: qi-qu, ji-ju, hou-huo, fu-hu, fan-huan, rou-lou, yin-yun ...
 * @param pl
 * @return 
 */
py_list *py_fuzzy_fix(py_list *pl)
{
	py_list *cur;
	int cc, fuzzy = 0;

	// find fuzzy type
	for (cur = pl; cur != NULL; cur = cur->next) {
		if (PY_ILLEGAL(cur)) {
			continue;
		}
		if ((cc = get_fuzzy_type(cur->py)) == 0) {
			continue;
		}
		if (fuzzy == 0 || ((cc & 0x7f) < (fuzzy & 0x7f))) {
			fuzzy = cc;
		}
		if (fuzzy == 1) {
			break;
		}
	}
	// process fuzzy
	for (cc = 0, cur = pl; cur != NULL; cur = cur->next) {
		if (PY_ILLEGAL(cur)) {
			continue;
		}
		cc += do_fuzzy_fix(cur->py, fuzzy);
	}
	return cc > 0 ? pl : NULL;
}

/**
 * free pinyin list 
 */
void py_list_free(py_list *pl)
{
	py_list *cur;

	while ((cur = pl) != NULL) {
		pl = pl->next;
		free(cur);
	}
}

/**
 * Test program:
 * gcc -DTEST pinyin.c -lscws
 * ./a.out zhongguoren
 * ./a.out php论坛
 * ./a.out C++教程
 * ./a.out 平安车险
 */
#ifdef TEST
#    include <stdio.h>

static inline int has_8bit_char(const char *s)
{
	unsigned char *p = (unsigned char *) s;
	while (*p) {
		if (*p++ & 0x80) {
			return 1;
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc == 1) {
		printf("Usage: %s <pinyin|chinese> [fuzzy]\n", argv[0]);
	} else {
		py_list *pl, *cur;
		char *buf, *ptr;

		if (has_8bit_char(argv[1])) {
			printf("PY convert for: `%s'\n", argv[1]);
			py_dict_load("etc/py.xdb");
			pl = py_convert(argv[1], strlen(argv[1]));
			py_dict_unload();
		} else {
			printf("PY segment for: `%s'\n", argv[1]);
			pl = py_segment(argv[1], strlen(argv[1]));
		}
		if (argc > 2 && (argv[2][0] == 'f' || argv[2][0] == 'F')) {
			printf("Do fuzzy fix ...\n");
			py_fuzzy_fix(pl);
		}
		ptr = buf = (char *) malloc(strlen(argv[1]) * 2 + 1);
		for (cur = pl; cur != NULL; cur = cur->next) {
			printf("[0x%02x]%s\n", cur->flag, cur->py);
			strcpy(ptr, cur->py);
			ptr += strlen(cur->py);
			if (cur->next != NULL) {
				if (PY_ILLEGAL(cur) && PY_ILLEGAL(cur->next)) {
					*ptr++ = ' ';
				} else if (PY_ZEROSM(cur->next)) {
					*ptr++ = '\'';
				}
			}
		}
		py_list_free(pl);
		printf("CONCAT RESULT: `%s'\n", buf);
		free(buf);
	}
	return 0;
}
#endif


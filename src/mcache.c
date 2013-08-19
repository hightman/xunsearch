/**
 * MyCache implement (C)
 * Hash + List/RBTree
 *
 * MCache 的结构设计, 支持采用共享内存存储数据, 不带锁定功能...
 * 共享内存管理 (+mm.c), 不加 mm.c 则用 malloc/free
 *
 * 数据查找: Hash, 冲撞结点采用 list 或 rbtree 2种选项
 * 数据淘汰: 双向链接, 新增在表头, 命中查找一次则往前移动一位
 * 内存申请失败? 从右向左删满 5条记录?
 * 
 * $Id$
 */

#define	MC	mc_core

#include <stdlib.h>
#include <string.h>
#include "mm.h"

/**
 * LRU move type: move one step to prev
 */
#undef	MOVE_ONE_STEP

/**
 * Cache node struct
 */
typedef struct mc_node mc_node;

struct mc_chain
{
	mc_node *next;
	mc_node *prev;
};

struct mc_rbtree
{
	mc_node *left;
	mc_node *right;
	mc_node *up;
	int color;
};

struct mc_node
{
	void *key;
	void *value;
	int vlen; // value len(if copy)
	struct mc_chain lru;

	union
	{
		struct mc_chain chain;
		struct mc_rbtree rbtree;
	} dash;
};

#define	dash_next		dash.chain.next
#define	dash_prev		dash.chain.prev
#define	dash_left		dash.rbtree.left
#define	dash_right		dash.rbtree.right
#define	dash_up			dash.rbtree.up
#define	dash_color		dash.rbtree.color

/**
 * Cache core struct
 */
typedef struct mc_core
{
	MM *mm; // MM-object
	int size; // Hash size
	int count; // node count..
	int flag; // mode flag
	int mem_used; // used memory!! (bytes, not include shcema MC)
	int mem_max; // allowed max memory
	mc_node **nodes; // nodes. (nodes[0] ... nodes[size-1]), every nodes[0]-> rbtree root OR chain head.
	mc_node *head; // LRU-head
	mc_node *tail; // LRU-tail

	mc_node * (*fetch)(mc_node **root, const char *key);
	void (*insert)(mc_node **root, mc_node * node);
	void (*remove)(mc_node **root, mc_node * node); // NULL => clean
} mc_core;

#include "mcache.h"

/**
 * Error descriptions
 */
static const char *mc_errlist[] = {
	"Successfully", /* MC_OK */
	"Out of memory usage", /* MC_EMEMORY */
	"Invalid argument", /* MC_EINVALID */
	"Not found", /* MC_EFOUND */
	"Un-implemented", /* MC_EUNIMP */
	"Disallowed now", /* MC_EDISALLOW */
	NULL
};

/**
 * Pre-defined hash size
 */
static int hash_sizes[] = {
	5, 7, 11, 23, 47,
	79, 97, 157, 197, 299,
	397, 599, 797, 1297, 1597,
	2499, 3199, 4799, 6397, 9599,
	12799, 19199, 25597, 38399, 51199,
	76799, 102397, 153599, 204797, 306797,
	409597, 614399, 819199, 1288799, 1638397,
	2457599, 3276799, 4915217, 6553577, 9830393
};

/**
 * Allocate memory on MC object
 */
static void *_mc_malloc(MC *mc, int size)
{
	if (size <= 0) return NULL;
	mc->mem_used += size;
	if (mc->mm == NULL) {
		return malloc(size);
	} else {
		return mm_malloc(mc->mm, size);
	}
}

/**
 * Release memory on MC object
 */
static void _mc_free(MC *mc, void *p, int size)
{
	if (size <= 0) {
		return;
	}

	mc->mem_used -= size;
	if (mc->mm == NULL) {
		free(p);
	} else {
		mm_free(mc->mm, p);
	}
}

/**
 * Hasher - calc hash value of given string
 */
static int _get_hasher(const char *s, int size)
{
	unsigned int h = 0xf422f;
	int l = strlen(s);
	while (l--) {
		h += (h << 5);
		h ^= (unsigned char) s[l];
		h &= 0x7fffffff;
	}
	return(h % size);
}

/**
 * Lookup collision item from linear chain list
 */
static mc_node *_chain_fetch(mc_node **root, const char *key)
{
	mc_node *node;
	for (node = *root; node != NULL && strcmp(node->key, key); node = node->dash_next);

	/* move it to first? */
	if (node != NULL && node->dash_prev != NULL) {
		node->dash_prev->dash_next = node->dash_next;
		if (node->dash_next != NULL) {
			node->dash_next->dash_prev = node->dash_prev;
		}

		(*root)->dash_prev = node;
		node->dash_next = *root;
		node->dash_prev = NULL;
		*root = node;
	}
	return node;
}

/**
 * Insert collision item into linear chain list
 */
static void _chain_insert(mc_node **root, mc_node *node)
{
	node->dash_prev = NULL;
	if (*root != NULL) {
		(*root)->dash_prev = node;
	}
	node->dash_next = *root;
	*root = node;
}

/**
 * Remove collision item(s) from linear chain list
 */
static void _chain_remove(mc_node **root, mc_node *node)
{
	if (node->dash_prev != NULL) {
		node->dash_prev->dash_next = node->dash_next;
	} else {
		*root = node->dash_next;
		if ((*root) != NULL) {
			(*root)->dash_prev = NULL;
		}
	}

	if (node->dash_next != NULL) {
		node->dash_next->dash_prev = node->dash_prev;
	}
}

#if 0	/* buggy rbtree begin */

/**
 * Collision scheme of red-black tree
 */
static mc_node *_rb_sibling(mc_node *node)
{
	return(node == node->dash_up->dash_left ? node->dash_up->dash_right : node->dash_up->dash_left);
}

#    define	_rb_is_right(n)		((n)->dash_up->dash_right == n)
#    define	_rb_is_left(n)		((n)->dash_up->dash_left == n)

/**         left rotate
 *
 *    (P)                (Q)
 *   /   \              /   \
 *  1    (Q)    ==>   (P)    3
 *      /   \        /   \
 *     2     3      1     2
 *
 */
static void _rb_left_rotate(mc_node **root, mc_node *p)
{
	mc_node *q = p->dash_right;
	mc_node **sup;

	if (p->dash_up) {
		sup = _rb_is_left(p) ? &(p->dash_up->dash_left) : &(p->dash_up->dash_right);
	} else {
		sup = root;
	}

	p->dash_right = q->dash_left;
	if (p->dash_right) {
		p->dash_right->dash_up = p;
	}
	q->dash_left = p;
	q->dash_up = p->dash_up;
	p->dash_up = q;
	*sup = q;
}

/**           right rotate
 *  
 *       (P)                (Q)
 *      /   \              /   \
 *    (Q)    3    ==>     1    (P)  
 *   /   \                    /   \
 *  1     2                  2     3
 *
 */
static void _rb_right_rotate(mc_node **root, mc_node *p)
{
	mc_node *q = p->dash_left;
	mc_node **sup;

	if (p->dash_up) {
		sup = _rb_is_left(p) ? &(p->dash_up->dash_left) : &(p->dash_up->dash_right);
	} else {
		sup = root;
	}

	p->dash_left = q->dash_right;
	if (p->dash_left) {
		p->dash_left->dash_up = p;
	}
	q->dash_right = p;
	q->dash_up = p->dash_up;
	p->dash_up = q;
	*sup = q;
}

/**
 * Re-balance the tree
 * newly entered node is RED; check balance recursively as required
 */
static void _rb_rebalance(mc_node **root, mc_node *node)
{
	mc_node *up = node->dash_up;

	if (up == NULL || up->dash_color == MC_RB_BLACK) return;
	if (_rb_sibling(up) && _rb_sibling(up)->dash_color == MC_RB_RED) {
		up->dash_color = MC_RB_BLACK;
		_rb_sibling(up)->dash_color = MC_RB_BLACK;
		if (up->dash_up->dash_up) {
			up->dash_up->dash_color = MC_RB_RED;
			_rb_rebalance(root, up->dash_up);
		}
	} else {
		if (_rb_is_left(node) && _rb_is_right(up)) {
			_rb_right_rotate(root, up);
			node = node->dash_right;
		} else if (_rb_is_right(node) && _rb_is_left(up)) {
			_rb_left_rotate(root, up);
			node = node->dash_left;
		}

		node->dash_up->dash_color = MC_RB_BLACK;
		node->dash_up->dash_up->dash_color = MC_RB_RED;

		if (_rb_is_left(node)) { // && _rb_is_left(node->dash_up)
			_rb_right_rotate(root, node->dash_up->dash_up);
		} else {
			_rb_left_rotate(root, node->dash_up->dash_up);
		}
	}
}

/**
 * Lookup collision item from rb-tree
 */
static mc_node *_rbtree_fetch(mc_node **root, const char *key)
{
	int cmp;
	mc_node *node = *root;

	while (node != NULL) {
		cmp = strcmp(node->key, key);
		if (cmp == 0) {
			break;
		}
		node = (cmp > 0 ? node->dash_left : node->dash_right);
	}

	return node;
}

/**
 * Insert collision item into rb-tree
 */
static void _rbtree_insert(mc_node **root, mc_node *node)
{
	if (*root == NULL) {
		node->dash_color = MC_RB_BLACK;
		*root = node;
	} else {
		int cmp;
		mc_node **curr = root;
		mc_node *prev = NULL;

		while (*curr != NULL) {
			prev = *curr;
			cmp = strcmp(prev->key, node->key);
			if (cmp < 0) {
				curr = &prev->dash_right;
			} else if (cmp > 0) {
				curr = &prev->dash_left;
			} else { /* Can this happenn?? -> memory leak!! */
				*curr = node;
				break;
			}
		}

		if (*curr == NULL) {
			node->dash_up = prev;
			*curr = node;
			_rb_rebalance(root, node);
		}
	}
}

/**
 * Swap node content
 */
static void _rb_swap_node_content(mc_node *a, mc_node *b)
{
	void *tmpkey, *tmpval;

	tmpkey = a->key;
	a->key = b->key;
	b->key = tmpkey;

	tmpval = a->value;
	a->value = b->value;
	b->value = tmpval;

	a->vlen ^= b->vlen;
	b->vlen ^= a->vlen;
	a->vlen ^= b->vlen;
}

/**
 * _rbtree_delete to remove nodes with either a left 
 * NULL branch or a right NULL branch
 */
static void _rb_unlink(mc_node **root, mc_node *node)
{
	if (node->dash_left) {
		node->dash_left->dash_up = node->dash_up;
		if (node->dash_up) {
			if (_rb_is_left(node)) {
				node->dash_up->dash_left = node->dash_left;
			} else {
				node->dash_up->dash_right = node->dash_left;
			}
		} else {
			*root = node->dash_left;
		}
	} else {
		if (node->dash_right) {
			node->dash_right->dash_up = node->dash_up;
		}
		if (node->dash_up) {
			if (_rb_is_left(node)) {
				node->dash_up->dash_left = node->dash_right;
			} else {
				node->dash_up->dash_right = node->dash_right;
			}
		} else {
			*root = node->dash_right;
		}
	}
}

/**
 * Perform rebalancing after a deletion
 */
static void _rb_delete_rebalance(mc_node **root, mc_node *n)
{
	if (n->dash_up) {
		mc_node *sibl = _rb_sibling(n);

		if (!sibl) {
			return;
		}
		if (sibl->dash_color == MC_RB_RED) {
			n->dash_up->dash_color = MC_RB_RED;
			sibl->dash_color = MC_RB_BLACK;
			if (_rb_is_left(n)) {
				_rb_left_rotate(root, n->dash_up);
			} else {
				_rb_right_rotate(root, n->dash_up);
			}
			sibl = _rb_sibling(n);
		}

		if (!sibl) {
			return;
		}
		if (n->dash_up->dash_color == MC_RB_BLACK &&
				sibl->dash_color == MC_RB_BLACK &&
				(sibl->dash_left == NULL || sibl->dash_left->dash_color == MC_RB_BLACK) &&
				(sibl->dash_right == NULL || sibl->dash_right->dash_color == MC_RB_BLACK)) {
			sibl->dash_color = MC_RB_RED;
			_rb_delete_rebalance(root, n->dash_up);
		} else {
			if (n->dash_up->dash_color == MC_RB_RED &&
					sibl->dash_color == MC_RB_BLACK &&
					(sibl->dash_left == NULL || sibl->dash_left->dash_color == MC_RB_BLACK) &&
					(sibl->dash_right == NULL || sibl->dash_right->dash_color == MC_RB_BLACK)) {
				sibl->dash_color = MC_RB_RED;
				n->dash_up->dash_color = MC_RB_BLACK;
			} else {
				if (_rb_is_left(n) &&
						sibl->dash_color == MC_RB_BLACK &&
						sibl->dash_left && sibl->dash_left->dash_color == MC_RB_RED &&
						(sibl->dash_right == NULL || sibl->dash_right->dash_color == MC_RB_BLACK)) {
					sibl->dash_color = MC_RB_RED;
					sibl->dash_left->dash_color = MC_RB_BLACK;
					_rb_right_rotate(root, sibl);

					sibl = _rb_sibling(n);
				} else if (_rb_is_right(n) &&
						sibl->dash_color == MC_RB_BLACK &&
						sibl->dash_right && sibl->dash_right->dash_color == MC_RB_RED &&
						(sibl->dash_left == NULL || sibl->dash_left->dash_color == MC_RB_BLACK)) {
					sibl->dash_color = MC_RB_RED;
					sibl->dash_right->dash_color = MC_RB_BLACK;
					_rb_left_rotate(root, sibl);

					sibl = _rb_sibling(n);
				}

				if (!sibl) {
					return;
				}
				sibl->dash_color = n->dash_up->dash_color;
				n->dash_up->dash_color = MC_RB_BLACK;
				if (_rb_is_left(n)) {
					sibl->dash_right->dash_color = MC_RB_BLACK;
					_rb_left_rotate(root, n->dash_up);
				} else {
					sibl->dash_left->dash_color = MC_RB_BLACK;
					_rb_right_rotate(root, n->dash_up);
				}
			}
		}
	}
}

/**
 *  Remove node from rb-tree
 */
static void _rbtree_remove(mc_node **root, mc_node *node)
{
	mc_node *child;

	if (node->dash_right && node->dash_left) {
		mc_node *surrogate = node;
		node = node->dash_right;
		while (node->dash_left) node = node->dash_left;
		_rb_swap_node_content(node, surrogate);
	}
	child = node->dash_right ? node->dash_right : node->dash_left;

	/* if the node was red - no rebalancing required */
	if (node->dash_color == MC_RB_BLACK) {
		if (child) {
			/* single red child - paint it black */
			if (child->dash_color == MC_RB_RED) {
				child->dash_color = MC_RB_BLACK; /* and the balance is restored */
			} else {
				_rb_delete_rebalance(root, child);
			}
		} else {
			_rb_delete_rebalance(root, node);
		}
	}

	_rb_unlink(root, node);
}
#endif	/* buggy rbtree end */

/**
 * Remove a node
 */
static void _mc_remove_node(MC *mc, mc_node *node)
{
	int i = _get_hasher(node->key, mc->size);

	/* remove from dash-bucket */
	mc->remove(&mc->nodes[i], node);

	/* free the memory */
	if (mc->flag & MC_FLAG_COPY_KEY) {
		_mc_free(mc, node->key, strlen(node->key) + 1);
	}
	if (mc->flag & MC_FLAG_COPY_VALUE) {
		_mc_free(mc, node->value, node->vlen);
	}
	_mc_free(mc, node, sizeof(mc_node));
}

/**
 * Remove some nodes according to LRU -list (about 10%)
 */
static void _mc_lru_purge(MC *mc)
{
	int num, i = 0;

	if (mc->count <= 0) {
		return;
	}
	num = mc->count / 10;
	if (num < 1) {
		num = 1;
	}

	while (i < num && mc->tail != NULL) {
		if (mc->tail->lru.prev == NULL) {
			// delete tail & set to NULL for head/tail
			_mc_remove_node(mc, mc->tail);
			mc->tail = mc->head = NULL;
		} else {
			mc->tail = mc->tail->lru.prev;
			_mc_remove_node(mc, mc->tail->lru.next);
			mc->tail->lru.next = NULL;
		}
		i++;
	}
	mc->count -= i;
}

/**
 * Remove spefied node
 */
static void _mc_lru_remove(MC *mc, mc_node *node)
{
	if (node->lru.prev != NULL) {
		node->lru.prev->lru.next = node->lru.next;
	} else {
		mc->head = node->lru.next;
		if (mc->head != NULL) {
			mc->head->lru.prev = NULL;
		}
	}

	if (node->lru.next != NULL) {
		node->lru.next->lru.prev = node->lru.prev;
	} else {
		mc->tail = node->lru.prev;
		if (mc->tail != NULL) {
			mc->tail->lru.next = NULL;
		}
	}
}

/**
 * Optimize position LRU list (move prev one step)
 */
static void _mc_lru_adjust(MC *mc, mc_node *node)
{
	if (node->lru.prev == NULL) {
		return;
	} else {
#ifdef MOVE_ONE_STEP	/* move one step to prev */
		mc_node *next, *prev;

		next = node->lru.next;
		prev = node->lru.prev;

		if (prev->lru.prev == NULL) {
			mc->head = node;
		} else {
			prev->lru.prev->lru.next = node;
		}
		node->lru.prev = prev->lru.prev;
		node->lru.next = prev;
		prev->lru.prev = node;
		prev->lru.next = next;
		if (next == NULL) {
			mc->tail = prev;
		} else {
			next->lru.prev = prev;
		}
#else					/* move to head */	
		node->lru.prev->lru.next = node->lru.next;
		if (node->lru.next != NULL) {
			node->lru.next->lru.prev = node->lru.prev;
		} else {
			mc->tail = node->lru.prev;
		}

		mc->head->lru.prev = node;
		node->lru.next = mc->head;
		node->lru.prev = NULL;
		mc->head = node;
#endif	
	}
}

/**
 * Insert record into LRU-list
 */
static void _mc_lru_insert(MC *mc, mc_node *node)
{
	if (mc->head == NULL) {
		mc->head = mc->tail = node;
	} else {
		mc->head->lru.prev = node;
		node->lru.next = mc->head;
		mc->head = node;
	}
}

/* -----------------------
 * Public memory cache API
 * -----------------------
 */

/**
 * Create a new mcache object, MM can be NULL
 */
MC *mc_create(MM *mm)
{
	MC *mc;
	if (mm == NULL) {
		mc = malloc(sizeof(MC));
	} else {
		mc = mm_malloc(mm, sizeof(MC));
	}
	if (mc == NULL) {
		return NULL;
	}

	memset(mc, 0, sizeof(MC));
	mc->mem_used = sizeof(MC);
	mc->mem_max = 8 * 1024 * 1024;
	mc->mm = mm;
	if (mc_set_hash_size(mc, 0x800) != MC_OK
			|| mc_set_dash_type(mc, MC_DASH_CHAIN) != MC_OK) {
		mc_destroy(mc);
		return NULL;
	}
	return mc;
}

/**
 * Destroy memory cache resrouce
 */
void mc_destroy(MC *mc)
{
	if (mc->size != 0) {
		mc_node *node, *swap;

		// free all nodes by LRU-chain
		for (node = mc->head; node != NULL; node = swap) {
			swap = node->lru.next;
			if (mc->flag & MC_FLAG_COPY_KEY) {
				_mc_free(mc, node->key, strlen(node->key) + 1);
			}
			if (mc->flag & MC_FLAG_COPY_VALUE) {
				_mc_free(mc, node->value, node->vlen);
			}

			_mc_free(mc, node, sizeof(mc_node));
		}
		_mc_free(mc, mc->nodes, sizeof(mc_node *) * mc->size);
	}
	_mc_free(mc, mc, 0);
}

/**
 * Change dash scheme(type: rbtree|chain)
 */
int mc_set_dash_type(MC *mc, int type)
{
	if (mc->head != NULL) {
		return MC_EDISALLOW;
	} else if (type == MC_DASH_CHAIN) {
		mc->fetch = _chain_fetch;
		mc->insert = _chain_insert;
		mc->remove = _chain_remove;
		return MC_OK;
	}
#if 0	/* buggy rbtree */
	else if (type == MC_DASH_RBTREE) {
		mc->fetch = _rbtree_fetch;
		mc->insert = _rbtree_insert;
		mc->remove = _rbtree_remove;
		return MC_OK;
	}
#endif
	else {
		return MC_EINVALID;
	}
}

/**
 * Choose the best hash size, just called at the first!!
 */
int mc_set_hash_size(MC *mc, int size)
{
	mc_node **nodes;
	int cur = 0;
	int max = sizeof(hash_sizes) / sizeof(int) - 1;

	if (mc->head != NULL) {
		return MC_EDISALLOW;
	}

	if (hash_sizes[max] < size) {
		for (cur = hash_sizes[max];
				cur < size;
				cur = ((cur << 1) | 1));
	} else {
		int min;

		for (cur = min = 0; max > min;) {
			cur = (max + min) >> 1;
			if (hash_sizes[cur] < size) {
				min = cur + 1;
			} else {
				max = cur - 1;
			}
		}
		cur = hash_sizes[max];
	}


	nodes = (mc_node **) _mc_malloc(mc, cur * sizeof(mc_node *));
	if (nodes == NULL) {
		return MC_EMEMORY;
	}

	if (mc->nodes != NULL) {
		_mc_free(mc, mc->nodes, sizeof(mc_node *) * mc->size);
	}

	mc->nodes = nodes;
	mc->size = cur;
	return MC_OK;
}

/**
 * Set the copy mode of cache item
 */
int mc_set_copy_flag(MC *mc, int flag)
{
	if (flag != 0 && !(flag & (MC_FLAG_COPY_KEY | MC_FLAG_COPY_VALUE))) {
		return MC_EINVALID;
	}

	if (mc->head != NULL) {
		return MC_EDISALLOW;
	}

	mc->flag &= ~(MC_FLAG_COPY_KEY | MC_FLAG_COPY_VALUE);
	mc->flag |= flag;
	return MC_OK;
}

/**
 * Set the max memory usage
 */
void mc_set_max_memory(MC *mc, int bytes)
{
	mc->mem_max = bytes;
}

/**
 * Fetch cached data
 */
void *mc_get(MC *mc, const char *key)
{
	mc_node *node;
	int i = _get_hasher(key, mc->size);

	node = mc->fetch(&mc->nodes[i], key);

	/* not found => return */
	if (node == NULL) {
		return NULL;
	}

	/* ok => optimze LRU + return */
	_mc_lru_adjust(mc, node);

	return node->value;
}

/**
 * Save data into cache
 */
int mc_put(MC *mc, const char *key, void *value, int vlen)
{
	mc_node *node;
	int i = _get_hasher(key, mc->size);
	int sz = sizeof(mc_node) + 1024;

	/* check memory used & size, keep 1kb */
	if (mc->flag & MC_FLAG_COPY_KEY) sz = sz + strlen(key) + 1;
	if (mc->flag & MC_FLAG_COPY_VALUE) sz += vlen;
	if (sz > (mc->mem_max - mc->mem_used)) {
		_mc_lru_purge(mc);
	}

	/* real not-enough */
	if (sz > (mc->mem_max - mc->mem_used)) {
		return MC_EMEMORY;
	}

	/* fetch node first to check it is exists or not! */
	node = mc->fetch(&mc->nodes[i], key);
	if (node == NULL) {
		/* not found? build it!! */
		node = (mc_node *) _mc_malloc(mc, sizeof(mc_node));
		if (node == NULL) {
			return MC_EMEMORY;
		}

		memset(node, 0, sizeof(mc_node));
		node->vlen = vlen;
		if (!(mc->flag & MC_FLAG_COPY_KEY)) {
			node->key = (char *) key;
		} else {
			int j = strlen(key) + 1;

			node->key = (char *) _mc_malloc(mc, j);
			if (node->key == NULL) {
				_mc_free(mc, node, sizeof(mc_node));
				return MC_EMEMORY;
			}
			memcpy(node->key, key, j);
		}
		if (!(mc->flag & MC_FLAG_COPY_VALUE) || vlen <= 0) {
			node->value = value;
		} else {
			node->value = _mc_malloc(mc, vlen);
			if (node->value == NULL) {
				if (mc->flag & MC_FLAG_COPY_KEY) {
					_mc_free(mc, node->key, strlen(node->key) + 1);
				}

				_mc_free(mc, node, sizeof(mc_node));
				return MC_EMEMORY;
			}
			memcpy(node->value, value, vlen);
		}

		/* add to dash-bucket  (failed? memory leak?) */
		mc->insert(&mc->nodes[i], node);
		mc->count++;

		/* add to LRU-chain */
		_mc_lru_insert(mc, node);
	} else {
		/* found, just update... */
		if (!(mc->flag & MC_FLAG_COPY_VALUE)) {
			node->value = value;
		} else {
			if (node->vlen == vlen && vlen != 0) {
				memcpy(node->value, value, vlen);
			} else {
				_mc_free(mc, node->value, node->vlen);
				if (vlen == 0) {
					node->value = value;
				} else {
					node->value = _mc_malloc(mc, vlen);
					if (node->value == NULL) {
						node->vlen = 0;
						return MC_EMEMORY;
					}
					memcpy(node->value, value, vlen);
				}
			}
		}

		/* adjust Pos on LRU-chain */
		_mc_lru_adjust(mc, node);
	}

	/* save other data for node */
	node->vlen = vlen;
	return MC_OK;
}

/**
 * Delete cached data by key
 */
int mc_del(MC *mc, const char *key)
{
	mc_node *node;
	int i = _get_hasher(key, mc->size);

	/* check found or not */
	node = mc->fetch(&mc->nodes[i], key);
	if (node == NULL) {
		return MC_EFOUND;
	}

	/* remove from dash-bucket */
	mc->remove(&mc->nodes[i], node);
	mc->count--;

	/* remove from LRU-chain */
	_mc_lru_remove(mc, node);

	/* free the memory */
	if (mc->flag & MC_FLAG_COPY_KEY) {
		_mc_free(mc, node->key, strlen(node->key) + 1);
	}
	if (mc->flag & MC_FLAG_COPY_VALUE) {
		_mc_free(mc, node->value, node->vlen);
	}
	_mc_free(mc, node, sizeof(mc_node));

	return MC_OK;
}

/**
 * Get error description
 */
const char *mc_strerror(int err)
{
	if (err < 0 || err > (sizeof(mc_errlist) / sizeof(char *))) {
		return NULL;
	}
	return mc_errlist[err];
}

/**
 * Test program, compiled with -DTEST
 */
#ifdef MC_TEST
#    include <stdio.h>
#    include <unistd.h>
#    define SAVE(k,v)	mc_put(mc, k, v, sizeof(v))
#    define GETC(k)		printf("[C] s(%s)=%s\n", k, (char *)mc_get(mc, k))
#    define GETP(k)		printf("[P] s(%s)=%s\n", k, (char *)mc_get(mc, k))

void show_mc(MC *mc)
{
	printf("mc=%p, hash_size=%d, count=%d, mem_used=%d, mem_max=%d, type=%s\n",
			mc, mc->size, mc->count, mc->mem_used, mc->mem_max, mc->fetch == _chain_fetch ? "CHAIN" : "RBTREE");
}

void rbtree_print(mc_node *node, int level)
{
	int i;
	if (node->dash_right) {
		rbtree_print(node->dash_right, level + 1);
	}
	for (i = 0; i < level; i++) {
		printf("  . ");
	}
	printf("(%d) [%s]\n", node->dash_color, (char *) node->key);
	if (node->dash_left) {
		rbtree_print(node->dash_left, level + 1);
	}
}

void nodes_mc(MC *mc)
{
	int i;
	mc_node *node;

	// by chain
	for (i = 0; i < mc->size; i++) {
		if (mc->fetch == _chain_fetch) {
			printf("NODES[%d]: ", i);
			for (node = mc->nodes[i]; node != NULL; node = node->dash_next) {
				printf("[%s]->", (char *) node->key);
			}
			printf("NULL\n");
		} else {
			printf("NODES[%d]: \n", i);
			if (mc->nodes[i] != NULL) {
				rbtree_print(mc->nodes[i], 0);
			}
		}
	}

	printf("LRU1: HEAD");
	for (node = mc->head; node != NULL; node = node->lru.next) {
		printf("->[%s]", (char *) node->key);
	}
	printf("->TAIL\n");
	printf("LRU2: TAIL");
	for (node = mc->tail; node != NULL; node = node->lru.prev) {
		printf("->[%s]", (char *) node->key);
	}
	printf("->HEAD\n");
}

int main(int argc, char *argv[])
{
	MC *mc;
	char *s;
	MM *mm;

	mm = mm_create(2 << 20);
	printf("[P] mm=%p\n", mm);
	mc = mc_create(mm);
	mc_set_hash_size(mc, 5);
	mc->size = 3;
	mc_set_max_memory(mc, 1 << 20);
	mc_set_copy_flag(mc, MC_FLAG_COPY);
	mc_set_dash_type(mc, (argc > 1 && !strcmp(argv[1], "chain")) ? MC_DASH_CHAIN : MC_DASH_RBTREE);
	show_mc(mc);
	nodes_mc(mc);

	if (fork() == 0) {
		printf("[C] wait the parent pressure test\n");
		sleep(1);

		printf("[C] try to get lock\n");
		mm_lock1(mm);
		printf("[C] child running. save gg0..\n");
		SAVE("gg0", "hightman0");
		printf("[C] chinld first end, start to sleep(2)!\n");
		mm_unlock1(mm);
		sleep(2);

		mm_lock1(mm);
		printf("[C] get the lock from parent now, saving data...\n");
		SAVE("mm1", "twomice1");
		//nodes_mc(mc);
		SAVE("mm2", "twomice2");
		SAVE("mm3", "twomice3");
		SAVE("mm4", "twomice4");
		SAVE("mm5", "twomice5");
		SAVE("mm6", "twomice6");
		SAVE("mm7", "twomice7");
		printf("[C] child: save ok, mm1~mm7, sleep 2 sec, then unlock\n");
		sleep(2);
		printf("[C] wake up to unlock\n");
		mm_unlock1(mm);

		mm_lock1(mm);
		printf("[C] child end show\n");
		GETC("mm7");
		GETC("mm3");
		show_mc(mc);
		nodes_mc(mc);
		mm_unlock1(mm);
		printf("[C] child finished!\n");
	} else {
		int i, max = 99999;
		char key[128], value[128];
		int j = time(NULL) % 999;

		mm_lock1(mm);
		printf("[P] testing put gg?? for %d times\n", max);
		for (i = 0; i < max; i++) {
			sprintf(key, "gg%d", i);
			sprintf(value, "hightman%d", i);
			SAVE(key, value);
		}
		printf("[P] testing get gg?? for %d times\n", max);
		for (i = 0; i < max; i++) {
			sprintf(key, "gg%d", i);
			s = mc_get(mc, key);
			if ((i % j) == 0) {
				printf("[P] ... mc_get(%s) = %s\n", key, s);
			}
		}
		printf("[P] testing del gg?? for %d times\n", max);
		for (i = 0; i < max; i++) {
			if ((i % j) != 0) {
				sprintf(key, "gg%d", i);
				mc_del(mc, key);
			}
		}
		printf("[P] testing end\n");
		mm_unlock1(mm);

		printf("[P] begin to sleep(1) & wait child\n");
		sleep(1);
		mm_lock1(mm);
		printf("[P] it is my turn(show mc)\n");
		printf("--------------------------\n");
		nodes_mc(mc);
		printf("--------------------------\n");
		GETP("gg0");
		printf("[P] go to sleep(2) too\n");
		sleep(2);
		printf("[P] wake up tounlock, then sleep(1)!\n");
		mm_unlock1(mm);

		sleep(1);
		printf("[P] waiting next lock\n");
		mm_lock1(mm);
		printf("[P] re show some keys\n");
		GETP("mm3");
		nodes_mc(mc);
		GETP("mm1");
		nodes_mc(mc);
		GETP("mm1");
		SAVE("mm7", "hightman7");
		nodes_mc(mc);
		mm_unlock1(mm);
		printf("[P] finished!\n");

		mc_destroy(mc);
		mm_destroy(mm);
	}
	return 0;
}
#endif

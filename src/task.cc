/**
 * Run task by worker thread
 * $Id$
 */
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <string>
#include <set>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <limits.h>
#include <xapian.h>
#include <pthread.h>

#include "log.h"
#include "conn.h"
#include "global.h"
#include "task.h"
#include "pinyin.h"
#include "import.h"

/**
 * Reset debug log macro to contain tid
 */
#ifdef DEBUG
#    undef	log_debug
#    undef	log_debug_conn
#    define	log_debug(fmt,...)		_log_printf(LOG_DEBUG, "[%s:%d] [thr:%p] " fmt, \
		__FILE__, __LINE__, pthread_self(), ##__VA_ARGS__)
#    define	log_debug_conn(fmt,...)	log_debug("[sock:%d] " fmt, CONN_FD(), ##__VA_ARGS__)
#endif	/* DEBUG */

/**
 * common scws
 */
#include <scws/scws.h>
static scws_t _scws;

/**
 * Extern global variables
 */
extern Xapian::Stem stemmer;
extern Xapian::SimpleStopper *stopper;
using std::string;

/**
 * Local static variables
 */
#define	QUERY_OP_NUM	6
static int query_ops[] = {
	Xapian::Query::OP_AND,
	Xapian::Query::OP_OR,
	Xapian::Query::OP_AND_NOT,
	Xapian::Query::OP_XOR,
	Xapian::Query::OP_AND_MAYBE,
	Xapian::Query::OP_FILTER,
	0
};

/**
 * Local cached query parser object
 */
struct cache_qp
{
	bool in_use;
	Xapian::QueryParser *qp;
	struct cache_qp *next;
};

static struct cache_qp *qp_base = NULL;
static pthread_mutex_t qp_mutex;

/**
 * Data structure for zcmd_exec
 */
enum object_type
{
	OTYPE_DB,
	OTYPE_RANGER,
	OTYPE_KEYMAKER
};

struct object_chain
{
	enum object_type type;
	char *key;
	void *val;
	struct object_chain *next;
};

struct search_zarg
{
	Xapian::Database *db;
	Xapian::Enquire *eq;
	Xapian::Query *qq;
	Xapian::QueryParser *qp;

	unsigned int parse_flag;
	unsigned int db_total;
	unsigned char cuts[XS_DATA_VNO + 1]; // 0x80(numeric)|(cut_len/10)
	unsigned char facets[MAX_SEARCH_FACETS]; // facets earch record

	struct object_chain *objs;
};

struct result_doc
{
	unsigned int docid; // Xapian::docid
	unsigned int rank;
	unsigned int ccount;
	int percent;
	float weight;
};

#define	DELETE_PTR(p)		do { if (p != NULL) { delete p; p = NULL; } } while(0)
#define	DELETE_PTT(p,t)		do { if (p != NULL) { delete (t) p; p = NULL; } } while(0)
#define	GET_SCALE(b)		(double)(b[0]<<8|b[1])/100
#define	GET_QUERY_OP(a)		(Xapian::Query::op)query_ops[a % QUERY_OP_NUM]

#define	CACHE_NONE			0
#define	CACHE_USE			1	// cache was used
#define	CACHE_FOUND			2	// cache found
#define	CACHE_VALID			4	// cache valid
#define	CACHE_NEED			8	// is cache need (for big object)

#ifdef HAVE_MEMORY_CACHE
#    include "global.h"
#    include "mcache.h"
#    include "md5.h"

extern MC *mc;

struct cache_count
{
	unsigned int total; // document total on caching
	unsigned int count; // matched count
	unsigned int lastid; // last docid
};

#    define	C_LOCK_CACHE()		G_LOCK_CACHE(); conn->flag |= CONN_FLAG_CACHE_LOCKED
#    define	C_UNLOCK_CACHE()	G_UNLOCK_CACHE(); conn->flag ^= CONN_FLAG_CACHE_LOCKED
#endif	/* HAVE_MEMORY_CACHE */

struct search_result
{
#ifdef HAVE_MEMORY_CACHE
	unsigned int total; // document total on caching
	unsigned int count; // matched count
	unsigned int lastid; // last docid
	struct result_doc doc[MAX_SEARCH_RESULT];
#endif
	unsigned int facets_len; // data length of facets result
};
/**
 * Geodist keymaker
 */
#include <math.h>
#define	DEG2RAD(x)	((x) * M_PI / 180)

class GeodistKeyMaker : public Xapian::KeyMaker {
	Xapian::valueno lat_vno, lon_vno;
	double lat_value, lon_value;

public:

	GeodistKeyMaker() {
	}
	virtual string operator()(const Xapian::Document & doc) const;

	void set_latitude(Xapian::valueno vno, double value) {
		lat_vno = vno;
		lat_value = value;
	}

	void set_longitude(Xapian::valueno vno, double value) {
		lon_vno = vno;
		lon_value = value;
	}
};

string GeodistKeyMaker::operator()(const Xapian::Document & doc) const {
	string result;
	double lat_value2 = Xapian::sortable_unserialise(doc.get_value(lat_vno));
	double lon_value2 = Xapian::sortable_unserialise(doc.get_value(lon_vno));
	
	/* Haversine algorithm */
#ifdef	USE_HAVERSINE
	double hsinX = sin(DEG2RAD(lon_value - lon_value2) * 0.5);
	double hsinY = sin(DEG2RAD(lat_value - lat_value2) * 0.5);
	double h = hsinY * hsinY + (cos(DEG2RAD(lat_value)) * cos(DEG2RAD(lat_value2)) * hsinX * hsinX);
	double dist = 2 * atan2(sqrt(h), sqrt(1 - h)) * 6367000.0;
#else
	/* Referer: http://www.cocoachina.com/ios/20141118/10238.html */
	double dx = lon_value - lon_value2; // 经度差值
    double dy = lat_value - lat_value2; // 纬度差值
	double b = (lat_value + lat_value2) * 0.5; // 平均纬度
	double lx = 6367000.0 * DEG2RAD(dx) * cos(DEG2RAD(b)); // 东西距离
	double ly = 6367000.0 * DEG2RAD(dy); // 南北距离
	double dist = sqrt(lx * lx + ly * ly);  // 用平面的矩形对角距离公式计算总距离   
#endif
	result = Xapian::sortable_serialise(dist);
	return result;
}

/**
 * TermCountMatchSpy
 * for multi-value facets
 */
class TermCountMatchSpy : public Xapian::ValueCountMatchSpy {

public:

	TermCountMatchSpy(Xapian::valueno slot_) : Xapian::ValueCountMatchSpy(slot_) {
	}
	void operator()(const Xapian::Document &doc, Xapian::weight wt);
};

void TermCountMatchSpy::operator()(const Xapian::Document &doc, Xapian::weight wt) {
	++(internal->total);
	string val(doc.get_value(internal->slot));
	if (!val.empty()) {
		++(internal->values[val]);
	} else {
		Xapian::TermIterator ti = doc.termlist_begin();
		while (ti != doc.termlist_end()) {
			string tt = *ti++;
			if (tt[0] != PREFIX_CHAR_ZZZ && prefix_to_vno((char *) tt.data()) == internal->slot) {
				tt = tt.substr(tt[1] >= 'A' && tt[1] <= 'Z' ? 2 : 1);
				if (!tt.empty()) {
					++(internal->values[tt]);
				}
			}
		}
	}
}

/**
 * @param conn
 * @return CMD_RES_CONT
 */
int task_add_search_log(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		log_warning_conn("search log too long to add (LOG:%.*s)", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd));
		return CONN_RES_ERR(TOOLONG);
	} else {
		FILE *fp;
		char fpath[256];
		sprintf(fpath, "%s/" SEARCH_LOG_FILE, conn->user->home);
		if ((fp = fopen(fpath, "a")) == NULL) {
			log_error_conn("failed to open search log (PATH:%s, ERROR:%s)", fpath, strerror(errno));
		} else {
			if (XS_CMD_BLEN1(cmd) != 4) {
				fprintf(fp, "%.*s\n", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd));
			} else {
				fprintf(fp, "%.*s\t%d\n", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd), (*(int *) XS_CMD_BUF1(cmd)));
			}
			fclose(fp);
		}
		return CONN_RES_OK(LOGGED);
	}
}

/**
 * Get a queryparser object from cached chain
 */
static Xapian::QueryParser *get_queryparser()
{
	struct cache_qp *head;

	pthread_mutex_lock(&qp_mutex);
	for (head = qp_base; head != NULL; head = head->next) {
		if (head->in_use == false) {
			log_debug("reuse qp (ADDR:%p)", head);
			break;
		}
	}
	if (head == NULL) /* alloc new one */ {
		debug_malloc(head, sizeof(struct cache_qp), struct cache_qp);
		if (head == NULL) {
			pthread_mutex_unlock(&qp_mutex);
			throw new Xapian::InternalError("not enough memory to create cache_qp");
		}
		log_debug("create qp (ADDR:%p)", head);
		head->qp = new Xapian::QueryParser();
		log_debug("new (Xapian::QueryParser *) %p", head->qp);
		head->next = qp_base;
		qp_base = head;
	}
	head->in_use = true;
	pthread_mutex_unlock(&qp_mutex);

	head->qp->clear();
	return head->qp;
}

/**
 * Free q queryparser object to chain
 */
static void free_queryparser(Xapian::QueryParser *qp)
{
	struct cache_qp *head;

	pthread_mutex_lock(&qp_mutex);
	for (head = qp_base; head != NULL; head = head->next) {
		if (head->qp == qp) {
			break;
		}
	}
	if (head != NULL) {
		log_debug("free qp (ADDR:%p)", head);
		head->in_use = false;
	} else {
		DELETE_PTR(qp);
	}
	pthread_mutex_unlock(&qp_mutex);
}

/**
 * Cut longer string or convert serialise string into numeric
 * @param s string
 * @param v int (char)
 * @param m MsetIterator
 * @param z search_zarg
 */
static inline void cut_matched_string(string &s, int v, unsigned int id, struct search_zarg *z)
{
	int cut = (int) z->cuts[v];
	if (cut & CMD_VALUE_FLAG_NUMERIC) {
		// convert to numeric string
		int i;
		char buf[64];
		buf[63] = '\0';
		snprintf(buf, sizeof(buf) - 1, "%f", Xapian::sortable_unserialise(s));
		i = strlen(buf) - 1;
		while (i >= 0) {
			if (buf[i] == '0') {
				buf[i--] = '\0';
				continue;
			}
			if (buf[i] == '.') {
				buf[i] = '\0';
			}
			break;
		}
		s = string(buf);
	} else {
		cut = (cut & (CMD_VALUE_FLAG_NUMERIC - 1)) * 10;
		if (cut != 0 && s.size() > cut) {
			int i, j;
			const char *ptr;
			string tt = string("");
			Xapian::TermIterator tb = z->eq->get_matching_terms_begin(id);
			Xapian::TermIterator te = z->eq->get_matching_terms_end(id);

			// get longest matched term
			while (tb != te) {
				string tm = *tb;
				ptr = tm.data();
				j = prefix_to_vno((char *) ptr);
				if (j == v || j == XS_DATA_VNO) {
					for (i = 0; ptr[i] >= 'A' && ptr[i] <= 'Z'; i++);
					if (i > 0) {
						tm = tm.substr(i);
					}
					if (tm.size() > tt.size()) {
						tt = tm;
					}
				}
				tb++;
			}

			// get start pos
			i = 0;
			if (tt.size() > 0) {
				ptr = strcasestr(s.data(), tt.data());
				if (ptr != NULL) {
					i = ptr - s.data() - (cut >> 2);
					if (i < 0) i = 0;
				}
			}

			ptr = s.data() + i;
			if (i == 0) {
				tt = string("");
			} else {
				tt = string("...");
				while ((*ptr & 0xc0) == 0x80) {
					ptr++;
					i++;
				}
			}
			if ((i + cut) >= s.size()) {
				tt += s.substr(i);
			} else {
				while (cut > 0 && (ptr[cut] & 0xc0) == 0x80) cut--;
				tt += s.substr(i, cut);
				tt += string("...");
			}
			s = tt;
		}
	}
}

/**
 * Get query object from CMD
 */
#define	FETCH_CMD_QUERY(q)		do { \
	if (XS_CMD_BLEN(cmd) == 0) { \
		q = Xapian::Query(*zarg->qq); \
	} else { \
		string qstr = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd)); \
		int flag = zarg->parse_flag > 0 ? zarg->parse_flag : Xapian::QueryParser::FLAG_DEFAULT;	\
		zarg->qp->set_default_op(GET_QUERY_OP(cmd->arg2)); \
		q = zarg->qp->parse_query(qstr, flag); \
		log_info_conn("search/count query (QUERY:%s, FLAG:0x%04x, DEF_OP:%d)", qstr.data(), flag, cmd->arg2); \
	} \
} while(0)

/**
 * Send a document to client
 * @param conn (XS_CONN *)
 * @param rd (struct result_doc *)
 */
static int send_result_doc(XS_CONN *conn, struct result_doc *rd, struct search_result *cr)
{
	int rc = CMD_RES_CONT;

	// send the doc header
	log_debug_conn("search result doc (ID:%u, PERCENT:%d%%)", rd->docid, rd->percent);
	try {
		int vno;
		string data;
		struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
		Xapian::Document d = zarg->db->get_document(rd->docid);
		Xapian::ValueIterator v = d.values_begin();

		// send doc header
		rc = conn_respond(conn, CMD_SEARCH_RESULT_DOC, 0, (char *) rd, sizeof(struct result_doc));
		if (rc != CMD_RES_CONT) {
			return rc;
		}

		// send other fields (value)
		while (v != d.values_end() && rc == CMD_RES_CONT) {
			vno = v.get_valueno();
			data = *v++;

			cut_matched_string(data, vno, rd->docid, (struct search_zarg *) conn->zarg);
			rc = conn_respond(conn, CMD_SEARCH_RESULT_FIELD, vno, data.data(), data.size());
		}

		// send data (body)
		data = d.get_data();
		vno = XS_DATA_VNO;

		cut_matched_string(data, vno, rd->docid, (struct search_zarg *) conn->zarg);
		rc = conn_respond(conn, CMD_SEARCH_RESULT_FIELD, vno, data.data(), data.size());

		// send matched terms
		if (conn->flag & CONN_FLAG_MATCHED_TERM) {
			int i;
			Xapian::TermIterator tb = zarg->eq->get_matching_terms_begin(rd->docid);
			Xapian::TermIterator te = zarg->eq->get_matching_terms_end(rd->docid);
			// get matched terms
			data.resize(0);
			while (tb != te) {
				string tt = *tb;
				Xapian::TermIterator ub = zarg->qp->unstem_begin(tt);
				Xapian::TermIterator ue = zarg->qp->unstem_end(tt);
				if (ub == ue) {
					for (i = 0; tt[i] >= 'A' && tt[i] <= 'Z'; i++);
					if (data.size() == 0) {
						data = tt.substr(i);
					} else {
						data += " " + tt.substr(i);
					}
				} else {
					while (ub != ue) {
						tt = *ub++;
						if (data.size() == 0) {
							data = tt;
						} else {
							data += " " + tt;
						}
					}
				}
				tb++;
			}
			rc = conn_respond(conn, CMD_SEARCH_RESULT_MATCHED, 0, data.data(), data.size());
		}
	} catch (const Xapian::Error &e) {
		// ignore the error simply
		log_error_conn("xapian exception on sending doc (ERROR:%s)", e.get_msg().data());
		if (cr != NULL) {
#ifdef HAVE_MEMORY_CACHE
			cr->count = cr->lastid = 0;
#endif
		}
		rc = CMD_RES_CONT;
	}
	return rc;
}

#ifdef XS_HACK_UUID

/**
 * Check is the query cotain hack uuid
 * @param q
 * @return 
 */
static inline bool is_hack_query(Xapian::Query &q)
{
	Xapian::TermIterator tb = q.get_terms_begin();
	Xapian::TermIterator te = q.get_terms_end();

	while (tb != te) {
		if (*tb == XS_HACK_UUID) {
			return true;
		}
		tb++;
	}
	return false;
}
#endif

/**
 * Add pointer into zarg
 * @param zarg
 */
static void zarg_add_object(struct search_zarg *zarg, enum object_type type, const char *key, void *val)
{
	struct object_chain *oc;

	debug_malloc(oc, sizeof(struct object_chain), struct object_chain);
	if (oc == NULL) {
		return;
	}

	oc->type = type;
	oc->key = key == NULL ? NULL : strdup(key);
	oc->val = val;
	oc->next = zarg->objs;
	zarg->objs = oc;
}

/**
 * Get pointer from zarg
 * @param zarg
 */
static void *zarg_get_object(struct search_zarg *zarg, enum object_type type, const char *key)
{
	struct object_chain *oc = zarg->objs;
	while (oc != NULL) {
		if (oc->type == type
				&& ((oc->key == NULL && key == NULL) || !strcmp(oc->key, key))) {
			return oc->val;
		}
		oc = oc->next;
	}
	return NULL;
}

/**
 * Free zarg pointers
 * @param zarg
 */
static inline void zarg_cleanup(struct search_zarg *zarg)
{
	struct object_chain *oc;

	log_debug("cleanup search zarg");
	while ((oc = zarg->objs) != NULL) {
		zarg->objs = oc->next;
		if (oc->type == OTYPE_DB) {
			log_debug("delete (Xapian::Database *) %p (KEY:%s)", oc->val,
					oc->key == NULL ? "-" : oc->key);
			DELETE_PTT(oc->val, Xapian::Database *);
		} else if (oc->type == OTYPE_RANGER) {
			log_debug("delete (Xapian::ValueRangeProcessor *) %p", oc->val);
			DELETE_PTT(oc->val, Xapian::ValueRangeProcessor *);
		} else if (oc->type == OTYPE_KEYMAKER) {
			log_debug("delete (Xapian::KeyMaker *) %p", oc->val);
			DELETE_PTT(oc->val, Xapian::KeyMaker *);
		}
		if (oc->key != NULL) {
			free(oc->key);
		}
		debug_free(oc);
	}
	DELETE_PTR(zarg->eq);
	free_queryparser(zarg->qp);
	DELETE_PTR(zarg->qq);
	DELETE_PTR(zarg->db);
}

/**
 * task zcmd command handler
 * @param conn
 * @return CMD_RES_CONT/CMD_RES_NEXT/CMD_RES_xxx
 */
static int zcmd_task_default(XS_CONN *conn)
{
	int rc = CMD_RES_CONT;
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;

	switch (cmd->cmd) {
		case CMD_USE:
			rc = CONN_RES_ERR(WRONGPLACE);
			break;
		case CMD_SEARCH_FINISH:
			// put conn backto main loop
			if ((rc = CONN_RES_OK(FINISHED)) == CMD_RES_CONT)
				rc = CMD_RES_PAUSE;
			break;
		case CMD_SEARCH_DB_TOTAL:
			// get total doccount of DB
			if (zarg->db == NULL) {
				rc = CONN_RES_ERR(NODB);
			} else {
				rc = CONN_RES_OK3(DB_TOTAL, (char *) &zarg->db_total, sizeof(zarg->db_total));
			}
			break;
		case CMD_SEARCH_ADD_LOG:
			rc = task_add_search_log(conn);
			break;
		case CMD_SEARCH_GET_DB:
			if (zarg->db == NULL) {
				rc = CONN_RES_ERR(NODB);
			} else {
				const string &desc = zarg->db->get_description();
				rc = CONN_RES_OK3(DB_INFO, desc.data(), desc.size());
			}
			break;
			// ===== silent commands ===== //
			// NOTE: the following commands without any respond
		case CMD_SEARCH_SET_SORT:
			if (zarg->eq != NULL) {
				int type = cmd->arg1 & CMD_SORT_TYPE_MASK;
				bool reverse = (cmd->arg1 & CMD_SORT_FLAG_ASCENDING) ? false : true;
				bool rv_first = (cmd->arg1 & CMD_SORT_FLAG_RELEVANCE) ? true : false;

				conn->flag |= CONN_FLAG_CH_SORT;
				if (type == CMD_SORT_TYPE_DOCID) {
					zarg->eq->set_docid_order(reverse ? Xapian::Enquire::DESCENDING : Xapian::Enquire::ASCENDING);
				} else if (type == CMD_SORT_TYPE_VALUE) {
					if (rv_first == true) {
						zarg->eq->set_sort_by_relevance_then_value(cmd->arg2, reverse);
					} else {
						zarg->eq->set_sort_by_value_then_relevance(cmd->arg2, reverse);
					}
				} else if (type == CMD_SORT_TYPE_RELEVANCE) {
					zarg->eq->set_sort_by_relevance();
					conn->flag &= ~CONN_FLAG_CH_SORT;
				} else if (type == CMD_SORT_TYPE_MULTI) {
					int i;
					unsigned char *buf = (unsigned char *) XS_CMD_BUF(cmd);
					Xapian::MultiValueKeyMaker *sorter = new Xapian::MultiValueKeyMaker();

					for (i = 0; i < (XS_CMD_BLEN(cmd) - 1); i += 2) {
						sorter->add_value(buf[i], buf[i + 1] == 0 ? true : false);
					}
					zarg_add_object(zarg, OTYPE_KEYMAKER, NULL, sorter);
					log_debug_conn("new (Xapian::MultiValueKeyMaker *) %p", sorter);
					if (rv_first == true) {
						zarg->eq->set_sort_by_relevance_then_key(sorter, reverse);
					} else {
						zarg->eq->set_sort_by_key_then_relevance(sorter, reverse);
					}
				} else if (type == CMD_SORT_TYPE_GEODIST) {
					unsigned char *buf = (unsigned char *) XS_CMD_BUF(cmd);
					int i = buf[1] + 2;
					Xapian::valueno lon_vno = buf[0];
					Xapian::valueno lat_vno = buf[i];
					string lon_value = string((char *) buf + 2, (int) buf[1]);
					string lat_value = string((char *) buf + i + 2, (int) buf[i + 1]);
					
					GeodistKeyMaker *sorter = new GeodistKeyMaker();
					sorter->set_longitude(lon_vno, strtod(lon_value.data(), NULL));					
					sorter->set_latitude(lat_vno, strtod(lat_value.data(), NULL));
					zarg_add_object(zarg, OTYPE_KEYMAKER, NULL, sorter);
					log_debug_conn("new (GeodistKeyMaker *) %p", sorter);
					if (rv_first == true) {
						zarg->eq->set_sort_by_relevance_then_key(sorter, reverse);
					} else {
						zarg->eq->set_sort_by_key_then_relevance(sorter, reverse);
					}
				}
			}
			break;
		case CMD_SEARCH_SET_CUT:
			zarg->cuts[cmd->arg2] &= CMD_VALUE_FLAG_NUMERIC;
			zarg->cuts[cmd->arg2] |= (cmd->arg1 & (CMD_VALUE_FLAG_NUMERIC - 1));
			break;
		case CMD_SEARCH_SET_NUMERIC:
			zarg->cuts[cmd->arg2] |= CMD_VALUE_FLAG_NUMERIC;
			break;
		case CMD_SEARCH_SET_COLLAPSE:
			if (zarg->eq != NULL) {
				int vno = cmd->arg2 == XS_DATA_VNO ? Xapian::BAD_VALUENO : cmd->arg2;
				int max = cmd->arg1 == 0 ? 1 : cmd->arg1;
				zarg->eq->set_collapse_key(vno, max);
				if (cmd->arg2 == XS_DATA_VNO) {
					conn->flag &= ~CONN_FLAG_CH_COLLAPSE;
				} else {
					conn->flag |= CONN_FLAG_CH_COLLAPSE;
				}
			}
			break;
		case CMD_SEARCH_SET_FACETS:
			// exact check
			if (cmd->arg1 == 1) {
				conn->flag |= CONN_FLAG_EXACT_FACETS;
			} else {
				conn->flag &= ~CONN_FLAG_EXACT_FACETS;
			}
			// facets list
			if (XS_CMD_BLEN(cmd) > 0) {
				int i, j;
				unsigned char *buf = (unsigned char *) XS_CMD_BUF(cmd);
				for (i = j = 0; i <= sizeof(zarg->facets) && j < XS_CMD_BLEN(cmd); j++) {
					if (buf[j] < XS_DATA_VNO) {
						zarg->facets[i++] = buf[j] + 1;
					}
				}
			}
			break;
		case CMD_SEARCH_SET_CUTOFF:
			zarg->eq->set_cutoff(cmd->arg1 > 100 ? 100 : cmd->arg1, (double) cmd->arg2 / 10.0);
			break;
		case CMD_SEARCH_SET_MISC:
			if (cmd->arg1 == CMD_SEARCH_MISC_SYN_SCALE) {
				zarg->qp->set_syn_scale((double) cmd->arg2 / 100.0);
			} else if (cmd->arg1 == CMD_SEARCH_MISC_MATCHED_TERM) {
				if (cmd->arg2 == 1) {
					conn->flag |= CONN_FLAG_MATCHED_TERM;
				} else {
					conn->flag &= ~CONN_FLAG_MATCHED_TERM;
				}
			}
			break;
		case CMD_QUERY_INIT:
			if (!zarg->qq->empty()) {
				delete zarg->qq;
				zarg->qq = new Xapian::Query();
			}
			if (cmd->arg1 == 1) {
				zarg->qp->clear();
				zarg->qp->set_database(*zarg->db);
				zarg->parse_flag = 0;
				memset(&zarg->cuts, 0, sizeof(zarg->cuts));
			}
			break;
		case CMD_QUERY_PREFIX:
			if (cmd->arg2 != XS_DATA_VNO) {
				char prefix[3];
				string field = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));

				vno_to_prefix(cmd->arg2, prefix);
				if (cmd->arg1 == CMD_PREFIX_BOOLEAN) {
					zarg->qp->add_boolean_prefix(field, prefix);
				} else {
					zarg->qp->add_prefix(field, prefix);
				}
			}
			break;
		case CMD_QUERY_PARSEFLAG:
			zarg->parse_flag = XS_CMD_ARG(cmd);
			break;
		case CMD_QUERY_RANGEPROC:
		{
			Xapian::ValueRangeProcessor *vrp;

			if (cmd->arg1 == CMD_RANGE_PROC_DATE) {
				vrp = new Xapian::DateValueRangeProcessor(cmd->arg2);
			} else if (cmd->arg1 == CMD_RANGE_PROC_NUMBER) {
				vrp = new Xapian::NumberValueRangeProcessor(cmd->arg2);
			} else {
				vrp = new Xapian::StringValueRangeProcessor(cmd->arg2);
			}

			zarg_add_object(zarg, OTYPE_RANGER, NULL, vrp);
			zarg->qp->add_valuerangeprocessor(vrp);
			log_debug_conn("new (Xapian::ValueRangeProcessor *) %p", vrp);
		}
			break;
		case CMD_SEARCH_SCWS_SET:
			// support multi only
			if (cmd->arg1 == CMD_SCWS_SET_MULTI) {
				scws_t scws = (scws_t) zarg->qp->get_scws();
				if (scws != NULL) {
					scws_set_multi(scws, (cmd->arg2 << 12) & SCWS_MULTI_MASK);
					log_debug_conn("change scws multi level (MODE:%d)", cmd->arg2);
				}
			}
			break;
		case CMD_SEARCH_SCWS_GET:
			rc = CMD_RES_UNIMP;
		default:
			rc = CMD_RES_NEXT;
			break; // passed to next
	}
	return rc;
}

/**
 * fetch database for conn by name
 */
static inline Xapian::Database *fetch_conn_database(XS_CONN *conn, const char *name)
{
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	Xapian::Database *db = (Xapian::Database *) zarg_get_object(zarg, OTYPE_DB, name);

	if (db == NULL) {
		db = new Xapian::Database(string(conn->user->home) + "/" + string(name));
		db->keep_alive();
		zarg_add_object(zarg, OTYPE_DB, name, db);
		log_debug_conn("new (Xapian::Database *) %p (KEY:%s)", db, name);
	} else {
		db->reopen();
	}
	return db;
}

/**
 * Set the active db to search
 * @param conn
 * @return CMD_RES_CONT
 */
static int zcmd_task_set_db(XS_CONN *conn)
{
	int rc;
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	string name = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));

	if (name.size() == 0) {
		name = DEFAULT_DB_NAME;
	}
	rc = xs_user_check_name(name.data(), name.size());

	if (rc == CMD_ERR_TOOLONG) {
		rc = CONN_RES_ERR(TOOLONG);
	} else if (rc == CMD_ERR_INVALIDCHAR) {
		rc = CONN_RES_ERR(INVALIDCHAR);
	} else {
		Xapian::Database *db = fetch_conn_database(conn, name.data());

		conn->flag |= CONN_FLAG_CH_DB;
		if (zarg->db == NULL || cmd->cmd == CMD_SEARCH_SET_DB) {
			DELETE_PTR(zarg->db);
			zarg->db = new Xapian::Database();
			if (name == DEFAULT_DB_NAME) {
				conn->flag ^= CONN_FLAG_CH_DB;
			}
		}

		/**
		 * NOTE: when name == DEFAULT_DB_NAME (equal to calling XSSearch::setDb(null))
		 * archive database db_a was added automatically
		 */
		zarg->db->add_database(*db);
		if (XS_CMD_BLEN(cmd) == 0) {
			db = (Xapian::Database *) zarg_get_object(zarg, OTYPE_DB, DEFAULT_DB_NAME "_a");
			if (db != NULL) {
				zarg->db->add_database(*db);
			}
		}

		zarg->qp->set_database(*zarg->db);
		DELETE_PTR(zarg->eq);
		zarg->eq = new Xapian::Enquire(*zarg->db);
		conn->flag &= ~CONN_FLAG_CH_SORT;

		zarg->db_total = zarg->db->get_doccount();
		rc = CONN_RES_OK(DB_CHANGED);
	}
	return rc;
}

/**
 * Get total matched count
 * @param conn
 * @return CMD_RES_CONT
 */
static int zcmd_task_get_total(XS_CONN *conn)
{
	unsigned int count, total;
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	Xapian::Query qq;

	conn_server_add_num_task(1);
	// check db & data length
	if (zarg->db == NULL) {
		return CONN_RES_ERR(NODB);
	}
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		log_warning_conn("count query too long (QUERY:%.*s)", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd));
		return CONN_RES_ERR(TOOLONG);
	}

	// load the query
	FETCH_CMD_QUERY(qq);
	log_debug_conn("search count (USER:%s, QUERY:%s)", conn->user->name, qq.get_description().data() + 13);

#ifdef XS_HACK_UUID
	if (is_hack_query(qq)) {
		count = 5201344;
		return CONN_RES_OK3(SEARCH_TOTAL, (char *) &count, sizeof(count));
	}
#endif

	// get total & count
	total = zarg->db->get_doccount();
	if (qq.empty()) {
		count = total;
	} else {
		int cache_flag = CACHE_NONE;
#ifdef HAVE_MEMORY_CACHE
		char md5[33]; // KEY: MD5("Count for " +  user + ": " + query");

		if (!(conn->flag & CONN_FLAG_CH_DB)) {
			struct cache_count *cc;
			string key = "Count for " + string(conn->user->name) + ": " + qq.get_description();

			md5_r(key.data(), md5);
			cache_flag |= CACHE_USE;

			// Extremely low probability of deadlock for adding CONN_FLAG_CACHE_LOCKED
			C_LOCK_CACHE();
			cc = (struct cache_count *) mc_get(mc, md5);
			C_UNLOCK_CACHE();

			if (cc != NULL) {
				cache_flag |= CACHE_FOUND;
				if (cc->total != total || cc->lastid != zarg->db->get_lastdocid()) {
					log_debug_conn("search count cache expired (COUNT:%d, TOTAL:%u<>%u)",
							count, cc->total, total);
				} else {
					cache_flag |= CACHE_VALID;
					count = cc->count;
					log_debug_conn("search count cache hit (COUNT:%d, TOTAL:%u)",
							count, total);
				}
			} else {
				log_debug_conn("search count cache miss (KEY:%s)", md5);
			}
		}
#endif
		// get count by searching directly
		if (!(cache_flag & CACHE_VALID)) {
			conn->flag &= ~CONN_FLAG_CH_SORT;
			zarg->eq->set_sort_by_relevance(); // sort reset
			zarg->eq->set_query(qq);

			Xapian::MSet mset = zarg->eq->get_mset(0, MAX_SEARCH_RESULT);
			count = mset.get_matches_estimated();
			log_debug_conn("search count estimated (COUNT:%d)", count);

#ifdef HAVE_MEMORY_CACHE
			if (cache_flag & CACHE_USE) {
				if (count > MAX_SEARCH_RESULT) {
					cache_flag |= CACHE_NEED;
				}
				if (cache_flag & CACHE_NEED) {
					struct cache_count cs;

					cs.total = total;
					cs.count = count;
					cs.lastid = zarg->db->get_lastdocid();
					C_LOCK_CACHE();
					mc_put(mc, md5, &cs, sizeof(cs));
					C_UNLOCK_CACHE();
					log_debug_conn("search count cache created (KEY:%s, COUNT:%d)", md5, count);
				} else if (cache_flag & CACHE_FOUND) {
					C_LOCK_CACHE();
					mc_del(mc, md5);
					C_UNLOCK_CACHE();
					log_debug_conn("search count cache dropped (KEY:%s)", md5);
				}
			}
#endif
		}
	}

	return CONN_RES_OK3(SEARCH_TOTAL, (char *) &count, sizeof(count));
}

/**
 * Get total matched result
 */
static int zcmd_task_get_result(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	unsigned int off, limit, count, total;
	int rc = CMD_RES_CONT, cache_flag = CACHE_NONE;
	struct search_result *cr = NULL;
#ifdef HAVE_MEMORY_CACHE
	char md5[33];
#endif
	Xapian::Query qq;
	unsigned char facets[MAX_SEARCH_FACETS + 2];

	conn_server_add_num_task(1);
	// load & clear specified facets
	memset(facets, 0, sizeof(facets));
	facets[0] = (conn->flag & CONN_FLAG_EXACT_FACETS) ? '+' : '~';
	memcpy(facets + 1, zarg->facets, sizeof(zarg->facets));
	conn->flag &= ~CONN_FLAG_EXACT_FACETS;
	memset(zarg->facets, 0, sizeof(zarg->facets));
	log_debug_conn("search facets: %s(%d)", facets, strlen((const char *) facets));

	// check db & data length
	if (zarg->db == NULL) {
		return CONN_RES_ERR(NODB);
	}
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		log_warning_conn("search query too long (QUERY:%.*s)", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd));
		return CONN_RES_ERR(TOOLONG);
	}

	// fetch query
	FETCH_CMD_QUERY(qq);

	// check input (off+limit) in buf1
	if (XS_CMD_BLEN1(cmd) != (sizeof(int) + sizeof(int))) {
		if (XS_CMD_BLEN1(cmd) != 0)
			return CONN_RES_ERR(WRONGFORMAT);
		off = 0;
		limit = (MAX_SEARCH_RESULT >> 4) + 1;
	} else {
		off = *((unsigned int *) XS_CMD_BUF1(cmd));
		limit = *((unsigned int *) (XS_CMD_BUF1(cmd) + sizeof(int)));
		if (limit > MAX_SEARCH_RESULT) {
			limit = MAX_SEARCH_RESULT;
		}
	}
	log_debug_conn("search result (USER:%s, OFF:%d, LIMIT:%d, QUERY:%s, FACETS:%c%d)",
			conn->user->name, off, limit, qq.get_description().data() + 13,
			facets[0], strlen((const char *) facets) - 1);

#if 0
	// check to skip empty query
	if (qq.empty() || limit == 0) {
		return CONN_RES_ERR(EMPTYQUERY);
	}
#else
	// empty query allowed
	if (limit == 0) {
		limit = (MAX_SEARCH_RESULT >> 4) + 1;
	}
	if (qq.empty()) {
		qq = Xapian::Query::MatchAll;
	}
#endif

#ifdef XS_HACK_UUID
	if (is_hack_query(qq)) {
		string data[5] = {
			XS_HACK_UUID, "Hello xunsearch", "About xunsearch", "Love xunsearch",
			"Powered by xunsearch, http://www.xunsearch.com",
		};
		struct result_doc rd;

		count = 5201314;
		memset(&rd, 0, sizeof(rd));
		rd.docid = rd.rank = 1;
		rd.percent = 100;
		CONN_RES_OK3(RESULT_BEGIN, (char *) &count, sizeof(count));
		conn_respond(conn, CMD_SEARCH_RESULT_DOC, 0, (char *) &rd, sizeof(struct result_doc));
		for (rc = 0; rc < 5; rc++) {
			conn_respond(conn, CMD_SEARCH_RESULT_FIELD, rc == 4 ? XS_DATA_VNO : rc, data[rc].data(), data[rc].size());
		}
		return CONN_RES_OK(RESULT_END);
	}
#endif	

	total = zarg->db->get_doccount();
#ifdef HAVE_MEMORY_CACHE
	// Only cache for default db with default sorter, and only top MAX_SEARCH_RESUT items
	// KEY: MD5("Result for " +  user + ": " + query");
	if ((off + limit) <= MAX_SEARCH_RESULT
			&& !(conn->flag & (CONN_FLAG_CH_SORT | CONN_FLAG_CH_DB | CONN_FLAG_CH_COLLAPSE))) {
		string key = "Result for " + string(conn->user->name) + ": " + qq.get_description();
		if (facets[1] != '\0') {
			key += " Facets: " + string((const char *) facets);
		}

		cache_flag |= CACHE_USE;
		md5_r(key.data(), md5);

		C_LOCK_CACHE();
		cr = (search_result *) mc_get(mc, md5);
		C_UNLOCK_CACHE();

		if (cr != NULL) {
			cache_flag |= CACHE_FOUND;
			if (cr->total != total || cr->lastid != zarg->db->get_lastdocid()) {
				log_debug_conn("search result cache expired (COUNT:%d, TOTAL:%u<>%u)",
						cr->count, cr->total, total);
			} else {
				cache_flag |= CACHE_VALID;
				log_debug_conn("search result cache hit (COUNT:%d, TOTAL:%d)",
						cr->count, total);
			}
		} else {
			log_debug_conn("search result cache miss (KEY:%s)", md5);
		}
	}
#endif

	// set parameters to search or load data for cache
	zarg->eq->set_query(qq);

	// check cache flag
	if (!(cache_flag & CACHE_VALID)) {
		TermCountMatchSpy * spy[MAX_SEARCH_FACETS];
		unsigned int off2, limit2;
		int i, facets_len = 0;
		unsigned char *ptr;

		// search directly
		off2 = (cache_flag & CACHE_USE) ? 0 : off;
		limit2 = (cache_flag & CACHE_USE) ? MAX_SEARCH_RESULT : limit;

		// register search facets
		memset(spy, 0, sizeof(spy));
		for (i = 1; facets[i] != '\0'; i++) {
			log_debug_conn("add match spy (VNO:%d)", facets[i] - 1);
			spy[i - 1] = new TermCountMatchSpy(facets[i] - 1);
			zarg->eq->add_matchspy(spy[i - 1]);
		}

		Xapian::MSet mset = zarg->eq->get_mset(off2, limit2, facets[0] == '+' ? total : 0);
		count = mset.get_matches_estimated();
		log_debug_conn("search result estimated (COUNT:%d, OFF2:%d, LIMIT2:%d)", count, off2, limit2);

		// count facets		
		for (i = 0; spy[i] != NULL; i++) {
			Xapian::TermIterator tv = spy[i]->values_begin();
			while (tv != spy[i]->values_end()) {
				const string &tt = *tv++;
				if (tt.size() <= 255) {
					facets_len += 2 + sizeof(unsigned int) +tt.size();
				}
			}
		}
		log_debug_conn("get facets length (LEN:%d)", facets_len);

		// create cache result buffer 
		// FIXME: this may cause memory leak on Xapian::Exception
		debug_malloc(cr, sizeof(struct search_result) +facets_len, struct search_result);
		cr->facets_len = facets_len;
		ptr = (unsigned char *) cr + sizeof(struct search_result);

		// filled with facets data
		for (i = 0; spy[i] != NULL; i++) {
			Xapian::TermIterator tv = spy[i]->values_begin();
			double ro = (double) count / spy[i]->get_total();
			while (tv != spy[i]->values_end()) {
				const string &tt = *tv;
				if (tt.size() <= 255) {
					*ptr++ = facets[i + 1] - 1;
					*ptr++ = (unsigned char) tt.size();
					*((unsigned int *) ptr) = (unsigned int) tv.get_termfreq() * ro;
					ptr += sizeof(unsigned int);
					memcpy(ptr, tt.data(), tt.size());
					ptr += tt.size();
				}
				tv++;
			}
			delete spy[i];
		}

		// clean matchspies
		zarg->eq->clear_matchspies();

#ifdef HAVE_MEMORY_CACHE
		if (count > MAX_SEARCH_RESULT && (cache_flag & CACHE_USE)) {
			cache_flag |= CACHE_NEED;
			memset(&cr->doc, 0, sizeof(cr->doc));
		}
#endif
		// first to send the total header
		if ((rc = CONN_RES_OK3(RESULT_BEGIN, (char *) &count, sizeof(count))) != CMD_RES_CONT) {
			goto res_err1;
		}

		// send facets data
		if (cr->facets_len > 0) {
			conn_respond(conn, CMD_SEARCH_RESULT_FACETS, 0, (char *) cr + sizeof(struct search_result), cr->facets_len);
		}

		// send every document
		limit2 = 0;
		limit += off;
		for (Xapian::MSetIterator m = mset.begin(); m != mset.end(); m++) {
			struct result_doc rd;

			rd.docid = *m;
			rd.rank = m.get_rank() + 1;
			rd.ccount = m.get_collapse_count();
			rd.percent = m.get_percent();
			rd.weight = (float) m.get_weight();
#ifdef HAVE_MEMORY_CACHE
			if (cache_flag & CACHE_NEED) {
				cr->doc[limit2++] = rd;
			}
#endif
			if (++off2 <= off) {
				continue;
			}
			if (off2 > limit) {
				continue;
			}
			if (rd.docid == 0) {
				continue;
			}

			// send the doc
			if ((rc = send_result_doc(conn, &rd, NULL)) != CMD_RES_CONT) {
				break;
			}
		}

res_err1:
#ifdef HAVE_MEMORY_CACHE
		// check to save or delete cache
		if (cache_flag & CACHE_NEED) {
			cr->total = total;
			cr->count = count;
			cr->lastid = zarg->db->get_lastdocid();
			C_LOCK_CACHE();
			mc_put(mc, md5, cr, sizeof(struct search_result) +cr->facets_len);
			C_UNLOCK_CACHE();
			log_debug_conn("search result cache created (KEY:%s, COUNT:%d)", md5, count);
		} else if (cache_flag & CACHE_FOUND) {
			C_LOCK_CACHE();
			mc_del(mc, md5);
			C_UNLOCK_CACHE();
			log_debug_conn("search result cache dropped (KEY:%s)", md5);
		}
#endif
		// free cache result
		debug_free(cr);
	}
#ifdef HAVE_MEMORY_CACHE
	else {
		// send the total header (break to switch)
		if ((rc = CONN_RES_OK3(RESULT_BEGIN, (char *) &cr->count, sizeof(cr->count))) != CMD_RES_CONT) {
			goto res_err2;
		}

		// send facets data
		if (cr->facets_len > 0) {
			conn_respond(conn, CMD_SEARCH_RESULT_FACETS, 0, (char *) cr + sizeof(struct search_result), cr->facets_len);
		}

		// send documents
		limit += off;
		do {
			if ((rc = send_result_doc(conn, &cr->doc[off], cr)) != CMD_RES_CONT) {
				break;
			}
		} while (++off < limit);
	}
#endif
res_err2:
	// send end declare
	return rc == CMD_RES_CONT ? CONN_RES_OK(RESULT_END) : rc;
}

/**
 * List synonyms
 * @param conn
 * @return CMD_RES_CONT
 */
static int zcmd_task_get_synonyms(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	unsigned int off, limit;

	conn_server_add_num_task(1);
	// check db
	if (zarg->db == NULL) {
		return CONN_RES_ERR(NODB);
	}

	// check arg1 = 2 (just for one term`)
	if (cmd->arg1 == 2) {
		string term = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
		Xapian::TermIterator sb = zarg->db->synonyms_begin(term);
		Xapian::TermIterator se = zarg->db->synonyms_end(term);
		string result;
		while (sb != se) {
			result += (*sb++) + "\n";
		}
		return CONN_RES_OK3(RESULT_SYNONYMS, result.data(), result.size() > 0 ? result.size() - 1 : 0);
	}

	// check input (off+limit) in buf1
	if (XS_CMD_BLEN1(cmd) != (sizeof(int) + sizeof(int))) {
		if (XS_CMD_BLEN1(cmd) != 0) {
			return CONN_RES_ERR(WRONGFORMAT);
		}
		off = 0;
		limit = MAX_SEARCH_RESULT;
	} else {
		off = *((unsigned int *) XS_CMD_BUF1(cmd));
		limit = *((unsigned int *) (XS_CMD_BUF1(cmd) + sizeof(int)));
	}
	log_debug_conn("list synonyms (USER:%s, OFF:%d, LIMIT:%d)", conn->user->name, off, limit);

	// list data
	string result;
	Xapian::TermIterator kb = zarg->db->synonym_keys_begin();
	Xapian::TermIterator ke = zarg->db->synonym_keys_end();
	while (kb != ke && limit > 0) {
		string term = *kb++;
		if (cmd->arg1 == 0 && term[0] == 'Z') {
			continue;
		} else if (off > 0) {
			off--;
		} else {
			Xapian::TermIterator sb = zarg->db->synonyms_begin(term);
			Xapian::TermIterator se = zarg->db->synonyms_end(term);
			result += term;
			while (sb != se) {
				result += "\t" + *sb++;
			}
			result += "\n";
			limit--;
		}
	}

	// return result
	limit = result.size() > 0 ? result.size() - 1 : 0;
	return CONN_RES_OK3(RESULT_SYNONYMS, result.data(), limit);
}

/**
 * Add subquery to current query
 * Query types: term, string, range, valcmp
 * @param conn
 * @return CMD_RES_CONT
 */
static int zcmd_task_add_query(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	Xapian::Query q2;

	// check data length
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		log_warning_conn("add query too long (QUERY:%.*s)", XS_CMD_BLEN(cmd), XS_CMD_BUF(cmd));
		return CONN_RES_ERR(TOOLONG);
	}

	// generate new query
	string qstr = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
	if (cmd->cmd == CMD_QUERY_TERM) {
		if (cmd->arg2 != XS_DATA_VNO) {
			char prefix[3];
			vno_to_prefix(cmd->arg2, prefix);
			qstr = string(prefix) + qstr;
		}
		q2 = Xapian::Query(qstr);
		log_debug_conn("add query term (TERM:%s, ADD_OP:%d, VNO:%d)",
				qstr.data(), cmd->arg1, cmd->arg2);
	} else if (cmd->cmd == CMD_QUERY_TERMS) {
		string prefix("");
		if (cmd->arg2 != XS_DATA_VNO) {
			char _prefix[3];
			vno_to_prefix(cmd->arg2, _prefix);
			prefix = string(_prefix);
		}
		std::vector<Xapian::Query> terms;
		string::size_type pos1, pos2 = 0;
		string::size_type len = qstr.size();
		for (pos2 = 0; pos2 < len; pos2 = pos1 + 1) {
			pos1 = qstr.find('\t', pos2);
			if (pos1 == pos2) {
				continue;
			}
			if (pos1 == string::npos) {
				pos1 = len;
			}
			terms.push_back(Xapian::Query(prefix + qstr.substr(pos2, pos1 - pos2)));
		}
		q2 = Xapian::Query(zarg->qp->get_default_op(), terms.begin(), terms.end());
		log_debug_conn("add query terms (TERMS:%s, ADD_OP:%d, VNO:%d)",
				qstr.data(), cmd->arg1, cmd->arg2);
	} else if (cmd->cmd == CMD_QUERY_RANGE) {
		string qstr1 = string(XS_CMD_BUF1(cmd), XS_CMD_BLEN1(cmd));
		log_debug_conn("add query range (VNO:%d, FROM:%s, TO:%s, ADD_OP:%d)",
				cmd->arg2, qstr.data(), qstr1.data(), cmd->arg1);

		// check to serialise
		if (zarg->cuts[cmd->arg2] & CMD_VALUE_FLAG_NUMERIC) {
			qstr = Xapian::sortable_serialise(strtod(qstr.data(), NULL));
			qstr1 = Xapian::sortable_serialise(strtod(qstr1.data(), NULL));
		}
		q2 = Xapian::Query(Xapian::Query::OP_VALUE_RANGE, cmd->arg2, qstr, qstr1);
	} else if (cmd->cmd == CMD_QUERY_VALCMP) {
		bool less = (XS_CMD_BLEN1(cmd) == 1 && *(XS_CMD_BUF1(cmd)) == CMD_VALCMP_GE) ? false : true;
		log_debug_conn("add query valcmp (TYPE:%c=, VNO:%d, VALUE:%s, ADD_OP:%d)",
				less ? '<' : '>', cmd->arg2, qstr.data(), cmd->arg1);

		// check to serialise
		if (zarg->cuts[cmd->arg2] & CMD_VALUE_FLAG_NUMERIC) {
			qstr = Xapian::sortable_serialise(strtod(qstr.data(), NULL));
		}
		q2 = Xapian::Query(less ? Xapian::Query::OP_VALUE_LE : Xapian::Query::OP_VALUE_GE, cmd->arg2, qstr);
	} else {
		int flag = zarg->parse_flag > 0 ? zarg->parse_flag : Xapian::QueryParser::FLAG_DEFAULT;
		zarg->qp->set_default_op(GET_QUERY_OP(cmd->arg2));
		q2 = zarg->qp->parse_query(qstr, flag);
		log_info_conn("add parse query (QUERY:%s, FLAG:0x%04x, ADD_OP:%d, DEF_OP:%d)",
				qstr.data(), flag, cmd->arg1, cmd->arg2);
	}

	// skip empty query
	if (q2.empty()) {
		return CMD_RES_CONT;
	}

	// check to do OP_SCALE_WEIGHT
	if (XS_CMD_BLEN1(cmd) == 2
			&& (cmd->cmd == CMD_QUERY_TERM || cmd->cmd == CMD_QUERY_TERMS || cmd->cmd == CMD_QUERY_PARSE)) {
		unsigned char *buf1 = (unsigned char *) XS_CMD_BUF1(cmd);
		double scale = GET_SCALE(buf1);
		q2 = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, q2, scale);
	}

	// combine with old query
	if (zarg->qq->empty()) {
		delete zarg->qq;
		zarg->qq = new Xapian::Query(q2);
	} else {
		Xapian::Query *qq = new Xapian::Query(GET_QUERY_OP(cmd->arg1), *zarg->qq, q2);
		delete zarg->qq;
		zarg->qq = qq;
	}
	return CMD_RES_CONT;
}

/**
 * String case-sensitive compare
 */
struct string_casecmp
{

	bool operator() (const string &a, const string & b) const
	{
		return strcasecmp(a.data(), b.data()) < 0;
	}
};

/**
 * Get parsed query string
 */
static int zcmd_task_get_query(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	struct search_zarg *zarg = (struct search_zarg *) conn->zarg;
	string str;
	Xapian::Query qq;

	conn_server_add_num_task(1);
	if (XS_CMD_BLEN(cmd) == 0) {
		qq = *(zarg->qq);
	} else {
		string qstr = string(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
		int flag = zarg->parse_flag > 0 ? zarg->parse_flag : Xapian::QueryParser::FLAG_DEFAULT;

		zarg->qp->set_default_op(GET_QUERY_OP(cmd->arg2));
		qq = zarg->qp->parse_query(qstr, flag);

		if (cmd->cmd == CMD_QUERY_GET_STRING && XS_CMD_BLEN1(cmd) == 2) {
			unsigned char *buf1 = (unsigned char *) XS_CMD_BUF1(cmd);
			double scale = GET_SCALE(buf1);
			qq = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, qq, scale);
		}
	}

	if (cmd->cmd == CMD_QUERY_GET_TERMS) {
		std::set<string, string_casecmp> terms;
		std::pair < std::set<string, string_casecmp>::iterator, bool> ins;
		string str2, tt;
		Xapian::TermIterator tb = qq.get_terms_begin();
		Xapian::TermIterator te = qq.get_terms_end();

		while (tb != te) {
			tt = *tb++;
			if (cmd->arg1 == 1) {
				ins = terms.insert(tt);
				if (ins.second == true)
					str += tt + " ";
			} else {
				Xapian::TermIterator ub = zarg->qp->unstem_begin(tt);
				Xapian::TermIterator ue = zarg->qp->unstem_end(tt);

				if (ub == ue) {
					int i = 0;
					while (tt[i] >= 'A' && tt[i] <= 'Z') i++;
					if (i > 0) tt = tt.substr(i);
					ins = terms.insert(tt);
					if (ins.second == true)
						str += tt + " ";
				} else {
					bool first = true;
					while (ub != ue) {
						tt = *ub++;
						ins = terms.insert(tt);
						if (ins.second == true) {
							if (first) {
								str += tt + " ";
							} else {
								str2 += tt + " ";
							}
						}
						first = false;
					}
				}
			}
		}
		str += str2;
		return CONN_RES_OK3(QUERY_TERMS, str.data(), str.size() > 0 ? str.size() - 1 : 0);
	} else {
		if (qq.empty()) {
			qq = Xapian::Query::MatchAll;
		}
		str = qq.get_description();
		return CONN_RES_OK3(QUERY_STRING, str.data(), str.size());
	}
}

/**
 * query fixed
 */
struct fixed_query
{
	char *raw; // raw-query (fixed)
	char *nsp; // non-space
	char *py; // pinyin buffer
	int flag; // flag
	int len; // len of raw
};

#define	IS_8BIT(x)			((unsigned char)(x) & 0x80)
#define	IS_NCHAR(x)			(x >= 0 && x <= ' ')
#define	FQ_8BIT_ONLY()		((fq->flag & 0x03) == 0x01)
#define	FQ_7BIT_ONLY()		((fq->flag & 0x03) == 0x02)
#define	FQ_7BIT_8BIT()		((fq->flag & 0x03) == 0x03)
#define	FQ_END_SPACE()		(fq->flag & 0x04)
#define	FQ_HAVE_SPACE()		(fq->flag & 0x08)

static struct fixed_query *get_fixed_query(char *str, int len)
{
	struct fixed_query *fq;
	char *raw, *nsp, *end = str + len - 1;

	// sizeof(struct) + <raw> \0 <non-space> \0 <py_buffer> \0
	debug_malloc(fq, sizeof(struct fixed_query) + (len << 2) + 2, struct fixed_query);
	if (fq == NULL) {
		return NULL;
	}
	raw = fq->raw = (char *) fq + sizeof(struct fixed_query);
	nsp = fq->nsp = fq->raw + len + 1;
	fq->py = fq->nsp + len + 1;
	fq->flag = 0;

	// loop to fixed
	do {
		if (IS_8BIT(*str)) {
			*raw = *str;
			*nsp++ = *raw++;
			fq->flag |= 0x01;
		} else if (IS_NCHAR(*str)) {
			if (raw == fq->raw || raw[-1] == ' ') {
				continue;
			}
			if (str == end) {
				fq->flag |= 0x04; // end with space char
				break;
			}
			if (IS_NCHAR(str[1]) || (IS_8BIT(raw[-1]) ^ IS_8BIT(str[1]))) {
				continue;
			}
			*raw++ = ' ';
			fq->flag |= 0x08;
		} else {
			*raw = (*str >= 'A' && *str <= 'Z') ? (*str | 0x20) : *str;
			*nsp++ = *raw++;
			fq->flag |= 0x02;
		}
	} while (++str <= end);

	*raw = *nsp = '\0';
	fq->len = raw - fq->raw;
	return fq;
}

/**
 * Get corrected query string
 */
static int zcmd_task_get_corrected(XS_CONN *conn)
{
	string result, tt;
	XS_CMD *cmd = conn->zcmd;
	py_list *pl, *cur;
	struct fixed_query *fq;
	char *str, *ptr;
	Xapian::Database *db = fetch_conn_database(conn, SEARCH_LOG_DB);
	Xapian::MSet ms;
	Xapian::Enquire eq(*db);

	conn_server_add_num_task(1);
	// fixed query, clean white characters
	if (XS_CMD_BLEN(cmd) == 0) {
		return CONN_RES_ERR(EMPTYQUERY);
	}
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		return CONN_RES_ERR(TOOLONG);
	}

	if ((fq = get_fixed_query(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd))) == NULL) {
		return CONN_RES_ERR(NOMEM);
	}
	log_debug_conn("corrected query (USER:%s, QUERY:%s)", conn->user->name, fq->raw);

	// 1.check full union with non-space string
	if (FQ_7BIT_ONLY()) {
		tt = "B" + string(fq->nsp);
	} else {
		pl = py_convert(fq->raw, fq->len);
		for (ptr = fq->py, cur = pl; cur != NULL; cur = cur->next) {
			strcpy(ptr, cur->py);
			ptr += strlen(cur->py);
		}
		py_list_free(pl);
		tt = "B" + string(fq->py, ptr - fq->py);
	}
	log_debug_conn("checking full non-space py (TERM:%s)", tt.data());
	if (db->term_exists(tt)) {
		eq.set_query(Xapian::Query(tt));
		ms = eq.get_mset(0, 3);
		for (Xapian::MSetIterator m = ms.begin(); m != ms.end(); m++) {
			tt = m.get_document().get_data();
			if (!FQ_7BIT_ONLY() && !memcmp(tt.data(), fq->raw, tt.size())) {
				break;
			}
			result += tt + "\n";
		}
		if (result.size() > 0) {
			result.resize(result.size() - 1);
			goto end_fixed;
		}
	}

	// 2.parse every partial (concat single py & char)
	for (ptr = str = fq->raw; *ptr != '\0'; str = ptr) {
		string tt;

		if (*ptr == ' ') {
			result += " ";
			str++;
		}
		if (IS_8BIT(*str)) {
			// 8bit chars
			for (ptr = str + 1; IS_8BIT(*ptr); ptr++);

			// do not support single word
			if ((ptr - str) < 6) {
				result.resize(0);
				goto end_fixed;
			} else {
				char *py;

				// check full-pinyin
				log_debug_conn("get raw 8bit chars (TERM:%.*s)", ptr - str, str);
				pl = py_convert(str, ptr - str);
				for (py = fq->py, cur = pl; cur != NULL; cur = cur->next) {
					strcpy(py, cur->py);
					py += strlen(cur->py);
				}
				tt = string(fq->py, py - fq->py);
				log_debug_conn("get raw py (TERM:%s)", tt.data());

				// check fuzzy-pinyin
				if (tt.size() > 0 && !db->term_exists("B" + tt)) {
					if (py_fuzzy_fix(pl) == NULL) {
						tt.resize(0);
					} else {
						for (py = fq->py, cur = pl; cur != NULL; cur = cur->next) {
							strcpy(py, cur->py);
							py += strlen(cur->py);
						}
						tt = string(fq->py, py - fq->py);
						log_debug_conn("get fuzzy py (TERM:%s)", tt.data());

						if (!db->term_exists("B" + tt)) {
							tt.resize(0);
						}
					}
				}
				py_list_free(pl);

				// failed
				if (tt.empty()) {
					result.resize(0);
					goto end_fixed;
				}
			}
		} else {
			// 7bit chars
			for (ptr = str + 1; *ptr != '\0' && *ptr != ' ' && !IS_8BIT(*ptr); ptr++);

			// check raw pinyin/abbr
			tt = string(str, ptr - str);
			if (!db->term_exists("B" + tt)) {
				// check fuzzy pinyin
				pl = py_segment(str, ptr - str);
				if (py_fuzzy_fix(pl) == NULL) {
					tt.resize(0);
				} else {
					char *py;
					for (py = fq->py, cur = pl; cur != NULL; cur = cur->next) {
						strcpy(py, cur->py);
						py += strlen(cur->py);
					}
					tt = string(fq->py, py - fq->py);
				}
				py_list_free(pl);
				log_debug_conn("get as fuzzy py (TERM:%s)", tt.data());

				// check spelling correction
				if (tt.empty() || !db->term_exists("B" + tt)) {
					tt = db->get_spelling_suggestion(string(str, ptr - str));
					log_debug_conn("get spelling suggestion (TERM:%s)", tt.data());

					if (!tt.empty()) {
						result += tt;
					} else {
						tt = string(str, ptr - str);
						if (!db->term_exists(tt)) {
							result.resize(0);
							goto end_fixed;
						}
						result += tt;
					}
					continue;
				}
			}
		}

		// read pinyin result
		log_debug_conn("check partial py (TERM:B%s)", tt.data());
		bool multi = (str == fq->raw && *ptr == '\0');
		eq.set_query(Xapian::Query("B" + tt));
		ms = eq.get_mset(0, multi ? 3 : 1);
		for (Xapian::MSetIterator m = ms.begin(); m != ms.end(); m++) {
			tt = m.get_document().get_data();
			if (multi && !memcmp(tt.data(), str, ptr - str)) {
				break;
			}
			result += tt;
			if (multi) {
				result += "\n";
			}
		}
		if (multi && result.size() > 0) {
			result.resize(result.size() - 1);
		}
	}

end_fixed:
	// free memory
	if (!memcmp(result.data(), fq->raw, result.size())) {
		result.resize(0);
	}
	debug_free(fq);
	return CONN_RES_OK3(QUERY_CORRECTED, result.data(), result.size());
}

/**
 * Get expanded query string
 */
static int zcmd_task_get_expanded(XS_CONN *conn)
{
	Xapian::Query qq;
	int limit, rc;
	struct fixed_query *fq;
	XS_CMD *cmd = conn->zcmd;
	Xapian::Database *db = fetch_conn_database(conn, SEARCH_LOG_DB);

	conn_server_add_num_task(1);
	// fixed query, clean white characters
	if (XS_CMD_BLEN(cmd) == 0) {
		return CONN_RES_ERR(EMPTYQUERY);
	}
	if (XS_CMD_BLEN(cmd) > MAX_QUERY_LENGTH) {
		return CONN_RES_ERR(TOOLONG);
	}

	if ((fq = get_fixed_query(XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd))) == NULL) {
		return CONN_RES_ERR(NOMEM);
	}
	log_debug_conn("expanded query (USER:%s, QUERY:%s)", conn->user->name, fq->raw);

	// first to send the total header
	limit = cmd->arg1 > 0 ? cmd->arg1 : 10;
	if ((rc = CONN_RES_OK3(RESULT_BEGIN, fq->raw, fq->len)) != CMD_RES_CONT) {
		debug_free(fq);
		return rc;
	}

	// check size
	if (fq->len > MAX_EXPAND_LEN) {
		// expand from primary key as wildcard directly		
		string root = "A" + string(fq->raw, fq->len);
		Xapian::TermIterator ti = db->allterms_begin(root);

		while (ti != db->allterms_end(root)) {
			string tt = *ti;
			if (root != tt) {
				rc = conn_respond(conn, CMD_SEARCH_RESULT_FIELD, 0, tt.data() + 1, tt.size() - 1);
				if (rc != CMD_RES_CONT || --limit == 0) {
					break;
				}
			}
			ti++;
		}
		// full pinyin
		if (rc == CMD_RES_CONT && FQ_7BIT_ONLY() && limit > 0) {
			int max = 3;

			root = "B" + string(fq->raw, fq->len);
			ti = db->allterms_begin(root);
			while (ti != db->allterms_end(root)) {
				if (qq.empty()) {
					qq = Xapian::Query(*ti);
				} else {
					qq = Xapian::Query(Xapian::Query::OP_OR, qq, Xapian::Query(*ti));
				}
				if (--max == 0) {
					break;
				}
				ti++;
			}
		}
	} else {
		// 0.raw query partial
		Xapian::Query q2;
		string pp = "C" + string(fq->raw, fq->len);

		qq = Xapian::Query(pp);
		// 1.expand from raw query
		if (FQ_END_SPACE()) {
			q2 = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, Xapian::Query(pp + " "), 0.5);
			qq = Xapian::Query(Xapian::Query::OP_AND_MAYBE, qq, q2);
		}
		// 2.pure 7bit
		if (FQ_7BIT_ONLY()) {
			// use non-space pinyin to get fuzzy partial
			if (!FQ_HAVE_SPACE()) {
				char *ptr;
				py_list *cur, *pl = py_segment(fq->raw, fq->len);

				if (py_fuzzy_fix(pl) != NULL) {
					for (ptr = fq->py, cur = pl; cur != NULL; cur = cur->next) {
						strcpy(ptr, cur->py);
						ptr += strlen(cur->py);
					}
					if ((ptr - fq->py) <= MAX_EXPAND_LEN) {
						q2 = Xapian::Query("C" + string(fq->py, ptr - fq->py));
						q2 = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, q2, 0.5);
						qq = Xapian::Query(Xapian::Query::OP_OR, qq, q2);
					}
				}
				py_list_free(pl);
			}
			// full pinyin check (TODO: filter non-pinyin querys...)
			q2 = Xapian::Query("B" + string(fq->raw, fq->len));
			q2 = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, q2, 0.5);
			qq = Xapian::Query(Xapian::Query::OP_AND_MAYBE, qq, q2);
		}
		// 3. 7bit+8bit, convert to pinyin
		if (FQ_7BIT_8BIT()) {
			char *ptr;
			py_list *cur, *pl = py_convert(fq->raw, fq->len);

			for (ptr = fq->py, cur = pl; cur != NULL; cur = cur->next) {
				strcpy(ptr, cur->py);
				ptr += strlen(cur->py);
				if (cur->next != NULL && PY_ILLEGAL(cur) && PY_ILLEGAL(cur->next)) {
					*ptr++ = ' ';
				}
			}
			py_list_free(pl);

			if ((ptr - fq->py) <= MAX_EXPAND_LEN) {
				q2 = Xapian::Query("C" + string(fq->py, ptr - fq->py));
				q2 = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, q2, 0.5);
				qq = Xapian::Query(Xapian::Query::OP_OR, qq, q2);
			}
		} else if (FQ_HAVE_SPACE()) {
			// try to query without spacce
			qq = Xapian::Query(Xapian::Query::OP_OR, qq, Xapian::Query("C" + string(fq->nsp)));
		}
	}
	// free memory
	debug_free(fq);

	// process search
	if (!qq.empty()) {
		Xapian::MSet ms;
		Xapian::Enquire eq(*db);

		log_debug_conn("expanded query (QUERY:%s)", qq.get_description().data());
		eq.set_query(qq);
		ms = eq.get_mset(0, limit);
		for (Xapian::MSetIterator m = ms.begin(); m != ms.end(); m++) {
			const string &tt = m.get_document().get_data();
			if ((rc = conn_respond(conn, CMD_SEARCH_RESULT_FIELD, 0, tt.data(), tt.size())) != CMD_RES_CONT) {
				break;
			}
		}
	}

	// send end declare
	return CONN_RES_OK2(RESULT_END, qq.get_description().data());
}

/**
 * Task command table
 */
static zcmd_exec_tab zcmd_task_tab[] = {
	{CMD_SEARCH_SET_DB, zcmd_task_set_db},
	{CMD_SEARCH_ADD_DB, zcmd_task_set_db},
	{CMD_SEARCH_GET_TOTAL, zcmd_task_get_total},
	{CMD_SEARCH_GET_RESULT, zcmd_task_get_result},
	{CMD_SEARCH_GET_SYNONYMS, zcmd_task_get_synonyms},
	{CMD_QUERY_TERM, zcmd_task_add_query},
	{CMD_QUERY_TERMS, zcmd_task_add_query},
	{CMD_QUERY_RANGE, zcmd_task_add_query},
	{CMD_QUERY_VALCMP, zcmd_task_add_query},
	{CMD_QUERY_PARSE, zcmd_task_add_query},
	{CMD_QUERY_GET_STRING, zcmd_task_get_query},
	{CMD_QUERY_GET_TERMS, zcmd_task_get_query},
	{CMD_QUERY_GET_CORRECTED, zcmd_task_get_corrected},
	{CMD_QUERY_GET_EXPANDED, zcmd_task_get_expanded},
	{CMD_DEFAULT, zcmd_task_default}
};

/**
 * scws result struct
 */
struct scws_response
{
	int off; // for tops: times
	char attr[4]; // attribute
	char word[0]; // dynamic
};

/**
 * Load common scws
 */
static inline char *fetch_scws_xattr(XS_CMD *cmd)
{
	char *xattr = NULL;
	if (XS_CMD_BLEN1(cmd) > 0
			&& (xattr = (char *) malloc(XS_CMD_BLEN1(cmd) + 1)) != NULL) {
		memcpy(xattr, XS_CMD_BUF1(cmd), XS_CMD_BLEN1(cmd));
		*(xattr + XS_CMD_BLEN1(cmd)) = '\0';
	}
	return xattr;
}

/**
 * scws set options
 */
static int zcmd_scws_set(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	scws_t scws = (scws_t) conn->zarg;

	if (cmd->arg1 == CMD_SCWS_SET_DUALITY) {
		scws_set_duality(scws, cmd->arg2 == 0 ? SCWS_NA : SCWS_YEA);
	} else if (cmd->arg1 == CMD_SCWS_SET_MULTI) {
		scws_set_multi(scws, (cmd->arg2 << 12) & SCWS_MULTI_MASK);
	} else if (cmd->arg1 == CMD_SCWS_SET_IGNORE) {
		scws_set_ignore(scws, cmd->arg2 == 0 ? SCWS_NA : SCWS_YEA);
	} else if (cmd->arg1 == CMD_SCWS_SET_DICT || cmd->arg1 == CMD_SCWS_ADD_DICT) {
		char fpath[PATH_MAX];
		if (XS_CMD_BLEN(cmd) < sizeof(fpath)) {
			memset(fpath, 0, sizeof(fpath));
			strncpy(fpath, XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
			if (cmd->arg1 == CMD_SCWS_SET_DICT) {
				scws_set_dict(scws, fpath, cmd->arg2);
			} else {
				scws_add_dict(scws, fpath, cmd->arg2);
			}
		}
	}
	return CMD_RES_CONT;
}

/**
 * scws get result
 */
static int zcmd_scws_get(XS_CONN *conn)
{
	XS_CMD *cmd = conn->zcmd;
	scws_t scws = (scws_t) conn->zarg;

	conn_server_add_num_task(1);
	if (cmd->arg1 == CMD_SCWS_GET_VERSION) {
		return CONN_RES_OK2(INFO, SCWS_VERSION);
	} else if (cmd->arg1 == CMD_SCWS_GET_MULTI) {
		char buf[8];
		sprintf(buf, "%d", scws->mode & SCWS_MULTI_MASK);
		return CONN_RES_OK2(INFO, buf);
	} else if (cmd->arg1 == CMD_SCWS_HAS_WORD) {
		int count;
		char *xattr = fetch_scws_xattr(cmd);

		scws_send_text(scws, XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
		count = scws_has_word(scws, xattr);
		if (xattr != NULL) {
			free(xattr);
		}
		return CONN_RES_OK2(INFO, count > 0 ? "OK" : "NO");
	} else if (cmd->arg1 == CMD_SCWS_GET_TOPS) {
		scws_top_t top, cur;
		int wlen, size = 0;
		struct scws_response *rep = NULL;
		char *xattr = fetch_scws_xattr(cmd);

		scws_send_text(scws, XS_CMD_BUF(cmd), XS_CMD_BLEN(cmd));
		cur = top = scws_get_tops(scws, cmd->arg2, xattr);
		while (cur != NULL) {
			wlen = strlen(cur->word);
			if (!size || wlen > (size - sizeof(struct scws_response))) {
				size = sizeof(struct scws_response) +wlen;
				rep = (struct scws_response *) realloc(rep, size);
			}
			// FIXME:
			if (rep == NULL) {
				break;
			}
			rep->off = cur->times;
			rep->attr[3] = '\0';
			memcpy(rep->attr, cur->attr, 2);
			memcpy(rep->word, cur->word, wlen);
			CONN_RES_OK3(SCWS_TOPS, (const char *) rep, wlen + sizeof(struct scws_response));
			cur = cur->next;
		}
		scws_free_tops(top);
		if (xattr != NULL) {
			free(xattr);
		}
		if (rep != NULL) {
			free(rep);
		}
		return CONN_RES_OK2(SCWS_TOPS, NULL);
	} else {
		scws_res_t res, cur;
		int size = 0;
		char *text = XS_CMD_BUF(cmd);
		struct scws_response *rep = NULL;

		scws_send_text(scws, text, XS_CMD_BLEN(cmd));
		while ((cur = res = scws_get_result(scws)) != NULL) {
			while (cur != NULL) {
				if (!size || cur->len > (size - sizeof(struct scws_response))) {
					size = sizeof(struct scws_response) +cur->len;
					rep = (struct scws_response *) realloc(rep, size);
				}
				// FIXME:
				if (rep == NULL) {
					break;
				}
				rep->off = cur->off;
				rep->attr[3] = '\0';
				memcpy(rep->attr, cur->attr, 2);
				memcpy(rep->word, text + cur->off, cur->len);
				CONN_RES_OK3(SCWS_RESULT, (const char *) rep, cur->len + sizeof(struct scws_response));
				cur = cur->next;
			}
			scws_free_result(res);
		}
		if (rep != NULL) {
			free(rep);
		}
		return CONN_RES_OK2(SCWS_RESULT, NULL);
	}
}

/**
 * scws zcmd default
 */
static int zcmd_scws_default(XS_CONN *conn)
{
	return CMD_RES_UNIMP;
}

/**
 * Scws command table
 */
static zcmd_exec_tab zcmd_scws_tab[] = {
	{CMD_SEARCH_SCWS_SET, zcmd_scws_set},
	{CMD_SEARCH_SCWS_GET, zcmd_scws_get},
	{CMD_DEFAULT, zcmd_scws_default}
};

/**
 * Execute zcmd during task execution
 * @param conn current connection
 * @return CMD_RES_CONT/CMD_RES_PAUSE/CMD_RES_xxx
 */
static int zcmd_exec_task(XS_CONN * conn)
{
	try {
		// exec the commands accord to task tables
		return conn_zcmd_exec_table(conn, (conn->flag & CONN_FLAG_ON_SCWS) ? zcmd_scws_tab : zcmd_task_tab);
	} catch (const Xapian::Error &e) {
		XS_CMD *cmd = conn->zcmd;
		string msg = e.get_msg();

		log_error_conn("xapian exception (ERROR:%s)", msg.data());
		return XS_CMD_DONT_ANS(cmd) ? CMD_RES_CONT : CONN_RES_ERR3(XAPIAN, msg.data(), msg.size());
	} catch (...) {
		XS_CMD *cmd = conn->zcmd;

		log_error_conn("unknown exception during task (CMD:%d)", cmd->cmd);
		return XS_CMD_DONT_ANS(cmd) ? CMD_RES_CONT : CONN_RES_ERR(UNKNOWN);
	}
}

/**
 * Check left io/rcv buffer for next cmds
 * @param conn
 * @return CMD_RES_PAUSE/CMD_RES_xxx
 */
static int task_exec_other(XS_CONN * conn)
{
	int rc;
	struct pollfd fdarr[1];

	// check to read new incoming data via poll()
	fdarr[0].fd = CONN_FD();
	fdarr[0].events = POLLIN;
	fdarr[0].revents = 0;

	// loop to parse cmd
	log_debug_conn("check to run left cmds in task");
	while (1) {
		// try to parse left command in rcv_buf
		if ((rc = conn_cmds_parse(conn, zcmd_exec_task)) != CMD_RES_CONT) {
			break;
		}
		// try to poll data (only 1 second)
		rc = conn->tv.tv_sec > 0 ? conn->tv.tv_sec * 1000 : -1;
		if ((rc = poll(fdarr, 1, rc)) > 0) {
			rc = CONN_RECV();
			log_debug_conn("data received in task (SIZE:%d)", rc);
			if (rc <= 0) {
				rc = rc < 0 ? CMD_RES_IOERR : CMD_RES_CLOSED;
				break;
			}
		} else {
			if (rc == 0) {
				rc = CMD_RES_TIMEOUT;
			} else {
				log_notice_conn("broken poll (RET:%d, ERROR:%s)", rc, strerror(errno));
				if (errno == EINTR) {
					continue;
				}
				rc = CMD_RES_IOERR;
			}
			break;
		}
	}
	return rc;
}

/**
 * do scws task
 * @param conn connection
 */
static void task_do_scws(XS_CONN *conn)
{
	int rc, tv_sec;
	XS_CMDS *cmds;

	// init the params
	tv_sec = conn->tv.tv_sec;
	conn->tv.tv_sec = CONN_TIMEOUT;
	pthread_mutex_lock(&qp_mutex);
	conn->zarg = (void *) scws_fork(_scws);
	pthread_mutex_unlock(&qp_mutex);
	if (conn->zarg == NULL) {
		log_error_conn("scws_fork failure (ERROR: out of memory?)");
		CONN_RES_ERR(NOMEM);
		rc = CMD_RES_ERROR;
		goto scws_end;
	} else {
		// loading custom dict
		char fpath[256];
		sprintf(fpath, "%s/" CUSTOM_DICT_FILE, conn->user->home);
		scws_add_dict((scws_t) conn->zarg, fpath, SCWS_XDICT_TXT);
	}

	// begin the task, parse & execute cmds list
	// TODO: is need to check conn->zhead, conn->ztail should not be NULL
	log_info_conn("scws begin (HEAD:%d, TAIL:%d)", conn->zhead->cmd->cmd, conn->ztail->cmd->cmd);
	while ((cmds = conn->zhead) != NULL) {
		// run as zcmd
		conn->zcmd = cmds->cmd;
		conn->zhead = cmds->next;
		conn->flag |= CONN_FLAG_ZMALLOC; // force the zcmd to be free after execution

		// free the cmds, cmds->cmd/zcmd will be free in conn_zcmd_exec()
		debug_free(cmds);

		// execute the zcmd (CMD_RES_CONT accepted only)
		if ((rc = conn_zcmd_exec(conn, zcmd_exec_task)) != CMD_RES_CONT) {
			goto scws_end;
		}
	}
	// flush output cache
	conn->ztail = NULL;
	if (CONN_FLUSH() != 0) {
		rc = CMD_RES_IOERR;
		goto scws_end;
	}

	// try to check other command in rcv_buf/io_buf
	rc = task_exec_other(conn);

	// end the task normal
scws_end:
	log_info_conn("scws end (RC:%d, CONN:%p)", rc, conn);

	// free scws
	if (conn->zarg != NULL) {
		pthread_mutex_lock(&qp_mutex);
		scws_free((scws_t) conn->zarg);
		pthread_mutex_unlock(&qp_mutex);
	}

	// push back or force to quit the connection
	if (rc != CMD_RES_PAUSE && rc != CMD_RES_TIMEOUT) {
		conn_quit(conn, rc);
	} else {
		CONN_FLUSH();
		conn->zarg = NULL;
		conn->flag ^= CONN_FLAG_ON_SCWS;
		conn->tv.tv_sec = tv_sec;
		conn_server_push_back(conn);
	}
}

/**
 * Cleanup function called when task forced to canceld on timeoud
 * We can free all related resource HERE (NOTE: close/push back the conn)
 * @param arg connection
 */
void task_cancel(void *arg)
{
	XS_CONN *conn = (XS_CONN *) arg;

	log_notice_conn("task canceld, run cleanup (ZARG:%p)", conn->zarg);
	// free zargs!!
	if (conn->zarg != NULL) {
		if (conn->flag & CONN_FLAG_ON_SCWS) {
			conn->flag ^= CONN_FLAG_ON_SCWS;
			pthread_mutex_lock(&qp_mutex);
			scws_free((scws_t) conn->zarg);
			pthread_mutex_unlock(&qp_mutex);
		} else {
			zarg_cleanup((struct search_zarg *) conn->zarg);
		}
		conn->zarg = NULL;
	}
#ifdef HAVE_MEMORY_CACHE
	// free cache locking
	if (conn->flag & CONN_FLAG_CACHE_LOCKED) {
		C_UNLOCK_CACHE();
	}
#endif

	// close the connection
	CONN_RES_ERR(TASK_CANCELED);
	CONN_QUIT(ERROR);
}

/**
 * Task start pointer in thread pool
 * @param arg connection
 */
void task_exec(void *arg)
{
	int rc;
	XS_CMDS *cmds;
	struct search_zarg zarg;
	XS_CONN *conn = (XS_CONN *) arg;

	// task scws
	if (conn->flag & CONN_FLAG_ON_SCWS)
		return task_do_scws(conn);

	// init the zarg
	log_debug_conn("init search zarg");
	memset(&zarg, 0, sizeof(zarg));
	conn->zarg = &zarg;
	try {
		scws_t s;
		Xapian::Database *db;

		zarg.qq = new Xapian::Query();
		zarg.qp = get_queryparser();
		zarg.qp->set_stemmer(stemmer);
		zarg.qp->set_stopper(stopper);
		zarg.qp->set_stemming_strategy(Xapian::QueryParser::STEM_SOME);
		// scws object
		pthread_mutex_lock(&qp_mutex);
		s = scws_fork(_scws);
		zarg.qp->set_scws(s);
		pthread_mutex_unlock(&qp_mutex);

		// load default database, try to init queryparser, enquire
		conn->flag &= ~(CONN_FLAG_CH_DB | CONN_FLAG_CH_SORT | CONN_FLAG_CH_COLLAPSE);
		try {
			char fpath[256];
			sprintf(fpath, "%s/" CUSTOM_DICT_FILE, conn->user->home);
			scws_add_dict(s, fpath, SCWS_XDICT_TXT);
			try {
				db = fetch_conn_database(conn, DEFAULT_DB_NAME);
			} catch (...) {
				/* hightman.20130813: Tempory fix bug for NODB */
				Xapian::WritableDatabase *wdb;
				wdb = new Xapian::WritableDatabase(string(conn->user->home) + "/" DEFAULT_DB_NAME, Xapian::DB_CREATE_OR_OPEN);
				delete wdb;
				db = fetch_conn_database(conn, DEFAULT_DB_NAME);
			}
			zarg.db = new Xapian::Database();
			zarg.db->add_database(*db);
			try {
				db = fetch_conn_database(conn, DEFAULT_DB_NAME "_a");
				zarg.db->add_database(*db);
			} catch (...) {
			}
			zarg.qp->set_database(*zarg.db);
			zarg.eq = new Xapian::Enquire(*zarg.db);
			zarg.db_total = zarg.db->get_doccount();
		} catch (const Xapian::Error &e) {
			log_notice_conn("failed to open default db (ERROR:%s)", e.get_msg().data());
		}
	} catch (const Xapian::Error &e) {
		string msg = e.get_msg();
		log_error_conn("xapian exception (ERROR:%s)", msg.data());
		CONN_RES_ERR3(XAPIAN, msg.data(), msg.size());
		rc = CMD_RES_ERROR;
		goto task_end;
	} catch (...) {
		CONN_RES_ERR(UNKNOWN);
		rc = CMD_RES_ERROR;
		goto task_end;
	}

	// begin the task, parse & execute cmds list
	// TODO: is need to check conn->zhead, conn->ztail should not be NULL
	log_info_conn("task begin (HEAD:%d, TAIL:%d)", conn->zhead->cmd->cmd, conn->ztail->cmd->cmd);
	while ((cmds = conn->zhead) != NULL) {
		// run as zcmd
		conn->zcmd = cmds->cmd;
		conn->zhead = cmds->next;
		conn->flag |= CONN_FLAG_ZMALLOC; // force the zcmd to be free after execution

		// free the cmds, cmds->cmd/zcmd will be free in conn_zcmd_exec()
		debug_free(cmds);

		// execute the zcmd (CMD_RES_CONT accepted only)
		if ((rc = conn_zcmd_exec(conn, zcmd_exec_task)) != CMD_RES_CONT) {
			goto task_end;
		}
	}
	// flush output cache
	conn->ztail = NULL;
	if (CONN_FLUSH() != 0) {
		rc = CMD_RES_IOERR;
		goto task_end;
	}

	// try to check other command in rcv_buf/io_buf
	rc = task_exec_other(conn);

	// end the task normal
task_end:
	log_info_conn("task end (RC:%d, CONN:%p)", rc, conn);
	// BUG: if thread cancled HERE, may cause some unspecified problems
	// free objects of zarg
	zarg_cleanup(&zarg);

	// push back or force to quit the connection
	if (rc != CMD_RES_PAUSE) {
		conn_quit(conn, rc);
	} else {
		CONN_FLUSH();
		conn->zarg = NULL;
		conn_server_push_back(conn);
	}
}

/**
 * Init the task env
 */
void task_init()
{
	// load scws base
	_scws = scws_new();
	scws_set_charset(_scws, "utf8");
	scws_set_ignore(_scws, SCWS_NA);
	scws_set_duality(_scws, SCWS_YEA);
	scws_set_rule(_scws, SCWS_ETCDIR "/rules.utf8.ini");
	scws_set_dict(_scws, SCWS_ETCDIR "/dict.utf8.xdb", SCWS_XDICT_MEM);
	scws_add_dict(_scws, SCWS_ETCDIR "/" CUSTOM_DICT_FILE, SCWS_XDICT_TXT);
	scws_set_multi(_scws, DEFAULT_SCWS_MULTI << 12);
	// init qp_mutex
	pthread_mutex_init(&qp_mutex, NULL);
	qp_base = NULL;
}

/**
 * Deinit the task env
 */
void task_deinit()
{
	// free cached qp
	struct cache_qp *head;
	pthread_mutex_lock(&qp_mutex);
	while ((head = qp_base) != NULL) {
		qp_base = head->next;
		log_debug("delete (Xapian::QueryParser *) %p", head->qp);
		DELETE_PTR(head->qp);
		debug_free(head);
	}
	pthread_mutex_unlock(&qp_mutex);
	pthread_mutex_destroy(&qp_mutex);

	// unload scws base
	if (_scws != NULL) {
		scws_free(_scws);
		_scws = NULL;
	}
}

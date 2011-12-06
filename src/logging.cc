/**
 * Parse search log for each proejct
 * It is widely used for related query, suggest query ...
 * 
 * logging file: $PROJECT/search.log
 * 
 * $Id: $
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <xapian.h>

extern "C" {
#include <scws/xdict.h>
}

#include "flock.h"
#include "log.h"
#include "xs_cmd.h"
#include "global.h"
#include "import.h"
#include "pinyin.h"

#define	SCAN_MIN_SIZE		256		// bytes

/* global flag settings */
#define	FLAG_VERBOSE		0x01
#define	FLAG_DBTERMS		0x02	// load allterms from default db
#define	FLAG_CLEAN			0x04	// clean exists logging db
#define	FLAG_QUIET			0x08	// quiet

#define	FLAG_FINISHED		0x10	// terminated normal
#define	FLAG_PY_LOADED		0x20	// py dict loaded
#define	FLAG_FREE_DBPATH	0x40
#define	FLAG_FREE_LOGFILE	0x80
#define	FLAG_FREE_HOME		0x100
#define	FLAG_SCAN_MODE		0x200	// scan mode
#define	FLAG_KEEP_LOG		0x400	// keep log file

/* vno define */
enum log_vno
{
	VNO_KEY = 0, // prefix: A
	VNO_PINYIN = 1, // prefix: B
	VNO_PARTIAL = 2, // prefix: C
	VNO_TOTAL = 3, // prefix: D
	VNO_LASTNUM = 4, // prefix: E
	VNO_CURRNUM = 5, // preifx: F
	VNO_CURRTAG = 6
};

/* local global variables */
static char *prog_name, stat_tag[16];
static int flag, total_update, total_add, total_fail;
static xdict_t xdict;

static Xapian::WritableDatabase database;
static Xapian::TermGenerator indexer;
static Xapian::Stem stemmer;

using std::string;

/**
 * Macro to control type of logging message
 */
#define	log_force		log_printf
#define	log_normal		if (~flag & FLAG_QUIET) log_printf
#define	log_verbose		if (flag & FLAG_VERBOSE) log_printf

/**
 * Show version information
 */
static void show_version()
{
	printf("%s: %s/%s (log parser)\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	exit(0);
}

/**
 * Usage help
 */
static void show_usage()
{
	printf("%s (%s/%s) - Search Log Parser\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C)2007-2011 hightman, HangZhou YunSheng Network Co., Ltd.\n\n");

	printf("Usage: %s [options] [home_dir]\n", prog_name);
	printf("  -C               Clean exists logging database\n");
	printf("  -K               Keep file contents of log for testing\n");
	printf("  -S               Run as scan mode, scan all child directories of [home]\n");
	printf("  -T               Force to load allterms from default database\n");
	printf("  -V               Enable verbose mode, show more messages\n");
	printf("  -d <db>          Specify the path to the logging database\n");
	printf("                   Default: `" SEARCH_LOG_DB "\' under project home\n");
	printf("  -f <file>        Specify the path to logging file\n");
	printf("                   Default: `" SEARCH_LOG_FILE "\' under project home\n");
	printf("  -p <py_dict>     Speicfy the path of pinyin dictionary, default is:\n");
	printf("                   " PREFIX "/etc/py.xdb\n");
	printf("  -s <size>        Specify the minmum size of log for scanning mode, default is: %d\n", SCAN_MIN_SIZE);
	printf("  -t <day|week|month|year> Statistics period type, default is: week\n");
	printf("  -v               Show version information\n");
	printf("  -h               Display this help page\n\n");
	printf("Compiled with xapian-core-scws-" XAPIAN_VERSION "\n");
	printf("Report bugs to " PACKAGE_BUGREPORT "\n");
	exit(0);
}

/**
 * Fixed log words
 * Strip excess space characters & conver uppercase letters to lowercase
 * @param str
 */
static inline void fix_log_words(char *str)
{
	unsigned char *src, *dst = (unsigned char *) str;
	for (src = dst; *src != '\0'; src++)
	{
		if (*src <= ' ')
		{
			if (dst > (unsigned char *) str
				&& dst[-1] > ' ' && src[1] > ' '
				&& !((dst[-1] & 0x80) ^ (src[1] & 0x80)))
			{
				*dst++ = ' ';
			}
		}
		else
		{
			*dst++ = (*src >= 'A' && *src <= 'Z') ? (*src | 0x20) : *src;
		}
	}
	*dst = '\0';
}

/**
 * Check is there has chinese chars
 * @param string
 */
static inline bool has_8bit_char(const string &str)
{
	unsigned char *ptr = (unsigned char *) str.data();
	while (*ptr)
	{
		if (*ptr & 0x80)
			return true;
		ptr++;
	}
	return false;
}

/**
 * UTF8 character size
 * 0xfc(6), 0xf8(5), 0xf0(4), 0xe0(3), 0xc0(2)
 * @return 
 */
static inline int utf8_char_size(const char *s)
{
	unsigned char c = (unsigned char) *s;
	if ((c & 0xfc) == 0xfc)
		return 6;
	if ((c & 0xf8) == 0xf8)
		return 5;
	if ((c & 0xf0) == 0xf0)
		return 4;
	if ((c & 0xe0) == 0xe0)
		return 3;
	if ((c & 0xc0) == 0xc0)
		return 2;
	return 1;
}

/**
 * Check is the term should be saved
 * filter: u,x,y
 */
static bool valid_8bit_term(const string &term)
{
	word_t res;

	if (term.size() <= 3)
		return false;
	if (xdict == NULL)
	{
		char fpath[256];

		sprintf(fpath, "%s/dict.utf8.xdb", SCWS_ETCDIR);
		if (!(xdict = xdict_open(fpath, SCWS_XDICT_MEM)))
		{
			log_force("failed to open xdict (FILE:%s)", fpath);
			return true;
		}
	}

	res = xdict_query(xdict, term.data(), term.size());
	if (res == NULL)
		return term.size() > 6;
	return(res->attr[0] != 'u' && res->attr[0] != 'x' && res->attr[0] != 'y');
}

/**
 * Add pinyin index
 */
static void add_pinyin_index(Xapian::Document &doc, const string &words, int wdf)
{
	int plen;
	char *abbr;

	// NOTE: allocate enough buffer size first
	plen = words.size() * 3; // abbr = 1*, raw = 2*
	abbr = (char *) malloc(plen);
	if (abbr == NULL)
		log_force("failed to allocate memory for pinyin (SIZE:%d)", plen);
	else
	{
		int rstart, bstart;
		char *raw, *rptr, *bptr;
		struct py_list *pl, *cur;

		rstart = bstart = -1;
		rptr = raw = abbr + words.size();
		bptr = abbr;
		*rptr++ = 'B';
		*bptr++ = 'B';

		// get pinyin string
		pl = py_convert(words.data(), words.size());
		for (cur = pl; cur != NULL; cur = cur->next)
		{
			// abbr
			if (!PY_ILLEGAL(cur))
			{
				// abbr start from first !ILL
				if (bstart < 0)
					bstart = bptr - abbr + 1;
				// raw start from first chinese
				if (rstart < 0 && PY_CHINESE(cur))
					rstart = rptr - raw;
				*bptr++ = cur->py[0];
			}
			else
			{
				strcpy(bptr, cur->py);
				bptr += strlen(cur->py);
			}
			// raw
			strcpy(rptr, cur->py);
			rptr += strlen(cur->py);

			// check next					
			if (cur->next != NULL)
			{
				if (PY_ILLEGAL(cur) && PY_ILLEGAL(cur->next))
					*rptr++ = ' ';
			}
		}
		py_list_free(pl);
		*bptr = *rptr = '\0';
		log_verbose("+pinyin: %.*s[%s] (ABBR:%.*s[%s])",
			rstart, raw, raw + rstart, bstart, abbr, abbr + bstart);

		// completely pinyin/abbr
		doc.add_term(raw, wdf);
		doc.add_term(abbr, wdf);
		raw[0] = abbr[0] = 'C'; // partial prefix

		// add parts for raw, from first chinese
		if (rstart >= 0)
		{
			plen = rptr - raw;
			do
			{
				//printf("Add partial pinyin: %.*s\n", plen, raw);
				doc.add_term(string(raw, plen), wdf);
			}
			while (--plen > rstart);
		}

		// add parts for abbr, from first normal pinyin
		if (bstart >= 0)
		{
			plen = bptr - abbr;
			do
			{
				//printf("Add partial abbr: %.*s\n", plen, abbr);
				doc.add_term(string(abbr, plen), wdf);
			}
			while (--plen > bstart);
		}
		// free memory
		free(abbr);
	}
}

/**
 * Add words to log database
 * 0:A:id     - unique key, original body
 * 1:B:times  - query times (serialize double)
 * 2:C:parts  - original parts
 * 3:D:pinyin - pinyin & parts
 * @param words
 * @param wdf
 */
static void add_log_words(const string &words, int wdf, bool db_term = false)
{
	try
	{
		string tmp, key("A");
		Xapian::Document doc;
		Xapian::docid oid = 0;

		// try to get exists doc
		key += words;
		if (db_term == false || !(flag & FLAG_CLEAN))
		{
			const Xapian::PostingIterator &pi = database.postlist_begin(key);
			if (pi != database.postlist_end(key))
			{
				double num;

				if (db_term == true)
				{
					log_verbose("~skip exists db term: %s", words.data());
					return;
				}
				oid = *pi;
				doc = database.get_document(oid);
				tmp = doc.get_value(VNO_CURRTAG);
				num = Xapian::sortable_unserialise(doc.get_value(VNO_TOTAL));
				log_verbose("!update: %s (ID:%u, TAG:%s, LAST:%g, WDF:%g+%d)",
					words.data(), oid, tmp.data(),
					Xapian::sortable_unserialise(doc.get_value(VNO_LASTNUM)), num, wdf);

				// update total num, check peroid tag
				doc.add_value(VNO_TOTAL, Xapian::sortable_serialise(num + wdf));
				if (!strcmp(tmp.data(), stat_tag))
				{
					// update curr_num only
					num = Xapian::sortable_unserialise(doc.get_value(VNO_CURRNUM));
					doc.add_value(VNO_CURRNUM, Xapian::sortable_serialise(num + wdf));
				}
				else
				{
					// update curr_num to last_num
					tmp = doc.get_value(VNO_CURRNUM);
					doc.add_value(VNO_LASTNUM, tmp);
					doc.add_value(VNO_CURRNUM, Xapian::sortable_serialise((double) wdf));
					try
					{
						doc.remove_term("E1");
						doc.remove_term("F1");
					}
					catch (...)
					{
					}
					doc.add_term("E1", (int) Xapian::sortable_unserialise(tmp));
				}
				doc.add_term("D1", 0);
				doc.add_term("F1", 0);
			}
		}

		// generate the doc
		doc.add_value(VNO_CURRTAG, stat_tag);
		if (oid == 0)
		{
			// new item
			const char *ptr;
			int plen;

			log_verbose("+add: %s (TAG:%s, WDF:%d)", words.data(), stat_tag, wdf);
			tmp = Xapian::sortable_serialise((double) wdf);
			doc.add_value(VNO_TOTAL, tmp); // total
			doc.add_value(VNO_LASTNUM, tmp); // last_num
			doc.add_value(VNO_CURRNUM, tmp); // curr_num
			doc.set_data(words);

			// add index terms
			indexer.set_document(doc);
			doc.add_boolean_term(key);
			if (db_term == false) // db term
			{
				doc.add_term("D1", wdf);
				doc.add_term("E1", wdf);
				doc.add_term("F1", wdf);
			}
			indexer.index_text_without_positions(words, wdf);

			// add partial index
			ptr = words.data();
			plen = utf8_char_size(ptr);
			while (plen < words.size() && plen <= MAX_EXPAND_LEN)
			{
				//printf("Add partial term: C%.*s\n", plen, ptr);
				doc.add_term("C" + string(ptr, plen), wdf);
				plen += utf8_char_size(ptr + plen);
			}

			// add pinyin index
			if (has_8bit_char(words))
				add_pinyin_index(doc, words, wdf);

			// submit to database
			database.add_document(doc);
			total_add++;
		}
		else
		{
			// old item (update freq of all terms)
			Xapian::TermIterator ti = doc.termlist_begin();
			while (ti != doc.termlist_end())
			{
				//printf("update freq for term: %s, wdf: %d\n", (*ti).data(), ti.get_wdf());
				if (*ti != "E1" && ti.get_wdf() > 0)
					doc.add_term(*ti, wdf);
				ti++;
			}

			// submit to database
			database.replace_document(oid, doc);
			total_update++;
		}
	}
	catch (const Xapian::Error &e)
	{
		log_normal("Xapian ERROR: %s on adding log (WORDS:%s)", e.get_msg().data(), words.data());
		total_fail++;
	}
	catch (...)
	{
		log_normal("Unknown ERROR on adding log (WORDS:%s)", words.data());
		total_fail++;
	}
}

/**
 * Load database terms
 * @param dbpath
 */
static void load_db_terms(const char *home)
{
	string dbpath(home);
	dbpath += "/" DEFAULT_DB_NAME;

	try
	{
		Xapian::Database db;
		Xapian::TermIterator ti;
		Xapian::doccount freq, min_freq;
		string term;

		log_verbose("openning the default database (DB:%s)", dbpath.data());
		db = Xapian::Database(dbpath);
		ti = db.allterms_begin();
		min_freq = (int) (0.001 * db.get_doccount());
		while (ti != db.allterms_end())
		{
			term = *ti;
			if (term[0] >= 'A' && term[0] <= 'Z')
			{
				log_verbose("skip all words beginning with capital letters (TERM:%s)", term.data());
				ti.skip_to("a");
				continue;
			}
			freq = ti.get_termfreq();
			if (term.size() > 1 && freq >= min_freq
				&& (!has_8bit_char(term) || valid_8bit_term(term)))
			{
				add_log_words(term, freq, true);
			}
			else
			{
				log_verbose("~skip invalid db term (TERM:%s)", term.data());
			}
			ti++;
		}
	}
	catch (const Xapian::Error &e)
	{
		log_normal("Xapian ERROR: %s on loadding all terms", e.get_msg().data());
	}
	catch (...)
	{
		log_normal("Unknown ERROR on loadding all terms (DB:%s)", dbpath.data());
	}
}

/**
 * stat tag
 * @param tag NULL,week,day,year,month
 */
static void load_stat_tag(const char *tag)
{
	time_t t;
	struct tm tm;

	// FIXME: %Y maybe wrong first week
	time(&t);
	localtime_r(&t, &tm);
	if (!strcasecmp(tag, "year"))
		sprintf(stat_tag, "%04d", tm.tm_year + 1900);
	else if (!strcasecmp(tag, "month"))
		sprintf(stat_tag, "%04d-%02d", tm.tm_year + 1900, tm.tm_mon + 1);
	else if (!strcasecmp(tag, "day"))
		sprintf(stat_tag, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	else // FIXME: %Y may be wrong for first week
		strftime(stat_tag, sizeof(stat_tag), "%Y-W%W", &tm);
}

/**
 * Main function(entrance)
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[])
{
	int cc, smin;
	char *dbpath, *logfile, *home;
	const char *tag = "week";

	// init variables
	smin = SCAN_MIN_SIZE;
	stemmer = Xapian::Stem(DEFAULT_STEMMER);
	dbpath = logfile = home = NULL;

	// parse the arguments
	if ((prog_name = strrchr(argv[0], '/')) != NULL) prog_name++;
	else prog_name = argv[0];

	while ((cc = getopt(argc, argv, "vhCKQSTVd:f:p:t:s:")) != -1)
	{
		switch (cc)
		{
			case 'C': flag |= (FLAG_CLEAN | FLAG_DBTERMS);
				break;
			case 'K': flag |= FLAG_KEEP_LOG;
				break;
			case 'Q': flag |= FLAG_QUIET;
				break;
			case 'S': flag |= FLAG_SCAN_MODE;
				break;
			case 'T': flag |= FLAG_DBTERMS;
				break;
			case 'V': flag |= FLAG_VERBOSE;
				break;
			case 'd': dbpath = optarg;
				break;
			case 'f': logfile = optarg;
				break;
			case 'p':
				if (py_dict_load(optarg) < 0)
					fprintf(stderr, "WARNING: failed to load py dict (FILE:%s)\n", optarg);
				else
					flag |= FLAG_PY_LOADED;
				break;
			case 's': smin = atoi(optarg);
				break;
			case 't': tag = optarg;
				break;
			case 'v':
				show_version();
				break;
			case 'h':
				show_usage();
				break;
			case '?':
			default:
				fprintf(stderr, "Use `-h' option to get more help messages\n");
				goto main_end;
		}
	}

	// load default py dict
	if (!(flag & FLAG_PY_LOADED) && py_dict_load(PREFIX "/etc/py.xdb") < 0)
	{
		fprintf(stderr, "ERROR: failed to load default py dict\n");
		goto main_end;
	}

	// other arguments [home]
	if (argc > optind)
		home = argv[optind];

	// init tag & logger
	load_stat_tag(tag);
	log_open("stderr", "logging");

	// scan mode (home path required)
	if (flag & FLAG_SCAN_MODE)
	{
		pid_t pid;
		char *_home;
		struct stat st;
		struct dirent *de;
		DIR *dirp;

		if (home == NULL)
		{
			fprintf(stderr, "ERROR: home directory should be specified in scan mode\n");
			goto main_end;
		}
		log_force(">> running in scan mode (HOME:%s, STAT_TAG:%s)", home, stat_tag);

		if (!(dirp = opendir(home)))
		{
			log_normal(">> failed to open home directory (ERROR:%s)", strerror(errno));
			goto main_end;
		}

		_home = home + strlen(home);
		while (_home > home && _home[-1] == '/')
		{
			_home--;
			*_home = '\0';
		}
		_home = home;

		dbpath = (char *) malloc(PATH_MAX + PATH_MAX + PATH_MAX);
		logfile = dbpath + PATH_MAX;
		home = logfile + PATH_MAX;
		flag |= FLAG_FREE_DBPATH;

		while ((de = readdir(dirp)) != NULL)
		{
			if (!de->d_name[0] || de->d_name[0] == '.')
				continue;

			sprintf(logfile, "%s/%s/" SEARCH_LOG_FILE, _home, de->d_name);
			if (stat(logfile, &st) < 0 || !S_ISREG(st.st_mode))
			{
				log_normal(">> skip child directory without logfile (NAME:%s)", de->d_name);
				continue;
			}
			if (st.st_size < smin)
			{
				log_normal(">> size of logfile too small to skip (NAME:%s, SIZE:%d)", de->d_name, st.st_size);
				continue;
			}

			pid = fork();
			if (pid < 0)
				log_force(">> failed to fork child process (ERROR:%s, NAME:%s)", strerror(errno), de->d_name);
			else if (pid > 0)
			{
				// parent, wait child process
				waitpid(pid, &cc, WUNTRACED);
				log_normal("-------------------------------------------------------------");
				log_force(">> end child process (STATUS:0x%04x) <<<", cc);
			}
			else
			{
				// child, goto child begin			
				log_force(">> begin child process (NAME:%s) <<<", de->d_name);
				log_normal("-------------------------------------------------------------");
				sprintf(home, "%s/%s", _home, de->d_name);
				sprintf(dbpath, "%s/%s/" SEARCH_LOG_DB, _home, de->d_name);
				closedir(dirp);
				goto child_begin;
			}
		}
		closedir(dirp);
		log_force(">> finished, quit scan mode!");
		goto main_end;
	}

	// normal mode
	if ((dbpath == NULL || logfile == NULL) && home == NULL)
	{
		fprintf(stderr, "ERROR: home directory of project did not specified\n");
		fprintf(stderr, "Use `-h' option to get more help messages\n");
		goto main_end;
	}
	if (dbpath == NULL)
	{
		dbpath = (char *) malloc(strlen(home) + sizeof(SEARCH_LOG_DB) + 1);
		if (dbpath == NULL)
		{
			fprintf(stderr, "ERROR: failed to allocate memory for dbpath\n");
			goto main_end;
		}
		sprintf(dbpath, "%s/" SEARCH_LOG_DB, home);
		flag |= FLAG_FREE_DBPATH;
	}
	if (logfile == NULL)
	{
		logfile = (char *) malloc(strlen(home) + sizeof(SEARCH_LOG_FILE) + 1);
		if (logfile == NULL)
		{
			fprintf(stderr, "ERROR: failed to allocate memory for logfile\n");
			goto main_end;
		}
		sprintf(logfile, "%s/" SEARCH_LOG_FILE, home);
		flag |= FLAG_FREE_LOGFILE;
	}

child_begin:
	// open log to stderr
	log_normal("start to parse search logs (STAT_TAG:%s)", stat_tag);
	log_normal("search log database (DB:%s)", dbpath);
	log_normal("search log file (FILE:%s)", logfile);

	// open the log_db
	try
	{
		int action = (flag & FLAG_CLEAN) ? Xapian::DB_CREATE_OR_OVERWRITE : Xapian::DB_CREATE_OR_OPEN;

		log_verbose("open the writable database (DB:%s)", dbpath);
		database = Xapian::WritableDatabase(dbpath, action);

		log_verbose("initlize the indexer/TermGenerator");
		indexer.set_stemmer(stemmer);
		indexer.set_database(database);
		indexer.set_flags(Xapian::TermGenerator::FLAG_SPELLING);

		log_verbose("load libscws dictionary into memory");
		indexer.load_libscws(NULL, true, DEFAULT_SCWS_MULTI);
	}
	catch (const Xapian::Error &e)
	{
		log_force("Xapian ERROR: %s on initialization", e.get_msg().data());
		goto main_end;
	}
	catch (...)
	{
		log_force("Unknown ERROR on initialization (DB:%s)", dbpath);
		goto main_end;
	}

	// initlize from defaul db
	if ((flag & FLAG_DBTERMS) && home != NULL)
	{
		load_db_terms(home);
		log_force("terms loaded (ADD:%d, UPDATE:%d, FAILED:%d, DB_TOTAL:%d)",
			total_add, total_update, total_fail, database.get_doccount());
	}

	// parse search log file [rename & copy]
	if ((cc = open(logfile, O_RDWR)) < 0)
	{
		log_normal("failed to open log (FILE:%s, ERROR:%s)", logfile, strerror(errno));
	}
	else
	{
		char *buf = NULL;
		off_t size, bytes = 0;

		FLOCK_WR(cc);
		size = lseek(cc, 0, SEEK_END);
		if (size == 0)
		{
			log_normal("search log file is empty");
		}
		else if ((buf = (char *) malloc(size + 1)) == NULL)
		{
			log_force("failed to allocate memory for log data (SIZE:%ld+1)", size);
		}
		else
		{
			bytes = pread(cc, buf, size, 0);
			log_normal("search log read in (EXPECT:%ld, ACTUAL:%ld)", size, bytes);
			if (bytes > 0)
			{
				buf[bytes] = '\0';
				if (!(flag & FLAG_KEEP_LOG))
				{
					log_printf("clear old search log (SIZE:%ld->0)", size);
					ftruncate(cc, 0);
				}
			}
		}
		FLOCK_UN(cc);
		close(cc);

		if (bytes > 0)
		{
			char *ptr, *str, *qtr;
			int wdf;
			std::map<string, int> lines;
			std::map<string, int>::iterator it;

			log_verbose("using std::map to parse log data (BYTES:%ld)", bytes);
			for (str = buf; *str && (ptr = strchr(str, '\n')) != NULL; str = ptr + 1)
			{
				if (ptr == str)
					continue;
				wdf = 1;
				*ptr = '\0';
				if ((qtr = strchr(str, '\t')) != NULL)
				{
					*qtr++ = '\0';
					wdf = atoi(qtr);
				}

				fix_log_words(str);
				if (*str != '\0')
				{
					string line(str);
					lines[line] = lines[line] + wdf;
				}
			}

			log_normal("log words loaded (COUNT:%d)", lines.size());
			for (it = lines.begin(); it != lines.end(); it++)
			{
				add_log_words(it->first, it->second);
			}
		}
		if (buf != NULL)
			free(buf);
	}

	// ok to finish
	database.commit();
	flag |= FLAG_FINISHED;
	log_force("all finished (ADD:%d, UPDATE:%d, FAILED:%d, DB_TOTAL:%d)",
		total_add, total_update, total_fail, database.get_doccount());

	// end, free resources
main_end:
	py_dict_unload();
	if (xdict != NULL)
		xdict_close(xdict);
	if (flag & FLAG_FREE_DBPATH)
		free(dbpath);
	if (flag & FLAG_FREE_LOGFILE)
		free(logfile);
	log_close();
	exit((flag & FLAG_FINISHED) ? 0 : -1);
}

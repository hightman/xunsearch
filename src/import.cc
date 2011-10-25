/**
 * Import exchangeable index data into Xapian::WritableDatabase
 * Read the data from a binary file or <stdin> directly, and without any header comparation or version check.
 *
 * $Id: $
 */

#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <xapian.h>
#include <xapian/unicode.h>

#include "flock.h"
#include "pcntl.h"
#include "log.h"
#include "xs_cmd.h"
#include "import.h"
#include "global.h"

/* global flag settings */
#define	FLAG_CORRECTION		0x01
#define	FLAG_QUIET			0x02
#define	FLAG_VERBOSE		0x04
#define	FLAG_TRANSACTION	0x08
#define	FLAG_FORCE			0x10
#define	FLAG_COMMITTING		0x100	// committing...
#define	FLAG_TERMINATED		0x200	// terminated after comitting
#define	FLAG_STDIN			0x400	// read data from stdin <need not lock>
#define	FLAG_HEADER			0x800	// file has import header
#define	FLAG_HEADER_ONLY	0x1000	// only show the header
#define	FLAG_ARCHIVE		0x2000	// there is archive database
#define	FLAG_DEFAULT_DB		0x4000	// is the dbname equal to db

/* fetch result type */
#define	FETCH_ABORT			-1		// error break
#define	FETCH_DIRTY			0		// invalid cmd
#define	FETCH_ADD			1		// add
#define	FETCH_UPDATE		2		// update
#define	FETCH_DELETE		3		// delete
#define	FETCH_SKIP			4		// number limit

/* local global variables */
static char *prog_name;
static int flag, fd, num_skip, bytes_read;
static int total, total_update, total_delete, total_add, archive_delete;

static Xapian::WritableDatabase database, archive;
static Xapian::TermGenerator indexer;
static Xapian::Stem stemmer;
static Xapian::SimpleStopper stopper;
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
	printf("%s: %s/%s (data import)\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	exit(0);
}

/**
 * Usage help
 */
static void show_usage()
{
	printf("%s (%s/%s) - Index Data Importer\n", prog_name, PACKAGE_NAME, PACKAGE_VERSION);
	printf("Copyright (C)2007-2011 hightman, HangZhou YunSheng Network Co., Ltd.\n\n");

	printf("Usage: %s [options] [DB_dir] [Input_File]\n", prog_name);
	printf("  -H               Display header of input file only\n");
	printf("  -N               Do not use transaction\n");
	printf("  -Q               Completely quiet mode, not output any information\n");
	printf("  -S               Enable saving information for spelling correction\n");
	printf("  -V               Verbose mode, show insert/update message for each document\n");
	printf("  -d <DB>          Specify the path to the writable database\n");
	printf("  -f <file>        Specify the path to input exchangeable file(binary)\n");
	printf("  -k <num>         Set the number of documents to be skipped\n");
	printf("                   Default: read from file header (CMD_IMPORT_HEADER)\n");
	printf("  -l <num>         Set the maximum documents to be imported(Not include skipped)\n");
	printf("  -m <multi level> Set the multi level to libSCWS, range is from 1 to 15, (default: %d)\n",
		DEFAULT_SCWS_MULTI);
	printf("                   1|2|4|8 = short|duality|zmain|zall, see scws documents pl.\n");
	printf("  -n <num>         Set the batch number of each transaction or progress report\n");
	printf("                   Default: %d\n", DEFAULT_COMMIT_NUMBER);
	printf("  -s <stopfile>    Specify the path to stop words list\n");
	printf("                   Default: none, refer to etc/stopwords.txt under install directory\n");
	printf("  -t <stemmer>     Specify the stemmer language, (default: " DEFAULT_STEMMER ")\n");
	printf("  -z <size>MB      Set the size of read data for each transaction, (default: %dMB)\n",
		DEFAULT_COMMIT_SIZE);
	printf("  -v               Show version information\n");
	printf("  -h               Display this help page\n\n");
	printf("The control signal during running:\n");
	printf("  Ctrl-C           Stop gracefully\n");
	printf("  Ctrl-\\           Terminate immediately\n");
	printf("  Ctrl-Z           Display progress report\n\n");
	printf("Compiled with xapian-core-scws-" XAPIAN_VERSION "\n");
	printf("Report bugs to " PACKAGE_BUGREPORT "\n");
	exit(0);
}

/**
 * inline function to commit transaction or flush data
 */
static inline void batch_committed(int reopen)
{
	flag |= FLAG_COMMITTING;
	try
	{
		if (reopen == 0 && archive_delete > 0)
			archive.commit();
		if (flag & FLAG_TRANSACTION)
		{
			database.commit_transaction();
			if (reopen)
				database.begin_transaction();
		}
		else
			database.commit();

		// save new number of skip
		if ((flag & FLAG_HEADER) && total > num_skip)
		{
			off_t off = lseek(fd, 0, SEEK_CUR);
			lseek(fd, sizeof(XS_CMD) + offsetof(struct xs_import_hdr, proc_num), SEEK_SET);
			write(fd, &total, sizeof(total));
			lseek(fd, off, SEEK_SET);
		}
	}
	catch (const Xapian::Error &e)
	{
		log_force("xapian ERROR: %s", e.get_msg().data());
	}
	catch (...)
	{
	}
	flag ^= FLAG_COMMITTING;
}

/**
 * Signal handlers should be compiled in C-style
 */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Termination handler
 */
int signal_term(int sig)
{
	log_force("signal[%d] received, try to save uncommitted data", sig);

	if (flag & FLAG_COMMITTING)
	{
		flag |= FLAG_TERMINATED;
		log_normal("signal received during transaction committing or flushing, wait for the end");
		return SIGNAL_TERM_LATER;
	}
	else
	{
		batch_committed(0);
		log_force("interrupted (ADD:%d, UPDATE:%d, DELETE:%d[%d], PROC_TOTAL:%d, DB_TOTAL:%d)",
			total_add, total_update, total_delete, archive_delete, total, database.get_doccount());

		if (!(flag & FLAG_STDIN))
		{
			FLOCK_UN(fd);
			close(fd);
		}
		return -1;
	}
}

/**
 * Child process reaper
 */
void signal_child(pid_t pid, int status)
{
	log_normal("child process[%d] exit with status[%d], but where the child process came out?",
		pid, status);
}

/**
 * Shutdown gracefully
 */
void signal_int()
{
	log_force("sigint received, set the flag to shutdown gracefully");
	flag |= FLAG_TERMINATED;
}

/**
 * Reload handler
 */
void signal_reload(int sig)
{
	if (sig == SIGTSTP)
	{
		// The signal came from keyboard(^Z)
		// Although printf is not signal safe, but we do not use in other place
		printf("importing progress (ADD:%d, UPDATE:%d, DELETE:%d[%d], PROC_TOTAL:%d, DB_TOTAL:%d)\n",
			total_add, total_update, total_delete, archive_delete, total, database.get_doccount());
	}
	else
	{
		log_force("importing progress (ADD:%d, UPDATE:%d, DELETE:%d[%d], PROC_TOTAL:%d, DB_TOTAL:%d)",
			total_add, total_update, total_delete, archive_delete, total, database.get_doccount());
	}
}

#ifdef __cplusplus
}
#endif

/**
 * Unicode range for CJK characters
 */
#define	UNICODE_CJK(x)	(((x)>=0x2e80 && (x)<=0x2eff)				\
    || ((x)>=0x3000 && (x)<=0xa71f)	|| ((x)>=0xac00 && (x)<=0xd7af)	\
    || ((x)>=0xf900 && (x)<=0xfaff)	|| ((x)>=0xff00 && (x)<=0xffef)	\
    || ((x)>=0x20000 && (x)<=0x2a6df) || ((x)>=0x2f800 && (x)<=0x2fa1f))

/**
 * check requirement of stemmer
 * @param term
 * @return 
 */
static inline bool should_stem(const string &term)
{
	const unsigned int SHOULD_STEM_MASK =
		(1 << Xapian::Unicode::LOWERCASE_LETTER) |
		(1 << Xapian::Unicode::TITLECASE_LETTER) |
		(1 << Xapian::Unicode::MODIFIER_LETTER) |
		(1 << Xapian::Unicode::OTHER_LETTER);
	Xapian::Utf8Iterator u(term);
	bool should = (!UNICODE_CJK(*u) && ((SHOULD_STEM_MASK >> Xapian::Unicode::get_category(*u)) & 1));
	return should;
}

/**
 * read input data from fd(maybe <stdin>)
 * @return integer Upon successful completion, returns zero. Otherwise, -1 is returned.
 */
static int data_read(void *buf, int len)
{
	int n, retry = 5;
	bytes_read += len; // Add byte first, len maybe changed later
	do
	{
		n = read(fd, buf, len);
		if (n >= len) break;
		if (n == 0)
		{
			log_normal("reached the end of file (EXPECTED:%d, ACTUAL:0)", len);
			return -1;
		}
		else if (n > 0)
		{
			buf = (char *) buf + n;
			len -= n;
			continue;
		}
		else if (errno != EINTR && errno != EAGAIN)
		{
			retry = 0;
			break;
		}
		usleep(20000);
	}
	while (--retry);

	if (retry == 0)
	{
		log_normal("error occurred while reading data (ERRNO:%d, ERROR:%s)", errno, strerror(errno));
		return -1;
	}

	return 0;
}

/**
 * Read header of import file
 * @return 0 on success, -1 on error
 */
static int header_read(struct xs_import_hdr *hdr)
{
	XS_CMD cmd;

	if (flag & FLAG_STDIN)
		return 0;

	if (data_read(&cmd, sizeof(cmd)) < 0)
		return -1;

	if (cmd.cmd != CMD_IMPORT_HEADER)
		lseek(fd, 0, SEEK_SET);
	else
	{
		char *buf;
		int size = XS_CMD_BUFSIZE(&cmd);

		buf = (char *) malloc(size);
		if (buf == NULL)
		{
			log_normal("unable to allocate memory for header buffer, (CMD:%d, BUFSIZE:%d)", cmd.cmd, size);
			return -1;
		}
		if (data_read(buf, size) < 0)
			return -1;

		if (size > sizeof(struct xs_import_hdr))
			size = sizeof(struct xs_import_hdr);
		memcpy(hdr, buf, size);
		free(buf);

		flag |= FLAG_HEADER;
		log_normal("got import header (PROC_NUM:%d, EFF_SIZE:%lld)", hdr->proc_num, hdr->eff_size);
	}
	return 0;
}

/**
 * Fetch a document from opened file description.
 * @return integer fetch result type
 */
static int doc_fetch()
{
	int rc, size, lsize;
	char prefix[3], *buf, *term;
	Xapian::Document doc;
	XS_CMD cmd;

	// read the first cmd header
	if (data_read(&cmd, sizeof(cmd)) < 0)
		return FETCH_ABORT;

	// check special CMD (CMD_INDEX_EXDATA) just skip, need not read the buffer
	if (cmd.cmd == CMD_INDEX_EXDATA)
		return FETCH_DIRTY;

	size = XS_CMD_BUFSIZE(&cmd);
	// just skip the import header
	if (cmd.cmd == CMD_IMPORT_HEADER)
	{
		struct xs_import_hdr *hdr;

		buf = (char *) malloc(size);
		if (buf == NULL)
		{
			log_normal("unable to allocate memory for header buffer, (CMD:%d, BUFSIZE:%d)", cmd.cmd, size);
			return FETCH_ABORT;
		}
		if (data_read(buf, size) < 0)
		{
			free(buf);
			return FETCH_ABORT;
		}

		hdr = (struct xs_import_hdr *) buf;
		log_normal("got unused import header (PROC_NUM:%d, EFF_SIZE:%lld)", hdr->proc_num, hdr->eff_size);
		free(buf);

		return FETCH_DIRTY;
	}

	// NOTE: check valid CMD
	if (cmd.cmd != CMD_INDEX_REQUEST && cmd.cmd != CMD_INDEX_REMOVE)
	{
		log_normal("got invalid command, abort (CMD:%d, BUFSIZE:%d)", cmd.cmd, XS_CMD_BUFSIZE(&cmd));
		return FETCH_ABORT;
	}

	// read cmd buffer? (try to get the vno from arg2)
	buf = term = NULL;
	if (size > 0)
	{
		term = (char *) malloc(size + sizeof(prefix));
		if (term == NULL)
		{
			log_normal("unable to allocate memory for request command (CMD:%d, BUFSIZE:%d)", cmd.cmd, size);
			return FETCH_ABORT;
		}
		vno_to_prefix(cmd.arg2, term);
		buf = term + strlen(term);
		if (data_read(buf, size) < 0)
		{
			free(term);
			return FETCH_ABORT;
		}
		buf[size] = '\0';
	}

	// set rc & size, used for: doc_end label
	rc = total < num_skip ? FETCH_SKIP : FETCH_DIRTY;
	size = lsize = 0;
	buf = NULL;

	// check the remove cmd
	if (cmd.cmd == CMD_INDEX_REMOVE && term != NULL)
	{
		if (rc == FETCH_SKIP)
		{
			log_verbose("~skip to remove the document (ID:%s, SKIP_LEFT:%d)", term, num_skip - total - 1);
		}
		else
		{
			rc = FETCH_DELETE;
			if ((flag & FLAG_ARCHIVE) && archive.term_exists(term))
			{
				archive_delete++;
				archive.delete_document(term);
				log_verbose("--remove the documnt from archive (ID:%s, ARCHIVE_DELETE:%d)", term, archive_delete);
			}
			else
			{
				total_delete++;
				database.delete_document(term);
				log_verbose("-remove the document (ID:%s, TOTAL_DELETE:%d)", term, total_delete);
			}
		}
		goto doc_end;
	}

	// other case, only CMD_INDEX_REQUEST was accepted
	if (cmd.cmd != CMD_INDEX_REQUEST)
		goto doc_end;

	if (rc != FETCH_SKIP)
		rc = (cmd.arg1 == CMD_INDEX_REQUEST_UPDATE && term != NULL) ? FETCH_UPDATE : FETCH_ADD;

	// create the document & wait a submit command
	indexer.set_document(doc);
	do
	{
		// read the doc cmd
		if (data_read(&cmd, sizeof(cmd)) < 0)
		{
			rc = FETCH_ABORT;
			goto doc_end;
		}

		// read buffer data assert(size == cmd.blen)
		size = XS_CMD_BUFSIZE(&cmd);
		if (size > 0)
		{
			if (size > lsize)
			{
				buf = (char *) realloc(buf, size + 1);
				if (buf == NULL)
				{
					rc = FETCH_ABORT;
					log_normal("unable to allocate memory for doc command (CMD:%d, BUFSIZE:%d)", cmd.cmd, size);
					goto doc_end;
				}
				lsize = size;
			}
			if (data_read(buf, size) < 0)
			{
				rc = FETCH_ABORT;
				goto doc_end;
			}
			buf[size] = '\0';
		}

		// parse the cmd, assert(size==cmd.blen)?
		switch (cmd.cmd)
		{
			case CMD_DOC_TERM:
				// arg1:wdf|flag, arg2:vno, blen:term_len, buf:term
				if (rc == FETCH_SKIP)
					break;
				// empty term cause to increase termpos
				if (size == 0)
					indexer.increase_termpos();
				else
				{
					string tt(buf, size);
					string pp("");
					if (CMD_INDEX_VALUENO(cmd) != XS_DATA_VNO)
					{
						vno_to_prefix(CMD_INDEX_VALUENO(cmd), prefix);
						pp = string(prefix);
					}
					if (!CMD_INDEX_WITHPOS(cmd))
						doc.add_term(pp + tt, CMD_INDEX_WEIGHT(cmd));
					else
					{
						// adding with position information
						doc.add_posting(pp + tt, indexer.get_termpos() + 1, CMD_INDEX_WEIGHT(cmd));
						indexer.increase_termpos(1);
					}
					// check stemmer
					if (CMD_INDEX_CHECK_STEM(cmd) && should_stem(tt))
					{
						string ss("Z");
						ss += pp + stemmer(tt);
						doc.add_term(ss, CMD_INDEX_WEIGHT(cmd));
					}
				}
				break;
			case CMD_DOC_VALUE:
				// arg1:flag(numeric=0x80), arg2:vno, blen:content_len, buf:content
				if (rc != FETCH_SKIP && size > 0)
				{
					if (CMD_INDEX_VALUENO(cmd) == XS_DATA_VNO)
						doc.set_data(buf);
					else
					{
						if (!CMD_VALUE_NUMERIC(cmd))
							doc.add_value(CMD_INDEX_VALUENO(cmd), buf);
						else
						{
							string enc = Xapian::sortable_serialise(strtod(buf, NULL));
							doc.add_value(CMD_INDEX_VALUENO(cmd), enc);
						}
					}
				}
				// save first value as ID term for logging
				if (term == NULL)
					term = strdup(buf);
				break;
			case CMD_DOC_INDEX:
				// arg1:weight|flag, arg2:vno, blen:content_len, buf:content
				// weight: 0~63, flag_64:withpos, flag_128:save value also
				if (rc != FETCH_SKIP && size > 0)
				{
					// index text
					vno_to_prefix(CMD_INDEX_VALUENO(cmd), prefix);
					if (!CMD_INDEX_WITHPOS(cmd))
						indexer.index_text_without_positions(buf, CMD_INDEX_WEIGHT(cmd), prefix);
					else
					{
						indexer.index_text(buf, CMD_INDEX_WEIGHT(cmd), prefix);
						indexer.increase_termpos();
					}
					// add value (numeric not supportted)
					if (CMD_INDEX_SAVE_VALUE(cmd))
					{
						if (CMD_INDEX_VALUENO(cmd) == XS_DATA_VNO)
							doc.set_data(buf);
						else
							doc.add_value(CMD_INDEX_VALUENO(cmd), buf);
					}
				}
				break;
		}
	}
	while (cmd.cmd != CMD_INDEX_SUBMIT);

	// submit it
	if (rc == FETCH_ADD)
	{
		total_add++;
		database.add_document(doc);
		log_verbose("+add the document (ID:%s, TOTAL_ADD:%d)", term == NULL ? "NULL" : term, total_add);
	}
	else if (rc == FETCH_UPDATE)
	{
		if ((flag & FLAG_ARCHIVE) && archive.term_exists(term))
		{
			archive_delete++;
			archive.delete_document(term);
			log_verbose("--remove the documnt from archive (ID:%s, ARCHIVE_DELETE:%d)", term, archive_delete);
		}
		total_update++;
		database.replace_document(term, doc);
		log_verbose("!update the document (ID:%s, TOTAL_UPDATE:%d)", term == NULL ? "NULL" : term, total_update);
	}
	else
	{
		log_verbose("~skip to update/add the document (ID:%s, SKIP_LEFT:%d)",
			term == NULL ? "NULL" : term, num_skip - total - 1);
	}

doc_end:
	if (buf != NULL) free(buf);
	if (term != NULL) free(term);

	return rc;
}

/**
 * Main function(entrance)
 * @param argc
 * @param argv
 */
int main(int argc, char *argv[])
{
	int num_commit, num_limit, multi, size_limit;
	time_t t_begin;
	char *db_path, *fpath;
	struct xs_import_hdr hdr;

	// init variables
	db_path = fpath = NULL;
	flag = FLAG_TRANSACTION;
	num_limit = bytes_read = 0;
	num_skip = -1;
	size_limit = DEFAULT_COMMIT_SIZE * 1048576;
	num_commit = DEFAULT_COMMIT_NUMBER;
	multi = DEFAULT_SCWS_MULTI;
	stemmer = Xapian::Stem(DEFAULT_STEMMER);

	// parse the arguments
	if ((prog_name = strrchr(argv[0], '/')) != NULL) prog_name++;
	else prog_name = argv[0];

	while ((fd = getopt(argc, argv, "vhHNQSVd:f:k:l:m:n:s:t:z:")) != -1)
	{
		switch (fd)
		{
			case 'H': flag |= FLAG_HEADER_ONLY;
				break;
			case 'N': flag &= ~FLAG_TRANSACTION;
				break;
			case 'Q': flag |= FLAG_QUIET;
				break;
			case 'S': flag |= FLAG_CORRECTION;
				break;
			case 'V': flag |= FLAG_VERBOSE;
				break;
			case 'd': db_path = optarg;
				break;
			case 'f': fpath = optarg;
				break;
			case 'k': num_skip = atoi(optarg);
				break;
			case 'l': num_limit = atoi(optarg);
				break;
			case 'm': multi = atoi(optarg) & 0x0f;
				break;
			case 'n':
				num_commit = atoi(optarg);
				if (num_commit <= 0)
					num_commit = DEFAULT_COMMIT_NUMBER;
				break;
			case 't':
				if (strcasecmp(optarg, DEFAULT_STEMMER))
				{
					try
					{
						stemmer = Xapian::Stem(optarg);
					}
					catch (...)
					{
						fprintf(stderr, "ERROR: stemmer language isn't recognised (LANG:%s)\n", optarg);
						goto main_end;
					}
				}
				break;
			case 's':
			{
				FILE *fp;
				char buf[64], *ptr;

				if ((fp = fopen(optarg, "r")) == NULL)
					fprintf(stderr, "WARNING: stopwords file not found (FILE:%s)\n", optarg);
				else
				{
					buf[sizeof(buf) - 1] = '\0';
					while (fgets(buf, sizeof(buf) - 1, fp) != NULL)
					{
						if (buf[0] == ';' || buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n')
							continue;
						ptr = buf + strlen(buf) - 1;
						while (ptr > buf && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')) ptr--;
						ptr[1] = '\0';
						ptr = buf;
						if (*ptr == '\0') continue;
						while (*ptr == ' ' || *ptr == '\t') ptr++;
						stopper.add(ptr);
					}
					fclose(fp);
				}
			}
				break;
			case 'z':
				size_limit = atoi(optarg);
				if (size_limit < 0 && size_limit > 1024)
					size_limit = DEFAULT_COMMIT_SIZE;
				size_limit <<= 20;
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

	// other arguments [db] [file]
	if (flag & FLAG_QUIET) flag &= ~FLAG_VERBOSE;
	argc -= optind;
	if (argc > 0 && db_path == NULL)
	{
		db_path = argv[optind++];
		argc--;
	}
	if (argc > 0 && fpath == NULL)
	{
		fpath = argv[optind++];
		argc--;
	}

	// open log to stderr
	log_open("stderr", "import");

	// check and open the database
	if (db_path == NULL && !(flag & FLAG_HEADER_ONLY))
	{
		fprintf(stderr, "ERROR: you should specify the database using `-d' option\n");
		goto main_end;
	}
	// check the input file(failed? redirect to <STDIN>
	if (fpath == NULL)
	{
		fprintf(stderr, "WARNING: read from STDIN, you may specify the input file using `-f' option\n");
		fd = STDIN_FILENO;
		flag |= FLAG_STDIN;
	}
	else if ((fd = open(fpath, O_RDWR)) < 0 || !FLOCK_WR_NB(fd))
	{
		log_force("failed to open/lock the input file (FILE:%s, ERROR:%s)",
			fpath, strerror(errno));

		flag |= FLAG_TERMINATED;
		goto main_end;
	}

	// set the env: XAPIAN_FLUSH_THRESHOLD=10000
	if (num_commit != 10000)
	{
		char envbuf[64];
		sprintf(envbuf, "XAPIAN_FLUSH_THRESHOLD=%d", num_commit);
		putenv(envbuf);
	}

	// install basic signal handler
	pcntl_base_signal();

	// read header
	if (header_read(&hdr) < 0)
	{
		flag |= FLAG_TERMINATED;
		log_force("unable to read import file header");
		goto main_end;
	}

	// just show file header
	if (flag & FLAG_HEADER_ONLY)
	{
		printf("Import header of `%s': ", fpath == NULL ? "<STDIN>" : fpath);
		if (flag & FLAG_STDIN)
			printf("** Not supported **\n");
		else if (flag & FLAG_HEADER)
			printf("{ proc_num:%d, eff_size:%lld, ... }\n", hdr.proc_num, hdr.eff_size);
		else
			printf("** Not Found **\n");
		goto main_end;
	}

	// reset num_skip & filesize
	if (num_skip < 0)
		num_skip = (flag & FLAG_HEADER) ? hdr.proc_num : 0;
	if (flag & FLAG_HEADER)
	{
		struct stat st;
		if (!fstat(fd, &st) && st.st_size > hdr.eff_size)
		{
			ftruncate(fd, hdr.eff_size);
			log_normal("reset file size (ST_SIZE:%lld,EFF_SIZE:%lld)", st.st_size, hdr.eff_size);
		}
	}

	// try open the database
	total = total_add = total_update = total_delete = 0;
	try
	{
		char *ptr;
		if ((ptr = strchr(db_path, ':')) == NULL)
			database = Xapian::WritableDatabase(db_path, Xapian::DB_CREATE_OR_OPEN);
		else
		{
			*ptr++ = '\0';
			database = Xapian::Remote::open_writable(db_path, atoi(ptr), 5000, 1000);
		}

		indexer.set_stemmer(stemmer);
		indexer.set_stopper(&stopper);
		indexer.set_database(database);
		indexer.load_libscws(NULL, true, multi);

		if (flag & FLAG_CORRECTION)
			indexer.set_flags(Xapian::TermGenerator::FLAG_SPELLING);

	}
	catch (const Xapian::Error &e)
	{
		log_force("Xapian ERROR: %s", e.get_msg().data());
		flag |= FLAG_TERMINATED;
		goto main_end;
	}
	catch (...)
	{
		log_force("ERROR: failed to initalize the database and indexer object (DB:%s)", db_path);
		flag |= FLAG_TERMINATED;
		goto main_end;
	}

	// try to open the archive database
	archive_delete = 0;
	try
	{
		char *ptr = strrchr(db_path, '/');
		if (ptr == NULL && !strcasecmp(db_path, DEFAULT_DB_NAME))
		{
			flag |= FLAG_DEFAULT_DB;
			archive = Xapian::WritableDatabase(DEFAULT_DB_NAME "_a", Xapian::DB_OPEN);
			flag |= FLAG_ARCHIVE;
		}
		else if (ptr != NULL && !strcasecmp(ptr + 1, DEFAULT_DB_NAME))
		{
			char dba_path[256];

			flag |= FLAG_DEFAULT_DB;
			snprintf(dba_path, sizeof(dba_path), "%.*s/" DEFAULT_DB_NAME "_a", (int) (ptr - db_path), db_path);
			archive = Xapian::WritableDatabase(dba_path, Xapian::DB_OPEN);
			flag |= FLAG_ARCHIVE;
		}
	}
	catch (...)
	{
	}

	// read the file & count them
	t_begin = time(NULL);
	log_normal("begin importing (NUM_BATCH:%d, SIZE_LIMIT:%dMB, DB_TOTAL:%d, NUM_SKIP:%d)",
		num_commit, size_limit >> 20, database.get_doccount(), num_skip);

	if (flag & FLAG_TRANSACTION) database.begin_transaction();
	while (!(flag & FLAG_TERMINATED) && ((argc = doc_fetch()) != FETCH_ABORT))
	{
		if (argc != FETCH_DIRTY) total++;
		if (num_limit > 0 && (total - num_skip) == num_limit)
		{
			log_normal("number of imported documents has reached the limitation (LIMIT:%d)", num_limit);
			break;
		}
		else if (total > 0 && ((total % num_commit) == 0 || bytes_read > size_limit))
		{
			if (total > num_skip)
				batch_committed(1);

			argc = time(NULL) - t_begin;
			log_normal("%s progress (PROC_TOTAL:%d, NUM_BATCH:%d, SIZE_READ:%.2fMB, TIME:%d'%02d\")",
				total > num_skip ? "committed" : "skipped", total, num_commit,
				(double) bytes_read / 1048576, argc / 60, argc % 60);

			bytes_read = 0;
		}
	}

	// last committed
	batch_committed(0);

	// finished report
	argc = time(NULL) - t_begin;
	log_force("%s (ADD:%d, UPDATE:%d, DELETE:%d[%d], PROC_TOTAL:%d, DB_TOTAL:%d, TIME:%d'%02d\")",
		(flag & FLAG_TERMINATED ? "terminated" : "finished"),
		total_add, total_update, total_delete, archive_delete, total,
		database.get_doccount(), argc / 60, argc % 60);

	// check to archive
	if ((flag & FLAG_DEFAULT_DB) && (database.get_doccount() >= DEFAULT_ARCHIVE_THRESHOLD))
	{
		char cmd_buf[256];
		char *ptr = strrchr(db_path, '/');

		log_force("compacting current database into archive (DB:%s, TOTAL:%d)", db_path, database.get_doccount());
		if (ptr != NULL)
		{
			ptr[1] = '\0';
			chdir(db_path);
		}

		database.close();
		if (flag & FLAG_ARCHIVE)
		{
			archive.close();

			// 1. merge: db + db_a -> db_b			
			system(XAPIAN_DIR "/bin/xapian-compact " DEFAULT_DB_NAME " " DEFAULT_DB_NAME "_a " DEFAULT_DB_NAME "_c");
			// 2. remove: db_o db
			system("/bin/rm -rf " DEFAULT_DB_NAME "_o " DEFAULT_DB_NAME);
			// 3. rename: db_a -> db_o, db_c -> db_a
			system("/bin/mv -f " DEFAULT_DB_NAME "_a " DEFAULT_DB_NAME "_o");
			system("/bin/mv -f " DEFAULT_DB_NAME "_c " DEFAULT_DB_NAME "_a");
		}
		else
		{
			// 1. remove: db_a (clean)
			system("/bin/rm -rf " DEFAULT_DB_NAME "_a");
			// 2. rename: db -> db_a
			system("/bin/mv -f " DEFAULT_DB_NAME " " DEFAULT_DB_NAME "_a");
		}
		
		// re-create empty db
		database = Xapian::WritableDatabase(DEFAULT_DB_NAME, Xapian::DB_CREATE_OR_OPEN);		
	}

main_end:
	if (fd >= 0)
	{
		if (!(flag & FLAG_STDIN))
			FLOCK_UN(fd);
		close(fd);
	}
	log_close();
	exit((flag & FLAG_TERMINATED) ? -1 : 0);
}

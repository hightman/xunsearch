/**
 * Communication protocol & command code for xun-search.
 *
 * $Id: $
 */

#ifndef __XS_CMD_20110517_H__
#define	__XS_CMD_20110517_H__

/**
 * Commander data structure.
 * Total size of command packet: sizeof(XS_CMD) + cmd->blen1 + cmd->blen2
 * arg = (arg1<<8|arg2)
 */
typedef struct xs_cmd
{
	unsigned char cmd; // 1bytes 0~127(request cmd), 128~25(respond, cmd)
	unsigned char arg1; // 1bytes -> arg1  \___(arg1<<8|arg2)=>arg___
	unsigned char arg2; // 1bytes -> arg2  /
	unsigned char blen1; // 1bytes -> (length of buf1, append to buf)
	unsigned int blen; // length for buf data (uint32, machine dependent)
	char buf[0]; // buffer content blen1+blen2 (DO NOT end with \0)
} XS_CMD, xs_cmd_t;

/**
 * TODO: File header for import file (64 bytes reserved)
 * Saved as special command: CMD_IMPORT_HEADER
 */
#ifndef	SIZEOF_OFF_T
#define	SIZEOF_OFF_T	4
#endif

#ifndef offsetof
#define offsetof(type, field) ((long)&((type *)0)->field)
#endif

struct xs_import_hdr
{
	int proc_num; // num for commands processed (used to skip?)
	off_t eff_size; // effective number of commands
	char reserved[60 - SIZEOF_OFF_T];
};

/* Macros to get buffer size of the command */
#define	XS_CMD_BUF(x)		(x)->buf
#define	XS_CMD_BUF1(x)		((x)->buf+(x)->blen)
#define	XS_CMD_BLEN(x)		(x)->blen
#define	XS_CMD_BLEN1(x)		(x)->blen1
#define	XS_CMD_SIZE(x)		(sizeof(struct xs_cmd)+(x)->blen+(x)->blen1)
#define	XS_CMD_BUFSIZE(x)	((x)->blen+(x)->blen1)
#define	XS_CMD_BUFTAIL(x)	((x)->buf+(x)->blen+(x)->blen1)

/* Macros to visit unsigned short argument by merging arg1 and arg2 */
#define	XS_CMD_ARG(x)		(unsigned short)(((x)->arg1<<8)|(x)->arg2)
#define	XS_CMD_SET_ARG(x,a)	do { (x)->arg1=(a>>8)&0xff; (x)->arg2=a&0xff; } while (0)

/* Special value no for data stored */
#define	XS_DATA_VNO			255

/* Special command (none) */
#define	CMD_NONE			0
#define	CMD_DEFAULT			CMD_NONE
#define	CMD_PROTOCOL		20110707

/**
 * ---------------------
 * Public commands: 1~31
 * ---------------------
 */
/**
 * Used to say hello, set the current project name.
 * Respond maybe OK or failure(cann't create project automatically)
 * blen:name_len, blen1:home_len, buf:name, buf1:home
 */
#define	CMD_USE				1
#define	CMD_HELLO			1

/**
 * Get deubg info for current connection
 */
#define	CMD_DEBUG			2

/**
 * Set the timeout for current connection
 * Disconnect automatically, once have not any IO action more than this time (seconds).
 * arg: timeout (unit:sec, 0~65535, 0->unlimit)
 */
#define	CMD_TIMEOUT			3

/**
 * Close the connection by client
 */
#define	CMD_QUIT			4

/**
 * ------------------------------------------------
 * Commands of index server & import command: 32~63
 * All commands can get respond from server.
 * ------------------------------------------------
 */
/**
 * Specifies the name of the database to manage.
 * Database is under home directory of the project, default is: db
 * blen:dbname_len, buf:dbname
 */
#define	CMD_INDEX_SET_DB	32

/**
 * Get information of current database
 */
#define	CMD_INDEX_GET_DB	33

/**
 * Submit an index request, work with CMD_INDEX_REQUEST
 * The command sent after the CMD_INDEX_REQUEST
 * There is only CMD_DOC_***  between them, any other commands were reported as WRONGPLACE
 */
#define	CMD_INDEX_SUBMIT	34

/**
 * Remove document from current database by a term(word).
 * arg2:vno, blen:term_len, buf:term
 */
#define	CMD_INDEX_REMOVE	35

/**
 * Submitting a piece of long exchangeable binary data
 * Data may contain many groups with request+submit+remove command, but ignores any other command.
 * blen:data_len, buf:data
 */
#define	CMD_INDEX_EXDATA	36

/**
 * Clean data from current database(rm -rf db_dir)
 * Forbidden this cmd during rebuild
 */
#define	CMD_INDEX_CLEAN_DB	37

/**
 * Used to remove current project
 */
#define	CMD_DELETE_PROJECT	38

/**
 * Force to commit current database
 */
#define	CMD_INDEX_COMMIT	39

/**
 * Mark current database to rebuild
 * Empty the index queue (.rcv file), back comming data was written to a temporary DB
 * After finishing, clean current DB & rename tempory DB to current DB
 * arg1: 0/1 => begin/end
 */
#define	CMD_INDEX_REBUILD	40

/**
 * Force to flush logging
 */
#define	CMD_FLUSH_LOGGING	41

/**
 * Add/Remove synonyms for a term (null passed to remove all synonyms of the term..)
 * arg1:flag(add|remove)
 * blen:original_term_len, buf:original_term,
 * blen1:synonym_term_len, buf1:synonym_term_len
 */
#define	CMD_INDEX_SYNONYMS	42

/**
 * Get/Set custom dict for a project
 * arg1:flag(get/set)
 * blen:dict_txt_len, buf:dict_txt_content
 */
#define	CMD_INDEX_USER_DICT	43

/**
 * ----------------------------------------
 * Commands of search server: 64~95
 * All commands can get respond from server
 * ----------------------------------------
 */
/**
 * Get the total number of documents in the current database
 */
#define	CMD_SEARCH_DB_TOTAL		64

/**
 * Get the number of matched documents, cache enabled
 * NOTE: If you have to read the search results, use CMD_SEARCH_GET_RESULT instead.
 * arg2:default_op, blen:query_len, buf:query
 */
#define	CMD_SEARCH_GET_TOTAL	65

/**
 * Get matched search results
 * arg2:default_op, blen:query_len, buf:query
 * blen1:8, buf1:int(offset)+int(limit)
 */
#define	CMD_SEARCH_GET_RESULT	66

/**
 * Specifying the name of database to search, and close the specified db first.
 * Default is: db in the project directory
 * If you need to search across mutliple databases, call CMD_SEARCH_ADD_DB plz..
 * NOTE: Returns error when open failed, use stub file to support remote db
 * blen:name_len, buf:name (Name can also be absolute database path).
 */
#define	CMD_SEARCH_SET_DB		CMD_INDEX_SET_DB
#define	CMD_SEARCH_GET_DB		CMD_INDEX_GET_DB

/**
 * Adding the name of database to search
 * Same description as above
 */
#define	CMD_SEARCH_ADD_DB		68

/**
 * Finished the current search request,
 * Leave the worker thread, push the conn back to main thread
 */
#define	CMD_SEARCH_FINISH		69

/**
 * Draw status of thread pool
 */
#define	CMD_SEARCH_DRAW_TPOOL	70

/**
 * Add search log (cleaned)
 * blen:query_len, buf:query, blen1:4/0, buf1:len
 */
#define	CMD_SEARCH_ADD_LOG		71

/**
 * Load and list synonyms for current db
 * arg1:0/1(no exclude Zxxx), blen1:8, buf1:int(offset)+int(limit)
 * arg1:2 (for special term), blen:term_len, buf:term
 */
#define	CMD_SEARCH_GET_SYNONYMS	72

/**
 * scws get operators
 * arg1:op ...
 */
#define	CMD_SEARCH_SCWS_GET		73

/**
 * ----------------------------------------
 * Commands of search query: 96~127
 * All commands can get respond from server
 * ----------------------------------------
 */
/**
 * Get parsed query description string
 * arg2:def_op, blen:query_len, blen1:2/0, buf:query, buf1:(u_short)(scale*100)/null
 * 
 * NOTE: def_op & scale will be ignorned if query string is not specified
 * The byteorder of buf1 is network byte order (big-endian, scale[0]<<8|scale[1])
 */
#define	CMD_QUERY_GET_STRING	96

/**
 * Get parsed terms in query (unstemmed words)
 * Used to highlight search result, all terms joined with space character as a long string.
 * arg1:flag (1:stemmed only),
 * blen:query_len/0, buf:query
 */
#define	CMD_QUERY_GET_TERMS		97

/**
 * Get corrected query string (pinyin + spelling fixed)
 * blen:query_len/0, buf:query
 */
#define	CMD_QUERY_GET_CORRECTED	98

/**
 * Get expanded query string (chinese -> pinyin, wildcard)
 * arg1: limit(default is 10), blen:query_len/0, buf:query
 */
#define	CMD_QUERY_GET_EXPANDED	99

/**
 * -------------------------
 * Respond commands: 128~159
 * -------------------------
 */
/**
 * Operation is successful
 * arg:OK_CODE, blen:message_len, buf:message
 */
#define	CMD_OK					128

/**
 * Operation failed
 * arg:ERR_CODE, blen:error_len, buf:error
 */
#define	CMD_ERR					129

/**
 * Result document start
 * blen:sizeof(struct result_doc)=20, buf:(struct result_doc)
 */
#define	CMD_SEARCH_RESULT_DOC	140

/**
 * Result field for last document
 * arg:vno, blen:content_len, buf:content
 */
#define	CMD_SEARCH_RESULT_FIELD	141

/**
 * Result facets content [vno:1][vlen:1][num:4][value] ...
 * blen:facets_len, buf:content
 */
#define	CMD_SEARCH_RESULT_FACETS	142

/**
 * Result matched terms of document
 * blen:terms_len, buff:terms_string (implode by ' ')
 */
#define	CMD_SEARCH_RESULT_MATCHED	143

/**
 * -----------------------------------------
 * Request commands without respond: 160~255
 * -----------------------------------------
 */
#define	XS_CMD_DONT_ANS(x)	((x)->cmd & 0x80)

/**
 * -------------------------------------------------
 * Commands for index server(& query & doc): 160~191
 * -------------------------------------------------
 */
/**
 * Add a term in the current index request(DOC)
 * arg1:wdf|flag, arg2:vno, blen:term_len, buf:term
 * wdf: 0~63, flag_64:withpos, flag_128:checkstem
 * EMPTY term cause increase_termpos()
 */
#define	CMD_DOC_TERM		160

/**
 * Set value in the current index request(DOC)
 * arg1:flag(numeric=0x80), arg2:vno, blen:content_len, buf:content
 */
#define	CMD_DOC_VALUE		161

/**
 * Index text in the current index request(DOC)
 * arg1:weight|flag, arg2:vno, blen:content_len, buf:content
 * weight: 0~63, flag_64:withpos, flag_128:save value also
 */
#define	CMD_DOC_INDEX		162

/**
 * Start a index request(Open a document to update/add, worked with CMD_INDEX_SUBMIT)
 * arg1:flag(replace|add), arg2:vno(for replace term), blen:term_len/0, buf:term/null
 */
#define	CMD_INDEX_REQUEST	163

/**
 * Header command for exchange file only
 * Use this to record full written data in bytes, to avoid the import data file is incomplete.
 * blen:sizeof(struct import_header), buf:(struct import_header)
 */
#define	CMD_IMPORT_HEADER	191

/**
 * -----------------------------------
 * Commands for search server: 192~223
 * -----------------------------------
 */
/**
 * Set the sort order of search results
 * arg1:sort_type|sort_flag(integer:docid|relevance|value+relevance) (type:0~63,flag_64,flag_128)
 * arg2:vno (when sort type=value+...)
 * buf:[vno][asc]...
 */
#define	CMD_SEARCH_SET_SORT		192

/**
 * Set the truncation length for a special field
 * arg1:cut_len/10|numeric_flag(0~127|flag_128), arg2:vno
 */
#define	CMD_SEARCH_SET_CUT		193

/**
 * Set numeric field, need to unserialize before returning
 * arg2:vno
 */
#define	CMD_SEARCH_SET_NUMERIC	194

/**
 * Set the field based on the search results collapse
 * arg1: collapse_max(default:1), arg2:vno (use XS_DATA_VNO to cancel this feature)
 */
#define	CMD_SEARCH_SET_COLLAPSE	195

/**
 * Do not kill timeoutd threads
 * arg1: 0/1
 */
#define	CMD_SEARCH_KEEPALIVE	196

/**
 * Register value slot for facets searching
 * arg1:0/1(exact or not), blen: field number, buf: vno list
 */
#define	CMD_SEARCH_SET_FACETS	197

/**
 * scws set operators
 * arg1:op, arg2:dict_mode, buf: dict_path
 */
#define	CMD_SEARCH_SCWS_SET		198

/**
 * Set the percentage and/or weight cutoffs
 * arg1:percent_off(0-100), arg2:weight_off(0.1-25.5)
 */
#define	CMD_SEARCH_SET_CUTOFF	199

/**
 * Set misc options of search
 * arg1:type(1:syn_scale|2:matched_term)
 * arg2:scale*10|0/1|
 */
#define	CMD_SEARCH_SET_MISC		200

/**
 * ----------------------------------
 * Commands for search query: 224~255
 * ----------------------------------
 */
/**
 * Initlize the current query for enquire object
 * arg1: 0/1 (reset queryparser or not)
 */
#define	CMD_QUERY_INIT		224

/**
 * Add a sub-query in the right side of the current query
 * arg1:add_op, others: @see CMD_QUERY_GET_STRING
 */
#define	CMD_QUERY_PARSE		225

/**
 * Add a term in the right side of the current query
 * arg1:add_op, arg2:vno, blen:term_len, blen1:2/0, buf:term, buf1:(u_short)(scale*100)
 */
#define	CMD_QUERY_TERM		226
#define	CMD_QUERY_TERMS		232	// multi terms, join with '\t'

/**
 * Register range processor
 * arg1:range_type, arg2:vno
 */
#define	CMD_QUERY_RANGEPROC	227

/**
 * Add range filter
 * arg1:add_op, arg2:vno, blen:from_len, blen1:to_len, buf:from, buf1:to
 */
#define	CMD_QUERY_RANGE		228

/**
 * Add value comparation
 * arg1:add_op, arg2:vno, blen:value_len, blen1:1/0, buf:value, buf1:(u_char)<cmp_type:GE|LE>
 */
#define	CMD_QUERY_VALCMP	229

/**
 * Register field prefix
 * arg1:type(bool|normal), arg2:vno, blen:field_len, buf:field_name
 */
#define	CMD_QUERY_PREFIX	230

/**
 * Set flag for parsing query
 * arg:(u_short)flag
 */
#define	CMD_QUERY_PARSEFLAG	231

/**
 * ----------------------------------
 * Constant defined from Xapian
 * ----------------------------------
 */
// 1. sort order
#define	CMD_SORT_TYPE_RELEVANCE		0
#define	CMD_SORT_TYPE_DOCID			1
#define	CMD_SORT_TYPE_VALUE			2
#define	CMD_SORT_TYPE_MULTI			3	// mutli fields sort
#define	CMD_SORT_TYPE_GEODIST		4	// sort by geo distance
#define	CMD_SORT_TYPE_MASK			0x3f
#define	CMD_SORT_FLAG_RELEVANCE		0x40
#define	CMD_SORT_FLAG_ASCENDING		0x80

// 2. query_op (defined as a global static array in task.cc)
#define	CMD_QUERY_OP_AND			0
#define	CMD_QUERY_OP_OR				1
#define	CMD_QUERY_OP_AND_NOT		2
#define	CMD_QUERY_OP_XOR			3
#define	CMD_QUERY_OP_AND_MAYBE		4
#define	CMD_QUERY_OP_FILTER			5

// 3. range processor type
#define	CMD_RANGE_PROC_STRING		0
#define	CMD_RANGE_PROC_DATE			1
#define	CMD_RANGE_PROC_NUMBER		2

// 4. value comparation type
#define	CMD_VALCMP_LE				0
#define	CMD_VALCMP_GE				1

// 5. query parse flag (must compatiable with Xapian)
#define	CMD_PARSE_FLAG_BOOLEAN						1
#define	CMD_PARSE_FLAG_PHRASE						2
#define	CMD_PARSE_FLAG_LOVEHATE						4
#define	CMD_PARSE_FLAG_BOOLEAN_ANY_CASE				8 
#define	CMD_PARSE_FLAG_WILDCARD						16
#define	CMD_PARSE_FLAG_PURE_NOT						32
#define	CMD_PARSE_FLAG_PARTIAL						64
#define	CMD_PARSE_FLAG_SPELLING_CORRECTION			128
#define	CMD_PARSE_FLAG_SYNONYM						256
#define	CMD_PARSE_FLAG_AUTO_SYNONYMS				512
#define	CMD_PARSE_FLAG_AUTO_MULTIWORD_SYNONYMS		1536

// 6. prefix type
#define	CMD_PREFIX_NORMAL			0
#define	CMD_PREFIX_BOOLEAN			1

// 7. index flag
#define	CMD_INDEX_WEIGHT_MASK		0x3f	// 0~63
#define	CMD_INDEX_FLAG_WITHPOS		0x40
#define	CMD_INDEX_FLAG_SAVEVALUE	0x80	// for cmd_index_text
#define	CMD_INDEX_FLAG_CHECKSTEM	0x80	// for cmd_doc_term
#define	CMD_INDEX_WEIGHT(c)			((c).arg1 & CMD_INDEX_WEIGHT_MASK)
#define	CMD_INDEX_WITHPOS(c)		((c).arg1 & CMD_INDEX_FLAG_WITHPOS)
#define	CMD_INDEX_SAVE_VALUE(c)		((c).arg1 & CMD_INDEX_FLAG_SAVEVALUE)
#define	CMD_INDEX_CHECK_STEM(c)		((c).arg1 & CMD_INDEX_FLAG_CHECKSTEM)
#define	CMD_INDEX_VALUENO(c)		c.arg2	// value no

// 8. special field flag
#define	CMD_VALUE_FLAG_NUMERIC		0x80	// CMD_SEARCH_SET_CUT, CMD_DOC_VALUE
#define	CMD_VALUE_NUMERIC(c)		((c).arg1 & CMD_VALUE_FLAG_NUMERIC)

// 9. request type
#define	CMD_INDEX_REQUEST_ADD		0
#define	CMD_INDEX_REQUEST_UPDATE	1

// 10. synonyms op
#define	CMD_INDEX_SYNONYMS_ADD		0
#define	CMD_INDEX_SYNONYMS_DEL		1

// 11. search misc options
#define	CMD_SEARCH_MISC_SYN_SCALE		1
#define	CMD_SEARCH_MISC_MATCHED_TERM	2

/**
 * ----------------------------------
 * Constant defined for scws set/get
 * ----------------------------------
 */
#define	CMD_SCWS_GET_VERSION	1
#define	CMD_SCWS_GET_RESULT		2
#define	CMD_SCWS_GET_TOPS		3
#define	CMD_SCWS_HAS_WORD		4
#define	CMD_SCWS_GET_MULTI		5

#define	CMD_SCWS_SET_IGNORE		50
#define	CMD_SCWS_SET_MULTI		51
#define	CMD_SCWS_SET_DUALITY	52
#define	CMD_SCWS_SET_DICT		53
#define	CMD_SCWS_ADD_DICT		54

/**
 * ----------------------------------
 * Respond status code (arg of CMD)
 * OK: 2xx
 * CLI-ERROR: 4xx
 * SRV-ERROR: 5xx
 * OTHERS: 6xx
 * ----------------------------------
 */
#define	CMD_ERR_UNKNOWN			600
#define	CMD_ERR_NOPROJECT		401
#define	CMD_ERR_TOOLONG			402
#define	CMD_ERR_INVALIDCHAR		403
#define	CMD_ERR_EMPTY			404
#define	CMD_ERR_NOACTION		405
#define	CMD_ERR_RUNNING			406
#define	CMD_ERR_REBUILDING		407

// no cmd_query_request for submit
#define	CMD_ERR_WRONGPLACE		450
#define	CMD_ERR_WRONGFORMAT		451
#define	CMD_ERR_EMPTYQUERY		452

// server error
#define	CMD_ERR_TIMEOUT			501
#define	CMD_ERR_IOERR			502
#define	CMD_ERR_NOMEM			503
#define	CMD_ERR_BUSY			504
#define	CMD_ERR_UNIMP			505
#define	CMD_ERR_NODB			506
#define	CMD_ERR_DBLOCKED		507
#define	CMD_ERR_CREATE_HOME		508
#define	CMD_ERR_INVALID_HOME	509
#define	CMD_ERR_REMOVE_HOME		510
#define	CMD_ERR_REMOVE_DB		511
#define	CMD_ERR_STAT			512
#define	CMD_ERR_OPEN_FILE		513
#define	CMD_ERR_TASK_CANCELED	514
#define	CMD_ERR_XAPIAN			515

// err string
#define	CMD_ERR_600				"Unknown internal error"
#define	CMD_ERR_401				"Project name not specified"
#define	CMD_ERR_402				"Data/Name too long"
#define	CMD_ERR_403				"Data/Name contains invalid characters"
#define	CMD_ERR_404				"Data/Name empty"
#define	CMD_ERR_405				"No action until timeout"
#define	CMD_ERR_406				"Import process running"
#define	CMD_ERR_407				"DB has been rebuilding"

#define	CMD_ERR_450				"Use the command in the wrong place"
#define	CMD_ERR_451				"Command format is incorrect"
#define	CMD_ERR_452				"Empty query"

#define	CMD_ERR_501				"IO timeout"
#define	CMD_ERR_502				"IO error"
#define	CMD_ERR_503				"Out of memory"
#define	CMD_ERR_504				"Server is too busy"
#define	CMD_ERR_505				"Command not implemented"
#define	CMD_ERR_506				"None of database avaiable"
#define	CMD_ERR_507				"Database is locked, cann't be removed"
#define	CMD_ERR_508				"Failed to create home directoy of the project"
#define	CMD_ERR_509				"Invalid home directory of the project"
#define	CMD_ERR_510				"Failed to remove home directoy of the project"
#define	CMD_ERR_511				"Failed to remove database directory or file"
#define	CMD_ERR_512				"Failed to stat the file or directory"
#define	CMD_ERR_513				"Failed to open file"
#define	CMD_ERR_514				"Task is canceled due to timeout/error"
#define	CMD_ERR_515				"Xapian ERROR"

// respond OK code
#define	CMD_OK_INFO				200
#define	CMD_OK_PROJECT			201

#define	CMD_OK_QUERY_STRING		202
#define	CMD_OK_DB_TOTAL			203
#define	CMD_OK_QUERY_TERMS		204
#define	CMD_OK_QUERY_CORRECTED	205
#define	CMD_OK_SEARCH_TOTAL		206
#define	CMD_OK_RESULT_BEGIN		CMD_OK_SEARCH_TOTAL
#define	CMD_OK_RESULT_END		207
#define	CMD_OK_TIMEOUT_SET		208
#define	CMD_OK_FINISHED			209
#define	CMD_OK_LOGGED			210

// for indexd
#define	CMD_OK_RQST_FINISHED	250
#define	CMD_OK_DB_CHANGED		251
#define	CMD_OK_DB_INFO			252
#define	CMD_OK_DB_CLEAN			253
#define	CMD_OK_PROJECT_ADD		254
#define	CMD_OK_PROJECT_DEL		255
#define	CMD_OK_DB_COMMITED		256
#define	CMD_OK_DB_REBUILD		257
#define	CMD_OK_LOG_FLUSHED		258
#define	CMD_OK_DICT_SAVED		259

// for searchd
// Each record per line, split by '\t'
#define	CMD_OK_RESULT_SYNONYMS	280

// for scws
// int(off|times)/char4(attr)/char[](word)
#define	CMD_OK_SCWS_RESULT		290		
// int(times)/char4(attr)/char[](word)
#define	CMD_OK_SCWS_TOPS		291		

// error str macro call to show err description
#define	__CMD_REPLACE(x,t)		CMD_##t##_##x
#define CMD_ERR_CODE(x)			__CMD_REPLACE(x,ERR)
#define	CMD_OK_CODE(x)			__CMD_REPLACE(x,OK)
#define CMD_ERR_STR(x)			CMD_ERR_CODE(__CMD_REPLACE(x,ERR))

/**
 * ---------------------------------------------
 * Inline functions
 * Conversion between value_no and search prefix
 * PREFIX: 16bytes [A-P], Z is special prefix
 * ---------------------------------------------
 */
#define	PREFIX_CHAR_START		'A'
#define	PREFIX_CHAR_END			'P'
#define	PREFIX_CHAR_ZZZ			'Z'

static inline void vno_to_prefix(int vno, char *p)
{
	if (vno != XS_DATA_VNO) {
		*p++ = (vno & 0x0f) + PREFIX_CHAR_START;
		if (vno & 0xf0) {
			*p++ = ((vno >> 4)&0x0f) + PREFIX_CHAR_START;
		}
	}
	*p = '\0';
}

static inline int prefix_to_vno(char *p)
{
	int vno = XS_DATA_VNO;

	while (*p == PREFIX_CHAR_ZZZ) p++;
	if (p[0] >= PREFIX_CHAR_START && p[0] <= PREFIX_CHAR_END) {
		vno = (p[0] - PREFIX_CHAR_START);
		if (p[1] >= PREFIX_CHAR_START && p[1] <= PREFIX_CHAR_END) {
			vno |= ((p[1] - PREFIX_CHAR_START) << 4);
		}
	}
	return (vno & 0xff);
}

#endif	/* __XS_CMD_20110517_H__ */

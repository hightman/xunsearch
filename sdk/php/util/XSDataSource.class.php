<?php
/**
 * XSDataSource 批量索引数据源定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 索引数据源抽象基类
 * 此部分代码仅用于 indexer 工具程序
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.utilf
 */
abstract class XSDataSource
{
	protected $type, $arg;
	protected $inCli;
	private $dataList, $dataPos;

	/**
	 * 构造函数
	 * @param mixed $arg 对象参数, 常为 SQL 语句或要导入的文件路径
	 */
	public function __construct($type, $arg)
	{
		$this->type = $type;
		$this->arg = $arg;
		$this->inCli = php_sapi_name() === 'cli';
		$this->init();
	}

	/**
	 * 取得数据源对象实例
	 * @param string $type 数据源类型, 如: mysql://.., json, csv ...
	 * @param mixed $arg 建立对象的参数, 如 SQL 语句, JSON/CSV 文件
	 * @return XSDataSource 初始化完毕的数据源对象
	 */
	public static function instance($type, $arg = null)
	{
		$type2 = ($pos = strpos($type, ':')) ? 'database' : $type;
		$class = 'XS' . ucfirst(strtolower($type2)) . 'DataSource';
		if (!class_exists($class)) {
			throw new XSException("Undefined data source type: `$type2'");
		}
		return new $class($type, $arg);
	}

	/**
	 * 从数据源中提取一条数据
	 * 实际使用时, 一般是循环调用此函数提取数据, 每条数据是由字段名为键的关联数组
	 * <pre>
	 * while ($ds->getData() !== false)
	 * {
	 *   ...
	 * }
	 * </pre>
	 * @return mixed 返回一条完整数据, 若无数据则返回 false
	 */
	final public function getData()
	{
		if ($this->dataPos === null || $this->dataPos === count($this->dataList)) {
			$this->dataPos = 0;
			$this->dataList = $this->getDataList();
			if (!is_array($this->dataList) || count($this->dataList) === 0) {
				$this->deinit();
				$this->dataList = $this->dataPos = null;
				return false;
			}
		}
		$data = $this->dataList[$this->dataPos];
		$this->dataPos++;
		return $data;
	}

	/**
	 * 取得数据源的准确字符集
	 * 如不能确定字符集, 请直接返回 false
	 * @return string 字符集名称
	 */
	public function getCharset()
	{
		return false;
	}

	/**
	 * 执行数据提取的准备工作
	 * 将自动在第一次提取数据前调用, 请在具体的数据源重载此函数
	 */
	protected function init()
	{
		
	}

	/**
	 * 执行数据提取完毕后的清理工作
	 * 将自动在没有更多数据供提取时调用此函数, 请在具体的数据源重载此函数
	 */
	protected function deinit()
	{
		
	}

	/**
	 * 从数据源中提取若干条数据
	 * 必须在数据源中定义此函数, 返回值必须是各条数据的数组
	 * @return array
	 */
	protected function getDataList()
	{
		return false;
	}
}

/**
 * SQL 数据库源
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util 
 */
class XSDatabaseDataSource extends XSDataSource
{
	const PLIMIT = 1000;

	private $sql, $offset, $limit;
	private $db; /* @var $db XSDatabase */

	/**
	 * 返回数据库输出字符集
	 * @return mixed 如果数据库不支持 UTF-8 转换则返回 false
	 */
	public function getCharset()
	{
		if ($this->db->setUtf8()) {
			return 'UTF-8';
		}
		return parent::getCharset();
	}

	protected function init()
	{
		if (strstr($this->type, 'sqlite')) {
			$pos = strpos($this->type, ':');
			$param = array('scheme' => substr($this->type, 0, $pos));
			$param['path'] = substr($this->type, $pos + (substr($this->type, $pos + 1, 2) === '//' ? 3 : 1));
		} elseif (!($param = parse_url($this->type))) {
			throw new XSException('Wrong format of DB connection parameter');
		} else {
			if (isset($param['user'])) {
				$param['user'] = urldecode($param['user']);
			}
			if (isset($param['pass'])) {
				$param['pass'] = urldecode($param['pass']);
			}
			$param['path'] = isset($param['path']) ? trim($param['path'], '/') : '';
			if (empty($param['path'])) {
				throw new XSException('Not contain dbname of DB connection parameter');
			}
			if (($pos = strpos($param['path'], '/')) === false) {
				$param['dbname'] = $param['path'];
			} else {
				$param['dbname'] = substr($param['path'], 0, $pos);
				$param['table'] = substr($param['path'], $pos + 1);
			}
		}

		// get driver
		$driver = self::getDriverName($param['scheme']);
		$class = 'XSDatabase' . ucfirst($driver);
		if (!class_exists($class)) {
			throw new XSException("Undefined database driver: '$driver'");
		}
		$this->db = new $class;
		$this->db->connect($param);

		// set SQL & parse limit/offset
		$this->limit = $this->offset = 0;
		$sql = $this->arg;
		if (empty($sql)) {
			if (!isset($param['table'])) {
				throw new XSException('Not specified any query SQL or db table');
			}
			$sql = 'SELECT * FROM ' . $param['table'];
		} elseif (preg_match('/ limit\s+(\d+)(?:\s*,\s*(\d+)|\s+offset\s+(\d+))?\s*$/i', $sql, $match)) {
			if (isset($match[3])) {  // LIMIT xxx OFFSET yyy
				$this->offset = intval($match[3]);
				$this->limit = intval($match[1]);
			} elseif (isset($match[2])) { // LIMIT yyy, xxx
				$this->offset = intval($match[1]);
				$this->limit = intval($match[2]);
			} else { // lIMIT xxx
				$this->limit = intval($match[1]);
			}
			$sql = substr($sql, 0, strlen($sql) - strlen($match[0]));
		}

		$this->sql = $sql;
		if ($this->limit == 0) {
			$sql = preg_replace('/SELECT\s+.+?\sFROM\s/is', 'SELECT COUNT(*) AS count FROM ', $sql);
			$res = $this->db->query1($sql);
			$this->limit = $res['count'] - $this->offset;
		}
	}

	protected function deinit()
	{
		$this->db->close();
	}

	/**
	 * 返回一批数据
	 * @return 结果数组, 没有更多数据时返回 false
	 */
	protected function getDataList()
	{
		if ($this->limit <= 0) {
			return false;
		}
		$sql = $this->sql . ' LIMIT ' . min(self::PLIMIT, $this->limit) . ' OFFSET ' . $this->offset;
		$this->limit -= self::PLIMIT;
		$this->offset += self::PLIMIT;
		return $this->db->query($sql);
	}

	/**
	 * 取解数据连接驱动名
	 */
	private static function getDriverName($scheme)
	{
		$name = strtr(strtolower($scheme), '.-', '__');
		if ($name == 'mysql' && !function_exists('mysql_connect')) {
			if (class_exists('mysqli')) {
				$name = 'mysqli';
			} elseif (extension_loaded('pdo_mysql')) {
				$name = 'pdo_mysql';
			}
		}
		if ($name == 'sqlite' && !function_exists('sqlite_open')) {
			if (class_exists('sqlite3')) {
				$name = 'sqlite3';
			} elseif (extension_loaded('pdo_sqlite')) {
				$name = 'pdo_sqlite';
			}
		}
		if ($name == 'sqlite3' && !class_exists('sqlite3') && extension_loaded('pdo_sqlite')) {
			$name = 'pdo_sqlite';
		}
		if (substr($name, 0, 4) != 'pdo_' && extension_loaded('pdo_' . $name)) {
			$name = 'pdo_' . $name;
		}
		return $name;
	}
}

/**
 * JSON 数据源
 * 要求以 \n (换行符) 分割, 每行为一条完整的 json 数据
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util  
 */
class XSJsonDataSource extends XSDataSource
{
	private $fd, $line;
	public $invalidLines = 0;

	protected function init()
	{
		$file = $this->arg;
		if (empty($file) && $this->inCli) {
			echo "WARNING: input file not specified, read data from <STDIN>\n";
			$file = 'php://stdin';
		}
		if (!($this->fd = fopen($file, 'r'))) {
			throw new XSException("Can not open input file: '$file'");
		}
		$this->line = 0;
	}

	protected function deinit()
	{
		if ($this->fd) {
			fclose($this->fd);
			$this->fd = null;
		}
	}

	protected function getDataList()
	{
		// read line (check to timeout?)
		$line = '';
		while (true) {
			$buf = fgets($this->fd, 8192);
			if ($buf === false || strlen($buf) === 0) {
				break;
			}
			$line .= $buf;
			if (strlen($buf) < 8191 || substr($buf, - 1, 1) === "\n") {
				break;
			}
		}

		// empty line (end of file)
		if (empty($line)) {
			if ($this->inCli) {
				echo "INFO: reach end of the file, total lines: " . $this->line . "\n";
			}
			return false;
		}

		// try to decode the line
		$this->line++;
		$line = rtrim($line, "\r\n");
		if (strlen($line) === 0) {
			if ($this->inCli) {
				echo "WARNING: empty line #" . $this->line . "\n";
			}
			$this->invalidLines++;
			return $this->getDataList();
		}

		$item = json_decode($line, true);
		if (!is_array($item) || count($item) === 0) {
			switch (json_last_error()) {
				case JSON_ERROR_DEPTH:
					$error = ' - Maximum stack depth exceeded';
					break;
				case JSON_ERROR_CTRL_CHAR:
					$error = ' - Unexpected control character found';
					break;
				case JSON_ERROR_SYNTAX:
					$error = ' - Syntax error, malformed JSON';
					break;
				default :
					$error = (count($item) === 0 ? ' - Empty array' : '');
					break;
			}
			if ($this->inCli) {
				echo "WARNING: invalid line #" . $this->line . $error . "\n";
			}
			$this->invalidLines++;
			return $this->getDataList();
		}
		return array($item);
	}
}

/**
 * CSV 数据源
 * 可在文件开头指定字段(必须是有效字段), 否则将默认按照 {@link XS} 项目字段顺序填充
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util  
 */
class XSCsvDataSource extends XSDataSource
{
	private $delim = ',', $line;
	public $invalidLines = 0;

	protected function init()
	{
		$file = $this->arg;
		if (empty($file) && $this->inCli) {
			echo "WARNING: input file not specified, read data from <STDIN>\n";
			$file = 'php://stdin';
		}
		if (!($this->fd = fopen($file, 'r'))) {
			throw new XSException("Can not open input file: '$file'");
		}
		$this->line = 0;
		if (isset($_SERVER['XS_CSV_DELIMITER'])) {
			$this->delim = $_SERVER['XS_CSV_DELIMITER'];
		}
	}

	protected function deinit()
	{
		if ($this->fd) {
			fclose($this->fd);
			$this->fd = null;
		}
	}

	protected function getDataList()
	{
		if (($item = fgetcsv($this->fd, 0, $this->delim)) === false) {
			if ($this->inCli) {
				echo "INFO: reach end of file or error occured, total lines: " . $this->line . "\n";
			}
			return false;
		}

		$this->line++;
		if (count($item) === 1 && is_null($item[0])) {
			if ($this->inCli) {
				echo "WARNING: invalid csv line #" . $this->line . "\n";
			}
			$this->invalidLines++;
			return $this->getDataList();
		}
		return array($item);
	}
}

/**
 * 数据库操作基类
 * 定义了 SQL 数据库源的四个基本操作: connect/query/close/setUtf8
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
abstract class XSDatabase
{

	/**
	 * 连接数据库
	 * @param array $param 连接参数, 采用 parse_url 解析, 可能包含: scheme,user,pass,host,path,table,dbname ...
	 */
	abstract public function connect($param);

	/**
	 * 关闭数据库连接
	 */
	abstract public function close();

	/**
	 * 查询 SQL 语句
	 * @return mixed 非 SELECT 语句返回执行结果(true/false), SELECT 语句返回所有结果行的数组
	 */
	abstract public function query($sql);

	/**
	 * 设置数据库字符集为 UTF-8
	 * @return bool 如果数据库能直接输出 UTF-8 编码则返回 true 否则返回 false
	 */
	public function setUtf8()
	{
		return false;
	}

	/**
	 * 查询数据库首行
	 * @param string $sql
	 * @return 查询结果首行, 失败或无数据则返回 false
	 */
	public function query1($sql)
	{
		$sql = preg_replace('/ limit\s+(\d+)(?:\s*,\s*(\d+)|\s+offset\s+(\d+))?\s*$/i', '', $sql);
		$sql .= ' LIMIT 1';
		$res = $this->query($sql);
		return (is_array($res) && isset($res[0])) ? $res[0] : false;
	}
}

/**
 * 使用传统 MySQL 扩展
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db   
 */
class XSDatabaseMySQL extends XSDatabase
{
	private $link;

	/**
	 * 连接数据库
	 * @param array $param 连接参数, 包含: user,pass,host,table,dbname ...
	 */
	public function connect($param)
	{
		$host = isset($param['host']) ? $param['host'] : ini_get('mysql.default_host');
		$host .= (isset($param['port']) && $param['port'] != 3306) ? ':' . $param['port'] : '';
		$user = isset($param['user']) ? $param['user'] : ini_get('mysql.default_user');
		$pass = isset($param['pass']) ? $param['pass'] : ini_get('mysql.default_pw');
		if (($this->link = mysql_connect($host, $user, $pass)) === false) {
			throw new XSException("Can not connect to mysql server: '$user@$host'");
		}
		if (!mysql_select_db($param['dbname'], $this->link)) {
			$this->close();
			throw new XSException("Can not switch to database name: '{$param['dbname']}'");
		}
		$this->setUtf8();
	}

	/**
	 * 关闭数据库连接
	 */
	public function close()
	{
		if ($this->link) {
			mysql_close($this->link);
			$this->link = null;
		}
	}

	/**
	 * 执行 SQL 语句查询
	 * @param string $sql 要执行的 SQL 语句	 
	 * @return mixed
	 */
	public function query($sql)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = mysql_query($sql, $this->link);
		if ($res === false) {
			throw new XSException('MySQL ERROR(#' . mysql_errno($this->link) . '): ' . mysql_error($this->link));
		}
		if (!is_resource($res)) {
			$ret = $res;
		} else {
			$ret = array();
			while ($tmp = mysql_fetch_assoc($res)) {
				$ret[] = $tmp;
			}
			mysql_free_result($res);
		}
		return $ret;
	}

	/**
	 * 将输出字符集设置为 UTF-8
	 * @return bool MySQL 自 4.1.0 起支持字符集
	 */
	public function setUtf8()
	{
		if (version_compare(mysql_get_server_info($this->link), '4.1.0', '>=')) {
			return @mysql_query("SET NAMES utf8", $this->link);
		}
		return false;
	}
}

/**
 * 面向对象的 PostgreSQL 扩展
 *
 * @author freechoice <freechoice@qq.com>
 * @version 1.0.0
 * @package XS.util.db
 */
class XSDatabasePgSQL extends XSDatabase
{
	private $link;

	public function connect($param)
	{
		$dsn = "host={$param['host']} ";
		$dsn .= isset($param['port']) ? "port={$param['port']} " : '';
		$dsn .= "dbname={$param['dbname']} user={$param['user']} password={$param['pass']}";
		if (!($this->link = @pg_connect($dsn))) {
			throw new XSException('Error connecting to PGSQL database:' . $param['dbname'] . '.');
			pg_set_error_verbosity($this->link, PGSQL_ERRORS_DEFAULT);
			pg_query('SET standard_conforming_strings=off');
		}
	}

	/**
	 * 关闭数据库连接
	 */
	public function close()
	{
		if (is_resource($this->link)) {
			pg_close($this->link);
			$this->link = null;
		}
	}

	/**
	 * 执行 SQL 语句查询
	 * @param string $sql 要执行的 SQL 语句
	 * @return mixed
	 */
	public function query($query)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = pg_query($this->link, $query);
		if ($res === false) {
			throw new XSException('PgSQL ERROR: ' . pg_last_error($this->link));
		}
		$ret = array();
		while ($tmp = pg_fetch_assoc($res)) {
			$ret[] = $tmp;
		}
		pg_free_result($res);
		return $ret;
	}

	/**
	 * 将输出字符集设置为 UTF-8
	 */
	public function setUtf8()
	{
		pg_set_client_encoding($this->link, 'UTF8');
	}
}

/**
 * 面向对象的 MySQLI 扩展
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
class XSDatabaseMySQLI extends XSDatabase
{
	private $obj;

	/**
	 * 连接数据库
	 * @param array $param 连接参数, 包含: user,pass,host,table,dbname ...
	 */
	public function connect($param)
	{
		$host = isset($param['host']) ? $param['host'] : ini_get('mysqli.default_host');
		$user = isset($param['user']) ? $param['user'] : ini_get('mysqli.default_user');
		$pass = isset($param['pass']) ? $param['pass'] : ini_get('mysqli.default_pw');
		$port = isset($param['port']) ? $param['port'] : ini_get('mysqli.default_port');
		$this->obj = new mysqli($host, $user, $pass, '', $port);
		if ($this->obj->connect_error) {
			throw new XSException("Can not connect to mysql server: '$user@$host'");
		}
		if (!$this->obj->select_db($param['dbname'])) {
			$this->close();
			throw new XSException("Can not switch to database name: '{$param['dbname']}'");
		}
		$this->setUtf8();
	}

	/**
	 * 关闭数据库连接
	 */
	public function close()
	{
		if ($this->obj) {
			$this->obj->close();
			$this->obj = null;
		}
	}

	/**
	 * 执行 SQL 语句查询
	 * @param string $sql 要执行的 SQL 语句	 
	 * @return mixed
	 */
	public function query($sql)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = $this->obj->query($sql);
		if ($res === false) {
			throw new XSException('MySQL ERROR(#' . $this->obj->error . '): ' . $this->obj->errno);
		}
		if (!is_object($res)) {
			$ret = $res;
		} else {
			$ret = array();
			while ($tmp = $res->fetch_assoc()) {
				$ret[] = $tmp;
			}
			$res->free();
		}
		return $ret;
	}

	/**
	 * 将输出字符集设置为 UTF-8
	 * @return bool 始终返回 true
	 */
	public function setUtf8()
	{
		$this->obj->set_charset('utf8');
		return true;
	}
}

/**
 * 使用传统的 SQLite 扩展
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
class XSDatabaseSQLite extends XSDatabase
{
	private $link;

	/**
	 * 打开数据库
	 * @param array $param 连接参数, 包含: path
	 */
	public function connect($param)
	{
		if (($this->link = sqlite_open($param['path'])) === false) {
			throw new XSException("Can not open sqlite file: '{$param['path']}'");
		}
	}

	/**
	 * 关闭数据库
	 */
	public function close()
	{
		if ($this->link) {
			sqlite_close($this->link);
			$this->link = null;
		}
	}

	/**
	 * 执行 SQL 语句查询
	 * @param string $sql 要执行的 SQL 语句	 
	 * @return mixed
	 */
	public function query($sql)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = sqlite_query($this->link, $sql);
		if ($res === false) {
			throw new XSException('SQLITE ERROR: ' . sqlite_error_string($this->link));
		}
		if (!is_resource($res)) {
			$ret = $res;
		} else {
			$ret = array();
			while ($tmp = sqlite_fetch_array($res, SQLITE_ASSOC)) {
				$ret[] = $tmp;
			}
		}
		return $ret;
	}
}

/**
 * 面向对象的 SQLite3 扩展
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
class XSDatabaseSQLite3 extends XSDatabase
{
	private $obj;

	/**
	 * 打开数据库
	 * @param array $param 连接参数, 包含: path
	 */
	public function connect($param)
	{
		try {
			$this->obj = new SQLite3($param['path'], SQLITE3_OPEN_READONLY);
		} catch (Exception $e) {
			throw new XSException($e->getMessage());
		}
	}

	/**
	 * 关闭数据库
	 */
	public function close()
	{
		if ($this->obj) {
			$this->obj->close();
			$this->obj = null;
		}
	}

	/**
	 * 执行 SQL 语句查询
	 * @param string $sql 要执行的 SQL 语句	 
	 * @return mixed
	 */
	public function query($sql)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = $this->obj->query($sql);
		if ($res === false) {
			throw new XSException('SQLITE3 ERROR(#' . $this->obj->lastErrorCode() . '): ' . $this->obj->lastErrorMsg());
		}
		if (!is_object($res)) {
			$ret = $res;
		} else {
			$ret = array();
			while ($tmp = $res->fetchArray(SQLITE3_ASSOC)) {
				$ret[] = $tmp;
			}
			$res->finalize();
		}
		return $ret;
	}
}

/**
 * 面向对象的 PDO 扩展基类
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
abstract class XSDatabasePDO extends XSDatabase
{
	protected $obj;

	/**
	 * 连接数据库
	 * 具体的每个类必须实现 {@link makeDsn} 来将参数转换为 dsn
	 * @param array $param 连接参数, 包含: user, pass ...
	 * @see makeDsn
	 */
	public function connect($param)
	{
		$dsn = $this->makeDsn($param);
		$user = isset($param['user']) ? $param['user'] : 'root';
		$pass = isset($param['pass']) ? $param['pass'] : '';
		try {
			$this->obj = new PDO($dsn, $user, $pass);
		} catch (PDOException $e) {
			throw new XSException($e->getMessage());
		}
	}

	/**
	 * 关闭数据库
	 */
	public function close()
	{
		$this->obj = null;
	}

	/**
	 * 执行 SQL 语句
	 * @param string $sql 要执行的 SQL 语句
	 * @return mixed
	 */
	public function query($sql)
	{
		//echo "[DEBUG] SQL: $sql\n";
		$res = $this->obj->query($sql);
		if ($res === false) {
			$info = $this->obj->errorInfo();
			throw new XSException('SQLSTATE[' . $info[0] . '] [' . $info[1] . '] ' . $info[2]);
		}
		$ret = $res->fetchAll(PDO::FETCH_ASSOC);
		return $ret;
	}

	/**
	 * 提取参数内容生成 PDO 连接专用的 DSN
	 * @param array $param 
	 */
	abstract protected function makeDsn($param);
}

/**
 * PDO.MySQL 实现
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
class XSDatabasePDO_MySQL extends XSDatabasePDO
{

	/**
	 * 生成 MySQL DSN
	 * @param array $param 包含 host, port, dbname
	 * @return string
	 */
	protected function makeDsn($param)
	{
		$dsn = 'mysql:host=' . (isset($param['host']) ? $param['host'] : 'localhost');
		if (isset($param['port']) && $param['port'] !== 3306) {
			$dsn .= ';port=' . $param['port'];
		}
		$dsn .= ';dbname=' . $param['dbname'];
		return $dsn;
	}

	/**
	 * 将输出字符集设置为 UTF-8
	 * @return bool 始终返回 true
	 */
	public function setUtf8()
	{
		// BUGFIXED: 此处应为不带引号的 utf8
		return $this->obj->prepare("SET NAMES utf8")->execute();
	}
}

/**
 * PDO.Pgsql 实现
 *
 * @author freechoice <freechoice@qq.com>
 * @version 1.0.0
 * @package XS.util.db
 */
class XSDatabasePDO_PgSQL extends XSDatabasePDO
{

	/**
	 * 生成 Postgres DSN
	 * @param array $param 包含 path 为数据库路径
	 * @return string
	 */
	protected function makeDsn($param)
	{
		$dsn = "pgsql:host={$param['host']};";
		$dsn .= isset($param['port']) ? "port={$param['port']};" : '';
		$dsn .= "dbname={$param['dbname']};client_encoding=utf-8";
		return $dsn;
	}

	/**
	 * 将输出字符集设置为 UTF-8
	 */
	public function setUtf8()
	{
		return true;
	}
}

/**
 * PDO.SQLite 实现
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util.db  
 */
class XSDatabasePDO_SQLite extends XSDatabasePDO
{

	/**
	 * 生成 SQLite DSN
	 * @param array $param 包含 path 为数据库路径
	 * @return string
	 */
	protected function makeDsn($param)
	{
		$dsn = 'sqlite:' . $param['path'];
		return $dsn;
	}
}

/**
 * 数据过滤器的接口
 * 以便在提交到索引前有一个修改和调整数据的机会
 * 
 * @author hightman <hightman@twomice.net>
 * @since 1.1.0
 * @package XS.util
 */
interface XSDataFilter
{

	/**
	 * 字段数据处理函数
	 * @param array $data 字段名和值组成的数据数组
	 * @param mixed $cs 数据字符集, 默认 false 表示无法确定源字符集
	 * @return mixed 返回处理后的数据数组, 返回 false 表示本条数据不加入索引
	 */
	public function process($data, $cs = false);

	/**
	 * 索引文档处理函数
	 * 在此通过 {@link XSDocument::addIndex} 或 {@link XSDocument::addTerm} 做索引相关调整
	 * @param XSDocument $doc 索引文档
	 * @since 1.3.4
	 */
	public function processDoc($doc);
}

/**
 * 内置调试过滤器, 直接打印数据内容
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util
 */
class XSDebugFilter implements XSDataFilter
{

	public function process($data, $cs = false)
	{
		echo "\n----- DEBUG DATA INFO -----\n";
		print_r($data);
		return $data;
	}

	public function processDoc($doc)
	{
		
	}
}

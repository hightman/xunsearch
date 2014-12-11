<?php
/**
 * XSServer 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * XSCommand 命令对象
 * 是与服务端交互的最基本单位, 命令对象可自动转换为通讯字符串,
 * 命令结构参见 C 代码中的 struct xs_cmd 定义, 头部长度为 8字节.
 * 
 * @property int $arg 参数, 相当于 (arg1<<8)|arg2 的值
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSCommand extends XSComponent
{
	/**
	 * @var int 命令代码
	 * 通常是预定义常量 XS_CMD_xxx, 取值范围 0~255
	 */
	public $cmd = XS_CMD_NONE;

	/**
	 * @var int 参数1
	 * 取值范围 0~255, 具体含义根据不同的 CMD 而变化
	 */
	public $arg1 = 0;

	/**
	 * @var int 参数2
	 * 取值范围 0~255, 常用于存储 value no, 具体参照不同 CMD 而确定
	 */
	public $arg2 = 0;

	/**
	 * @var string 主数据内容, 最长 2GB
	 */
	public $buf = '';

	/**
	 * @var string 辅数据内容, 最长 255字节
	 */
	public $buf1 = '';

	/**
	 * 构造函数
	 * @param mixed $cmd 命令类型或命令数组
	 *        当类型为 int 表示命令代码, 范围是 1~255, 参见 xs_cmd.inc.php 里的定义
	 *        当类型为 array 时忽略其它参数, 可包含 cmd, arg1, arg2, buf, buf1 这些键值
	 * @param int $arg1 参数1, 其值为 0~255, 具体含义视不同 CMD 而确定
	 * @param int $arg2 参数2, 其值为 0~255, 具体含义视不同 CMD 而确定, 常用于存储 value no
	 * @param string $buf 字符串内容, 最大长度为 2GB
	 * @param string $buf1 字符串内容1, 最大长度为 255字节
	 */
	public function __construct($cmd, $arg1 = 0, $arg2 = 0, $buf = '', $buf1 = '')
	{
		if (is_array($cmd)) {
			foreach ($cmd as $key => $value) {
				if ($key === 'arg' || property_exists($this, $key)) {
					$this->$key = $value;
				}
			}
		} else {
			$this->cmd = $cmd;
			$this->arg1 = $arg1;
			$this->arg2 = $arg2;
			$this->buf = $buf;
			$this->buf1 = $buf1;
		}
	}

	/**
	 * 转换为封包字符串
	 * @return string 用于服务端交互的字符串
	 */
	public function __toString()
	{
		if (strlen($this->buf1) > 0xff) {
			$this->buf1 = substr($this->buf1, 0, 0xff);
		}
		return pack('CCCCI', $this->cmd, $this->arg1, $this->arg2, strlen($this->buf1), strlen($this->buf)) . $this->buf . $this->buf1;
	}

	/**
	 * 获取属性 arg 的值
	 * @return int 参数值
	 */
	public function getArg()
	{
		return $this->arg2 | ($this->arg1 << 8);
	}

	/**
	 * 设置属性 arg 的值
	 * @param int $arg 参数值
	 */
	public function setArg($arg)
	{
		$this->arg1 = ($arg >> 8) & 0xff;
		$this->arg2 = $arg & 0xff;
	}
}

/**
 * XSServer 服务器操作对象
 * 同时兼容于 indexd, searchd, 所有交互均采用 {@link XSCommand} 对象
 * 
 * @property string $project 当前使用的项目名
 * @property-write int $timeout 服务端IO超时秒数, 默认为 5秒
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSServer extends XSComponent
{
	/**
	 * 连接标志定义(常量)
	 */
	const FILE = 0x01;
	const BROKEN = 0x02;

	/**
	 * @var XS 服务端关联的 XS 对象
	 */
	public $xs;
	protected $_sock, $_conn;
	protected $_flag;
	protected $_project;
	protected $_sendBuffer;

	/**
	 * 构造函数, 打开连接
	 * @param string $conn 服务端连接参数
	 * @param XS $xs 需要捆绑的 xs 对象
	 */
	public function __construct($conn = null, $xs = null)
	{
		$this->xs = $xs;
		if ($conn !== null) {
			$this->open($conn);
		}
	}

	/**
	 * 析构函数, 关闭连接
	 */
	public function __destruct()
	{
		$this->xs = null;
		$this->close();
	}

	/**
	 * 打开服务端连接
	 * 如果已关联 XS 对象, 则会同时切换至相应的项目名称
	 * @param mixed $conn 服务端连接参数, 支持: <端口号|host:port|本地套接字路径>
	 */
	public function open($conn)
	{
		$this->close();
		$this->_conn = $conn;
		$this->_flag = self::BROKEN;
		$this->_sendBuffer = '';
		$this->_project = null;
		$this->connect();
		$this->_flag ^= self::BROKEN;
		if ($this->xs instanceof XS) {
			$this->setProject($this->xs->getName());
		}
	}

	/**
	 * 重新打开连接
	 * 仅应用于曾经成功打开的连并且异常关闭了
	 * @param bool $force 是否强制重新连接, 默认为否
	 * @return XSServer 返回自己, 以便串接操作
	 */
	public function reopen($force = false)
	{
		if ($this->_flag & self::BROKEN || $force === true) {
			$this->open($this->_conn);
		}
		return $this;
	}

	/**
	 * 关闭连接
	 * 附带发送发送 quit 命令
	 * @param bool $ioerr 关闭调用是否由于 IO 错误引起的, 以免发送 quit 指令
	 */
	public function close($ioerr = false)
	{
		if ($this->_sock && !($this->_flag & self::BROKEN)) {
			if (!$ioerr && $this->_sendBuffer !== '') {
				$this->write($this->_sendBuffer);
				$this->_sendBuffer = '';
			}
			if (!$ioerr && !($this->_flag & self::FILE)) {
				$cmd = new XSCommand(XS_CMD_QUIT);
				fwrite($this->_sock, $cmd);
			}
			fclose($this->_sock);
			$this->_flag |= self::BROKEN;
		}
	}

	/**
	 * @return string 连接字符串
	 */
	public function getConnString()
	{
		$str = $this->_conn;
		if (is_int($str) || is_numeric($str)) {
			$str = 'localhost:' . $str;
		} elseif (strpos($str, ':') === false) {
			$str = 'unix://' . $str;
		}
		return $str;
	}

	/**
	 * 获取连接资源描述符
	 * @return mixed 连接标识, 仅用于内部测试等目的
	 */
	public function getSocket()
	{
		return $this->_sock;
	}

	/**
	 * 获取当前项目名称
	 * @return string 项目名称
	 */
	public function getProject()
	{
		return $this->_project;
	}

	/**
	 * 设置当前项目
	 * @param string $name 项目名称
	 * @param string $home 项目在服务器上的目录路径, 可选参数(不得超过255字节).
	 */
	public function setProject($name, $home = '')
	{
		if ($name !== $this->_project) {
			$cmd = array('cmd' => XS_CMD_USE, 'buf' => $name, 'buf1' => $home);
			$this->execCommand($cmd, XS_CMD_OK_PROJECT);
			$this->_project = $name;
		}
	}

	/**
	 * 设置服务端超时秒数
	 * @param int $sec 秒数, 设为 0则永不超时直到客户端主动关闭
	 */
	public function setTimeout($sec)
	{
		$cmd = array('cmd' => XS_CMD_TIMEOUT, 'arg' => $sec);
		$this->execCommand($cmd, XS_CMD_OK_TIMEOUT_SET);
	}

	/**
	 * 执行服务端指令并获取返回值
	 * @param mixed $cmd 要提交的指令, 若不是 XSCommand 实例则作为构造函数的第一参数创建对象
	 * @param int $res_arg 要求的响应参数, 默认为 XS_CMD_NONE 即不检测, 若检测结果不符
	 *        则认为命令调用失败, 会返回 false 并设置相应的出错信息
	 * @param int $res_cmd 要求的响应指令, 默认为 XS_CMD_OK 即要求结果必须正确.
	 * @return mixed 若无需要检测结果则返回 true, 其它返回响应的 XSCommand 对象
	 * @throw XSException 操作失败或响应命令不正确时抛出异常
	 */
	public function execCommand($cmd, $res_arg = XS_CMD_NONE, $res_cmd = XS_CMD_OK)
	{
		// create command object
		if (!$cmd instanceof XSCommand) {
			$cmd = new XSCommand($cmd);
		}

		// just cache the cmd for those need not answer
		if ($cmd->cmd & 0x80) {
			$this->_sendBuffer .= $cmd;
			return true;
		}

		// send cmd to server
		$buf = $this->_sendBuffer . $cmd;
		$this->_sendBuffer = '';
		$this->write($buf);

		// return true directly for local file
		if ($this->_flag & self::FILE) {
			return true;
		}

		// got the respond
		$res = $this->getRespond();

		// check respond
		if ($res->cmd === XS_CMD_ERR && $res_cmd != XS_CMD_ERR) {
			throw new XSException($res->buf, $res->arg);
		}
		// got unexpected respond command
		if ($res->cmd != $res_cmd || ($res_arg != XS_CMD_NONE && $res->arg != $res_arg)) {
			throw new XSException('Unexpected respond {CMD:' . $res->cmd . ', ARG:' . $res->arg . '}');
		}
		return $res;
	}

	/**
	 * 往服务器直接发送指令 (无缓存)
	 * @param mixed $cmd 要提交的指令, 支持 XSCommand 实例或 cmd 构造函数的第一参数
	 * @throw XSException 失败时抛出异常
	 */
	public function sendCommand($cmd)
	{
		if (!$cmd instanceof XSCommand) {
			$cmd = new XSCommand($cmd);
		}
		$this->write(strval($cmd));
	}

	/**
	 * 从服务器读取响应指令
	 * @return XSCommand 成功返回响应指令
	 * @throw XSException 失败时抛出异常
	 */
	public function getRespond()
	{
		// read data from server
		$buf = $this->read(8);
		$hdr = unpack('Ccmd/Carg1/Carg2/Cblen1/Iblen', $buf);
		$res = new XSCommand($hdr);
		$res->buf = $this->read($hdr['blen']);
		$res->buf1 = $this->read($hdr['blen1']);
		return $res;
	}

	/**
	 * 判断服务端是否有可读数据
	 * 用于某些特别情况在 {@link getRespond} 前先调用和判断, 以免阻塞
	 * @return bool 如果有返回 true, 否则返回 false
	 */
	public function hasRespond()
	{
		// check socket
		if ($this->_sock === null || $this->_flag & (self::BROKEN | self::FILE)) {
			return false;
		}
		$wfds = $xfds = array();
		$rfds = array($this->_sock);
		$res = stream_select($rfds, $wfds, $xfds, 0, 0);
		return $res > 0;
	}

	/**
	 * 写入数据
	 * @param string $buf 要写入的字符串
	 * @param string $len 要写入的长度, 默认为字符串长度
	 * @throw XSException 失败时抛出异常
	 */
	protected function write($buf, $len = 0)
	{
		// quick return for empty buf
		$buf = strval($buf);
		if ($len == 0 && ($len = $size = strlen($buf)) == 0) {
			return true;
		}

		// loop to send data
		$this->check();
		while (true) {
			$bytes = fwrite($this->_sock, $buf, $len);
			if ($bytes === false || $bytes === 0 || $bytes === $len) {
				break;
			}
			$len -= $bytes;
			$buf = substr($buf, $bytes);
		}

		// error occured
		if ($bytes === false || $bytes === 0) {
			$meta = stream_get_meta_data($this->_sock);
			$this->close(true);
			$reason = $meta['timed_out'] ? 'timeout' : ($meta['eof'] ? 'closed' : 'unknown');
			$msg = 'Failed to send the data to server completely ';
			$msg .= '(SIZE:' . ($size - $len) . '/' . $size . ', REASON:' . $reason . ')';
			throw new XSException($msg);
		}
	}

	/**
	 * 读取数据
	 * @param int $len 要读入的长度
	 * @return string 成功时返回读到的字符串
	 * @throw XSException 失败时抛出异常
	 */
	protected function read($len)
	{
		// quick return for zero size
		if ($len == 0) {
			return '';
		}

		// loop to send data
		$this->check();
		for ($buf = '', $size = $len;;) {
			$bytes = fread($this->_sock, $len);
			if ($bytes === false || strlen($bytes) == 0) {
				break;
			}
			$len -= strlen($bytes);
			$buf .= $bytes;
			if ($len === 0) {
				return $buf;
			}
		}

		// error occured
		$meta = stream_get_meta_data($this->_sock);
		$this->close(true);
		$reason = $meta['timed_out'] ? 'timeout' : ($meta['eof'] ? 'closed' : 'unknown');
		$msg = 'Failed to recv the data from server completely ';
		$msg .= '(SIZE:' . ($size - $len) . '/' . $size . ', REASON:' . $reason . ')';
		throw new XSException($msg);
	}

	/**
	 * 检测服务端的连接情况
	 * @throw XSException 连接不可用时抛出异常
	 */
	protected function check()
	{
		if ($this->_sock === null) {
			throw new XSException('No server connection');
		}
		if ($this->_flag & self::BROKEN) {
			throw new XSException('Broken server connection');
		}
	}

	/**
	 * 连接服务端
	 * @throw XSException 无法连接时抛出异常
	 */
	protected function connect()
	{
		// connect to server
		$conn = $this->_conn;
		if (is_int($conn) || is_numeric($conn)) {
			$host = 'localhost';
			$port = intval($conn);
		} elseif (!strncmp($conn, 'file://', 7)) {
			// write-only for saving index exchangable data to file
			// NOTE: this will cause file content be turncated
			$conn = substr($conn, 7);
			if (($sock = @fopen($conn, 'wb')) === false) {
				throw new XSException('Failed to open local file for writing: `' . $conn . '\'');
			}
			$this->_flag |= self::FILE;
			$this->_sock = $sock;
			return;
		} elseif (($pos = strpos($conn, ':')) !== false) {
			$host = substr($conn, 0, $pos);
			$port = intval(substr($conn, $pos + 1));
		} else {
			$host = 'unix://' . $conn;
			$port = -1;
		}
		if (($sock = @fsockopen($host, $port, $errno, $error, 5)) === false) {
			throw new XSException($error . '(C#' . $errno . ', ' . $host . ':' . $port . ')');
		}

		// set socket options
		$timeout = ini_get('max_execution_time');
		$timeout = $timeout > 0 ? ($timeout - 1) : 30;
		stream_set_blocking($sock, true);
		stream_set_timeout($sock, $timeout);
		$this->_sock = $sock;
	}
}

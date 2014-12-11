<?php
/**
 * XSTokenizer 接口和内置分词器文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 自定义字段词法分析器接口
 * 系统将按照 {@link getTokens} 返回的词汇列表对相应的字段建立索引
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
interface XSTokenizer
{
	/**
	 * 内置分词器定义(常量)
	 */
	const DFL = 0;

	/**
	 * 执行分词并返回词列表
	 * @param string $value 待分词的字段值(UTF-8编码)
	 * @param XSDocument $doc 当前相关的索引文档
	 * @return array 切好的词组成的数组
	 */
	public function getTokens($value, XSDocument $doc = null);
}

/**
 * 内置空分词器
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
class XSTokenizerNone implements XSTokenizer
{

	public function getTokens($value, XSDocument $doc = null)
	{
		return array();
	}
}

/**
 * 内置整值分词器
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
class XSTokenizerFull implements XSTokenizer
{

	public function getTokens($value, XSDocument $doc = null)
	{
		return array($value);
	}
}

/**
 * 内置的分割分词器
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
class XSTokenizerSplit implements XSTokenizer
{
	private $arg = ' ';

	public function __construct($arg = null)
	{
		if ($arg !== null && $arg !== '') {
			$this->arg = $arg;
		}
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		if (strlen($this->arg) > 2 && substr($this->arg, 0, 1) == '/' && substr($this->arg, -1, 1) == '/') {
			return preg_split($this->arg, $value);
		}
		return explode($this->arg, $value);
	}
}

/**
 * 内置的定长分词器
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
class XSTokenizerXlen implements XSTokenizer
{
	private $arg = 2;

	public function __construct($arg = null)
	{
		if ($arg !== null && $arg !== '') {
			$this->arg = intval($arg);
			if ($this->arg < 1 || $this->arg > 255) {
				throw new XSException('Invalid argument for ' . __CLASS__ . ': ' . $arg);
			}
		}
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		$terms = array();
		for ($i = 0; $i < strlen($value); $i += $this->arg) {
			$terms[] = substr($value, $i, $this->arg);
		}
		return $terms;
	}
}

/**
 * 内置的步长分词器
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 */
class XSTokenizerXstep implements XSTokenizer
{
	private $arg = 2;

	public function __construct($arg = null)
	{
		if ($arg !== null && $arg !== '') {
			$this->arg = intval($arg);
			if ($this->arg < 1 || $this->arg > 255) {
				throw new XSException('Invalid argument for ' . __CLASS__ . ': ' . $arg);
			}
		}
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		$terms = array();
		$i = $this->arg;
		while (true) {
			$terms[] = substr($value, 0, $i);
			if ($i >= strlen($value)) {
				break;
			}
			$i += $this->arg;
		}
		return $terms;
	}
}

/**
 * SCWS - 分词器(与搜索服务端通讯)
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tokenizer
 * @since 1.3.1
 */
class XSTokenizerScws implements XSTokenizer
{
	const MULTI_MASK = 15;

	/* @var string 字符集，默认为项目字符集 */
	private static $_charset;

	/* @var array 选项设置记录 */
	private $_setting = array();

	/* @var XSServer 分词服务对象 */
	private static $_server;

	/**
	 * 构造函数
	 * 初始化用于分词的搜索服务端
	 * @param string $arg 复合等级参数，默认不指定
	 */
	public function __construct($arg = null)
	{
		if (self::$_server === null) {
			$xs = XS::getLastXS();
			if ($xs === null) {
				throw new XSException('An XS instance should be created before using ' . __CLASS__);
			}
			self::$_server = $xs->getScwsServer();
			self::$_server->setTimeout(0);
			self::$_charset = $xs->getDefaultCharset();
			// constants
			if (!defined('SCWS_MULTI_NONE')) {
				define('SCWS_MULTI_NONE', 0);
				define('SCWS_MULTI_SHORT', 1);
				define('SCWS_MULTI_DUALITY', 2);
				define('SCWS_MULTI_ZMAIN', 4);
				define('SCWS_MULTI_ZALL', 8);
			}
			if (!defined('SCWS_XDICT_XDB')) {
				define('SCWS_XDICT_XDB', 1);
				define('SCWS_XDICT_MEM', 2);
				define('SCWS_XDICT_TXT', 4);
			}
		}
		if ($arg !== null && $arg !== '') {
			$this->setMulti($arg);
		}
	}

	/**
	 * XSTokenizer 接口
	 */
	public function getTokens($value, XSDocument $doc = null)
	{
		$tokens = array();
		$this->setIgnore(true);
		// save charset, force to use UTF-8
		$_charset = self::$_charset;
		self::$_charset = 'UTF-8';
		$words = $this->getResult($value);
		foreach ($words as $word) {
			$tokens[] = $word['word'];
		}
		// restore charset
		self::$_charset = $_charset;
		return $tokens;
	}

	/**
	 * 设置字符集
	 * 默认字符集是 UTF-8, 这是指 {@link getResult} 系列函数的 $text 参数的字符集
	 * @param string $charset
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function setCharset($charset)
	{
		self::$_charset = strtoupper($charset);
		if (self::$_charset == 'UTF8') {
			self::$_charset = 'UTF-8';
		}
		return $this;
	}

	/**
	 * 设置忽略标点符号
	 * @param bool $yes 是否忽略
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function setIgnore($yes = true)
	{
		$this->_setting['ignore'] = new XSCommand(XS_CMD_SEARCH_SCWS_SET, XS_CMD_SCWS_SET_IGNORE, $yes === false
							? 0 : 1);
		return $this;
	}

	/**
	 * 设置复合分词选项
	 * @param int $mode 复合选项, 值范围 0~15
	 *  默认为值为 3, 可使用常量组合: 
	 *  SCWS_MULTI_SHORT|SCWS_MULTI_DUALITY|SCWS_MULTI_ZMAIN|SCWS_MULTI_ZALL
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function setMulti($mode = 3)
	{
		$mode = intval($mode) & self::MULTI_MASK;
		$this->_setting['multi'] = new XSCommand(XS_CMD_SEARCH_SCWS_SET, XS_CMD_SCWS_SET_MULTI, $mode);
		return $this;
	}

	/**
	 * 设置分词词典, 支持 TXT/XDB 格式
	 * @param string $fpath 服务端的词典路径
	 * @param int $mode 词典类型, 常量: SCWS_XDICT_XDB|SCWS_XDICT_TXT|SCWS_XDICT_MEM
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function setDict($fpath, $mode = null)
	{
		if (!is_int($mode)) {
			$mode = stripos($fpath, '.txt') !== false ? SCWS_XDICT_TXT : SCWS_XDICT_XDB;
		}
		$this->_setting['set_dict'] = new XSCommand(XS_CMD_SEARCH_SCWS_SET, XS_CMD_SCWS_SET_DICT, $mode, $fpath);
		unset($this->_setting['add_dict']);
		return $this;
	}

	/**
	 * 添加分词词典, 支持 TXT/XDB 格式
	 * @param string $fpath 服务端的词典路径
	 * @param int $mode 词典类型, 常量: SCWS_XDICT_XDB|SCWS_XDICT_TXT|SCWS_XDICT_MEM
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function addDict($fpath, $mode = null)
	{
		if (!is_int($mode)) {
			$mode = stripos($fpath, '.txt') !== false ? SCWS_XDICT_TXT : SCWS_XDICT_XDB;
		}
		if (!isset($this->_setting['add_dict'])) {
			$this->_setting['add_dict'] = array();
		}
		$this->_setting['add_dict'][] = new XSCommand(XS_CMD_SEARCH_SCWS_SET, XS_CMD_SCWS_ADD_DICT, $mode, $fpath);
		return $this;
	}

	/**
	 * 设置散字二元组合
	 * @param bool $yes 是否开启散字自动二分组合功能
	 * @return XSTokenizerScws 返回对象本身以支持串接操作
	 */
	public function setDuality($yes = true)
	{
		$this->_setting['duality'] = new XSCommand(XS_CMD_SEARCH_SCWS_SET, XS_CMD_SCWS_SET_DUALITY, $yes === false
							? 0 : 1);
		return $this;
	}

	/**
	 * 获取 scws 版本号
	 * @return string 版本号
	 */
	public function getVersion()
	{
		$cmd = new XSCommand(XS_CMD_SEARCH_SCWS_GET, XS_CMD_SCWS_GET_VERSION);
		$res = self::$_server->execCommand($cmd, XS_CMD_OK_INFO);
		return $res->buf;
	}

	/**
	 * 获取分词结果
	 * @param string $text 待分词的文本
	 * @return array 返回词汇数组, 每个词汇是包含 [off:词在文本中的位置,attr:词性,word:词]
	 */
	public function getResult($text)
	{
		$words = array();
		$text = $this->applySetting($text);
		$cmd = new XSCommand(XS_CMD_SEARCH_SCWS_GET, XS_CMD_SCWS_GET_RESULT, 0, $text);
		$res = self::$_server->execCommand($cmd, XS_CMD_OK_SCWS_RESULT);
		while ($res->buf !== '') {
			$tmp = unpack('Ioff/a4attr/a*word', $res->buf);
			$tmp['word'] = XS::convert($tmp['word'], self::$_charset, 'UTF-8');
			$words[] = $tmp;
			$res = self::$_server->getRespond();
		}
		return $words;
	}

	/**
	 * 获取重要词统计结果
	 * @param string $text 待分词的文本
	 * @param string $xattr 在返回结果的词性过滤, 多个词性之间用逗号分隔, 以~开头取反
	 *  如: 设为 n,v 表示只返回名词和动词; 设为 ~n,v 则表示返回名词和动词以外的其它词 
	 * @return array 返回词汇数组, 每个词汇是包含 [times:次数,attr:词性,word:词]
	 */
	public function getTops($text, $limit = 10, $xattr = '')
	{
		$words = array();
		$text = $this->applySetting($text);
		$cmd = new XSCommand(XS_CMD_SEARCH_SCWS_GET, XS_CMD_SCWS_GET_TOPS, $limit, $text, $xattr);
		$res = self::$_server->execCommand($cmd, XS_CMD_OK_SCWS_TOPS);
		while ($res->buf !== '') {
			$tmp = unpack('Itimes/a4attr/a*word', $res->buf);
			$tmp['word'] = XS::convert($tmp['word'], self::$_charset, 'UTF-8');
			$words[] = $tmp;
			$res = self::$_server->getRespond();
		}
		return $words;
	}

	/**
	 * 判断是否包含指定词性的词
	 * @param string $text 要判断的文本
	 * @param string $xattr 要判断的词性, 参见 {@link getTops} 的说明
	 * @return bool 文本中是否包含指定词性的词汇
	 */
	public function hasWord($text, $xattr)
	{
		$text = $this->applySetting($text);
		$cmd = new XSCommand(XS_CMD_SEARCH_SCWS_GET, XS_CMD_SCWS_HAS_WORD, 0, $text, $xattr);
		$res = self::$_server->execCommand($cmd, XS_CMD_OK_INFO);
		return $res->buf === 'OK';
	}

	private function applySetting($text)
	{
		self::$_server->reopen();
		foreach ($this->_setting as $key => $cmd) {
			if (is_array($cmd)) {
				foreach ($cmd as $_cmd) {
					self::$_server->execCommand($_cmd);
				}
			} else {
				self::$_server->execCommand($cmd);
			}
		}
		return XS::convert($text, 'UTF-8', self::$_charset);
	}
}

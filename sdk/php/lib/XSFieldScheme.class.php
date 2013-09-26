<?php
/**
 * XSFieldScheme 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * XS 数据字段方案
 * 每个方案包含若干个字段结构对象 {@link XSFieldMeta}
 * 每个方案必须并且只能包含一个类型为 ID 的字段, 支持 foreach 遍历所有字段
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSFieldScheme implements IteratorAggregate
{
	const MIXED_VNO = 255;

	private $_fields = array();
	private $_typeMap = array();
	private $_vnoMap = array();
	private static $_logger;

	/**
	 * 将对象转换为配置文件字符串
	 */
	public function __toString()
	{
		$str = '';
		foreach ($this->_fields as $field) {
			$str .= $field->toConfig() . "\n";
		}
		return $str;
	}

	/**
	 * 获取主键字段元数据
	 * @return XSFieldMeta 类型为 ID 的字段
	 */
	public function getFieldId()
	{
		if (isset($this->_typeMap[XSFieldMeta::TYPE_ID])) {
			$name = $this->_typeMap[XSFieldMeta::TYPE_ID];
			return $this->_fields[$name];
		}
		return false;
	}

	/**
	 * 获取标题字段元数据
	 * @return XSFieldMeta 类型为 TITLE 的字段
	 */
	public function getFieldTitle()
	{
		if (isset($this->_typeMap[XSFieldMeta::TYPE_TITLE])) {
			$name = $this->_typeMap[XSFieldMeta::TYPE_TITLE];
			return $this->_fields[$name];
		}
		foreach ($this->_fields as $name => $field) {
			if ($field->type === XSFieldMeta::TYPE_STRING && !$field->isBoolIndex()) {
				return $field;
			}
		}
		return false;
	}

	/**
	 * 获取内容字段元数据
	 * @return XSFieldMeta 类型为 BODY 的字段
	 */
	public function getFieldBody()
	{
		if (isset($this->_typeMap[XSFieldMeta::TYPE_BODY])) {
			$name = $this->_typeMap[XSFieldMeta::TYPE_BODY];
			return $this->_fields[$name];
		}
		return false;
	}

	/**
	 * 获取项目字段元数据
	 * @param mixed $name 字段名称(string) 或字段序号(vno, int)
	 * @param bool $throw 当字段不存在时是否抛出异常, 默认为 true
	 * @return XSFieldMeta 字段元数据对象, 若不存在则返回 false
	 * @throw XSException 当字段不存在并且参数 throw 为 true 时抛出异常
	 */
	public function getField($name, $throw = true)
	{
		if (is_int($name)) {
			if (!isset($this->_vnoMap[$name])) {
				if ($throw === true) {
					throw new XSException('Not exists field with vno: `' . $name . '\'');
				}
				return false;
			}
			$name = $this->_vnoMap[$name];
		}
		if (!isset($this->_fields[$name])) {
			if ($throw === true) {
				throw new XSException('Not exists field with name: `' . $name . '\'');
			}
			return false;
		}
		return $this->_fields[$name];
	}

	/**
	 * 获取项目所有字段结构设置
	 * @return XSFieldMeta[]
	 */
	public function getAllFields()
	{
		return $this->_fields;
	}

	/**
	 * 获取所有字段的vno与名称映映射关系
	 * @return array vno为键, 字段名为值的数组
	 */
	public function getVnoMap()
	{
		return $this->_vnoMap;
	}

	/**
	 * 添加字段到方案中
	 * 每个方案中的特殊类型字段都不能重复出现
	 * @param mixed $field 若类型为 XSFieldMeta 表示要添加的字段对象, 
	 *        若类型为 string 表示字段名称, 连同 $config 参数一起创建字段对象
	 * @param array $config 当 $field 参数为 string 时作为新建字段的配置内容
	 * @throw XSException 出现逻辑错误时抛出异常
	 */
	public function addField($field, $config = null)
	{
		if (!$field instanceof XSFieldMeta) {
			$field = new XSFieldMeta($field, $config);
		}

		if (isset($this->_fields[$field->name])) {
			throw new XSException('Duplicated field name: `' . $field->name . '\'');
		}

		if ($field->isSpeical()) {
			if (isset($this->_typeMap[$field->type])) {
				$prev = $this->_typeMap[$field->type];
				throw new XSException('Duplicated ' . strtoupper($config['type']) . ' field: `' . $field->name . '\' and `' . $prev . '\'');
			}
			$this->_typeMap[$field->type] = $field->name;
		}

		$field->vno = ($field->type == XSFieldMeta::TYPE_BODY) ? self::MIXED_VNO : count($this->_vnoMap);
		$this->_vnoMap[$field->vno] = $field->name;

		// save field, ensure ID is the first field
		if ($field->type == XSFieldMeta::TYPE_ID) {
			$this->_fields = array_merge(array($field->name => $field), $this->_fields);
		} else {
			$this->_fields[$field->name] = $field;
		}
	}

	/**
	 * 判断该字段方案是否有效、可用
	 * 每个方案必须并且只能包含一个类型为 ID 的字段
	 * @param bool $throw 当没有通过检测时是否抛出异常, 默认为 false
	 * @return bool 有效返回 true, 无效则返回 false
	 * @throw XSException 当检测不通过并且参数 throw 为 true 时抛了异常
	 */
	public function checkValid($throw = false)
	{
		if (!isset($this->_typeMap[XSFieldMeta::TYPE_ID])) {
			if ($throw) {
				throw new XSException('Missing field of type ID');
			}
			return false;
		}
		return true;
	}

	/**
	 * IteratorAggregate 接口, 以支持 foreach 遍历访问所有字段
	 */
	public function getIterator()
	{
		return new ArrayIterator($this->_fields);
	}

	/**
	 * 获取搜索日志的字段方案
	 * @return XSFieldScheme 搜索日志字段方案
	 */
	public static function logger()
	{
		if (self::$_logger === null) {
			$scheme = new self;
			$scheme->addField('id', array('type' => 'id'));
			$scheme->addField('pinyin');
			$scheme->addField('partial');
			$scheme->addField('total', array('type' => 'numeric', 'index' => 'self'));
			$scheme->addField('lastnum', array('type' => 'numeric', 'index' => 'self'));
			$scheme->addField('currnum', array('type' => 'numeric', 'index' => 'self'));
			$scheme->addField('currtag', array('type' => 'string'));
			$scheme->addField('body', array('type' => 'body'));
			self::$_logger = $scheme;
		}
		return self::$_logger;
	}
}

/**
 * 数据字段结构元数据
 * 每个搜索项目包含若干个字段, 字段元数据保存在项目的 ini 配置文件中
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 * @see XS::loadIniFile()
 */
class XSFieldMeta
{
	/**
	 * 词条权重最大值
	 */
	const MAX_WDF = 0x3f;
	/**
	 * 字段类型常量定义
	 */
	const TYPE_STRING = 0;
	const TYPE_NUMERIC = 1;
	const TYPE_DATE = 2;
	const TYPE_ID = 10;
	const TYPE_TITLE = 11;
	const TYPE_BODY = 12;
	/**
	 * 索引标志常量定义
	 */
	const FLAG_INDEX_SELF = 0x01;
	const FLAG_INDEX_MIXED = 0x02;
	const FLAG_INDEX_BOTH = 0x03;
	const FLAG_WITH_POSITION = 0x10;
	const FLAG_NON_BOOL = 0x80; // 强制让该字段参与权重计算 (非布尔)

	/**
	 * @var string 字段名称
	 * 理论上支持各种可视字符, 推荐字符范围:[0-9A-Za-z-_], 长度控制在 1~32 字节为宜
	 */
	public $name;

	/**
	 * @var int 剪取长度 (单位:字节)
	 * 用于在返回搜索结果自动剪取较长内容的字段, 默认为 0表示不截取, body 型字段默认为 300 字节
	 */
	public $cutlen = 0;

	/**
	 * @var int 混合区检索时的相对权重
	 * 取值范围: 1~63, title 类型的字段默认为 5, 其它字段默认为 1
	 */
	public $weight = 1;

	/**
	 * @var int 字段类型
	 */
	public $type = 0;

	/**
	 * @var int 字段序号
	 * 取值为 0~255, 同一字段方案内不能重复, 由 {@link XSFieldScheme::addField} 进行确定
	 */
	public $vno = 0;

	/**
	 * @var string 词法分析器
	 */
	private $tokenizer = XSTokenizer::DFL;

	/**
	 * @var integer 索引标志设置
	 */
	private $flag = 0;

	/**
	 * @staticvar XSTokenizer[] 分词器实例缓存
	 */
	private static $_tokenizers = array();

	/**
	 * 构造函数
	 * @param string $name 字段名称
	 * @param array $config 可选参数, 初始化字段各项配置
	 */
	public function __construct($name, $config = null)
	{
		$this->name = strval($name);
		if (is_array($config)) {
			$this->fromConfig($config);
		}
	}

	/**
	 * 将对象转换为字符串
	 * @return string 字段名称
	 */
	public function __toString()
	{
		return $this->name;
	}

	/**
	 * 把给定的值转换为符合这个字段的数据格式
	 * @param mixed $value 原值
	 * @return mixed 转换后的值
	 */
	public function val($value)
	{
		if ($this->type == self::TYPE_DATE) {
			// 日期类型: 转换成专用的 YYYYmmdd 格式
			if (!is_numeric($value) || strlen($value) !== 8) {
				$value = date('Ymd', is_numeric($value) ? $value : strtotime($value));
			}
		}
		return $value;
	}

	/**
	 * 判断当前字段索引是否支持短语搜索
	 * @return bool 是返回 true, 不是返回 false
	 */
	public function withPos()
	{
		return ($this->flag & self::FLAG_WITH_POSITION) ? true : false;
	}

	/**
	 * 判断当前字段的索引是否为布尔型
	 * 目前只有内置分词器支持语法型索引, 自 1.0.1 版本起把非索引字段也视为布尔便于判断
	 * @return bool 是返回 true, 不是返回 false
	 */
	public function isBoolIndex()
	{
		if ($this->flag & self::FLAG_NON_BOOL) {
			return false;
		}
		return (!$this->hasIndex() || $this->tokenizer !== XSTokenizer::DFL);
	}

	/**
	 * 判断当前字段是否为数字型
	 * @return bool 是返回 true, 不是返回 false
	 */
	public function isNumeric()
	{
		return ($this->type == self::TYPE_NUMERIC);
	}

	/**
	 * 判断当前字段是否为特殊类型
	 * 特殊类型的字段是指 id, title, body, 每个项目至多只能有一个这种类型的字段
	 * @return bool 是返回 true, 不是返回 false
	 */
	public function isSpeical()
	{
		return ($this->type == self::TYPE_ID || $this->type == self::TYPE_TITLE || $this->type == self::TYPE_BODY);
	}

	/**
	 * 判断当前字段是否需要索引
	 * @return bool 若需要返回 true, 不需要则返回 false
	 */
	public function hasIndex()
	{
		return ($this->flag & self::FLAG_INDEX_BOTH) ? true : false;
	}

	/**
	 * 判断当前字段是否需要在混合区索引
	 * @return bool 若需要返回 true, 不需要则返回 false
	 */
	public function hasIndexMixed()
	{
		return ($this->flag & self::FLAG_INDEX_MIXED) ? true : false;
	}

	/**
	 * 判断当前字段是否需要在字段区索引
	 * @return bool 若需要返回 true, 不需要则返回 false
	 */
	public function hasIndexSelf()
	{
		return ($this->flag & self::FLAG_INDEX_SELF) ? true : false;
	}

	/**
	 * 判断当前字段是否采用自定义分词器
	 * @return bool 是返回 true, 不是返回 false
	 */
	public function hasCustomTokenizer()
	{
		return ($this->tokenizer !== XSTokenizer::DFL);
	}

	/**
	 * 获取自定义词法分析器
	 * 自 1.4.8 起会自动加载 lib 或当前目录下的 XSTokenizer???.class.php
	 * @return XSTokenizer 获取当前字段的自定义词法分析器
	 * @throw XSException 如果分词器不存在或有出错抛出异常
	 */
	public function getCustomTokenizer()
	{
		if (isset(self::$_tokenizers[$this->tokenizer])) {
			return self::$_tokenizers[$this->tokenizer];
		} else {
			if (($pos1 = strpos($this->tokenizer, '(')) !== false
					&& ($pos2 = strrpos($this->tokenizer, ')', $pos1 + 1))) {
				$name = 'XSTokenizer' . ucfirst(trim(substr($this->tokenizer, 0, $pos1)));
				$arg = substr($this->tokenizer, $pos1 + 1, $pos2 - $pos1 - 1);
			} else {
				$name = 'XSTokenizer' . ucfirst($this->tokenizer);
				$arg = null;
			}
			if (!class_exists($name)) {
				$file = $name . '.class.php';
				if (file_exists($file)) {
					require_once $file;
				} else if (file_exists(XS_LIB_ROOT . DIRECTORY_SEPARATOR . $file)) {
					require_once XS_LIB_ROOT . DIRECTORY_SEPARATOR . $file;
				}
				if (!class_exists($name)) {
					throw new XSException('Undefined custom tokenizer `' . $this->tokenizer . '\' for field `' . $this->name . '\'');
				}
			}

			$obj = $arg === null ? new $name : new $name($arg);
			if (!$obj instanceof XSTokenizer) {
				throw new XSException($name . ' for field `' . $this->name . '\' dose not implement the interface: XSTokenizer');
			}
			self::$_tokenizers[$this->tokenizer] = $obj;
			return $obj;
		}
	}

	/**
	 * 将对象转换为配置文件字符串
	 * @return string 转换后的配置文件字符串
	 */
	public function toConfig()
	{
		// type
		$str = "[" . $this->name . "]\n";
		if ($this->type === self::TYPE_NUMERIC) {
			$str .= "type = numeric\n";
		} elseif ($this->type === self::TYPE_DATE) {
			$str .= "type = date\n";
		} elseif ($this->type === self::TYPE_ID) {
			$str .= "type = id\n";
		} elseif ($this->type === self::TYPE_TITLE) {
			$str .= "type = title\n";
		} elseif ($this->type === self::TYPE_BODY) {
			$str .= "type = body\n";
		}
		// index
		if ($this->type !== self::TYPE_BODY && ($index = ($this->flag & self::FLAG_INDEX_BOTH))) {
			if ($index === self::FLAG_INDEX_BOTH) {
				if ($this->type !== self::TYPE_TITLE) {
					$str .= "index = both\n";
				}
			} elseif ($index === self::FLAG_INDEX_MIXED) {
				$str .= "index = mixed\n";
			} else {
				if ($this->type !== self::TYPE_ID) {
					$str .= "index = self\n";
				}
			}
		}
		// tokenizer
		if ($this->type !== self::TYPE_ID && $this->tokenizer !== XSTokenizer::DFL) {
			$str .= "tokenizer = " . $this->tokenizer . "\n";
		}
		// cutlen
		if ($this->cutlen > 0 && !($this->cutlen === 300 && $this->type === self::TYPE_BODY)) {
			$str .= "cutlen = " . $this->cutlen . "\n";
		}
		// weight
		if ($this->weight !== 1 && !($this->weight === 5 && $this->type === self::TYPE_TITLE)) {
			$str .= "weight = " . $this->weight . "\n";
		}
		// phrase
		if ($this->flag & self::FLAG_WITH_POSITION) {
			if ($this->type !== self::TYPE_BODY && $this->type !== self::TYPE_TITLE) {
				$str .= "phrase = yes\n";
			}
		} else {
			if ($this->type === self::TYPE_BODY || $this->type === self::TYPE_TITLE) {
				$str .= "phrase = no\n";
			}
		}
		// non-bool
		if ($this->flag & self::FLAG_NON_BOOL) {
			$str .= "non_bool = yes\n";
		}
		return $str;
	}

	/**
	 * 解析字段对象属性
	 * @param array $config 原始配置属性数组
	 */
	public function fromConfig($config)
	{
		// type & default setting
		if (isset($config['type'])) {
			$predef = 'self::TYPE_' . strtoupper($config['type']);
			if (defined($predef)) {
				$this->type = constant($predef);
				if ($this->type == self::TYPE_ID) {
					$this->flag = self::FLAG_INDEX_SELF;
					$this->tokenizer = 'full';
				} elseif ($this->type == self::TYPE_TITLE) {
					$this->flag = self::FLAG_INDEX_BOTH | self::FLAG_WITH_POSITION;
					$this->weight = 5;
				} elseif ($this->type == self::TYPE_BODY) {
					$this->vno = XSFieldScheme::MIXED_VNO;
					$this->flag = self::FLAG_INDEX_SELF | self::FLAG_WITH_POSITION;
					$this->cutlen = 300;
				}
			}
		}
		// index flag
		if (isset($config['index']) && $this->type != self::TYPE_BODY) {
			$predef = 'self::FLAG_INDEX_' . strtoupper($config['index']);
			if (defined($predef)) {
				$this->flag &= ~ self::FLAG_INDEX_BOTH;
				$this->flag |= constant($predef);
			}
			if ($this->type == self::TYPE_ID) {
				$this->flag |= self::FLAG_INDEX_SELF;
			}
		}
		// others
		if (isset($config['cutlen'])) {
			$this->cutlen = intval($config['cutlen']);
		}
		if (isset($config['weight']) && $this->type != self::TYPE_BODY) {
			$this->weight = intval($config['weight']) & self::MAX_WDF;
		}
		if (isset($config['phrase'])) {
			if (!strcasecmp($config['phrase'], 'yes')) {
				$this->flag |= self::FLAG_WITH_POSITION;
			} elseif (!strcasecmp($config['phrase'], 'no')) {
				$this->flag &= ~ self::FLAG_WITH_POSITION;
			}
		}
		if (isset($config['non_bool'])) {
			if (!strcasecmp($config['non_bool'], 'yes')) {
				$this->flag |= self::FLAG_NON_BOOL;
			} elseif (!strcasecmp($config['non_bool'], 'no')) {
				$this->flag &= ~ self::FLAG_NON_BOOL;
			}
		}
		if (isset($config['tokenizer']) && $this->type != self::TYPE_ID
				&& $config['tokenizer'] != 'default') {
			$this->tokenizer = $config['tokenizer'];
		}
	}
}

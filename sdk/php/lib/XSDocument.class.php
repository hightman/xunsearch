<?php
/**
 * XSDocument 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 文档用于描述检索/索引的基础对象, 包含一组字段及其值, 相当于常规SQL数据表中的一行记录.
 * 通过魔术方法, 每个字段名都是文档的虚拟属性, 可直接赋值或取值, 也支持数组方式访问文档字段.
 * <pre>
 * $doc = new XSDocument;
 * $doc->name = 'value'; // 用对象属性方式进行赋值、取值
 * $doc['name'] = 'value'; // 用数组下标方式进行赋值、取值
 * $value = $doc->f('name'); // 用函数方式进行取值
 * $doc->setField('name', 'value'); // 用函数方式进行赋值
 * $doc->setFields(array('name' => 'value', 'name2' => 'value2')); // 用数组进行批量赋值
 * 
 * // 迭代方式取所有字段值
 * foreach($doc as $name => $value) 
 * {
 *     echo "$name: $value\n";  
 * } 
 * </pre>
 * 如果有特殊需求, 可以自行扩展本类, 重写 beforeSubmit() 及 afterSubmit() 方法以定义在索引
 * 提交前后的行为
 * 
 * @method int docid() docid(void) 取得搜索结果文档的 docid 值 (实际数据库的id)
 * @method int rank() rank(void) 取得搜索结果文档的序号值 (第X条结果)
 * @method int percent() percent(void) 取得搜索结果文档的匹配百分比 (结果匹配度, 1~100)
 * @method float weight() weight(void) 取得搜索结果文档的权重值 (浮点数)
 * @method int ccount() ccount(void) 取得搜索结果折叠的数量 (按字段折叠搜索时)
 * @method array matched() matched(void) 取得搜索结果文档中匹配查询的词汇 (数组)
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSDocument implements ArrayAccess, IteratorAggregate
{
	private $_data;
	private $_terms, $_texts;
	private $_charset, $_meta;
	private static $_resSize = 20;
	private static $_resFormat = 'Idocid/Irank/Iccount/ipercent/fweight';

	/**
	 * 构造函数
	 * @param mixed $p 字符串表示索引文档的编码或搜索结果文档的 meta 数据, 数组则表示或索引文档的初始字段数据
	 * @param string $d 可选参数, 当 $p 不为编码时, 本参数表示数据编码
	 */
	public function __construct($p = null, $d = null)
	{
		$this->_data = array();
		if (is_array($p)) {
			$this->_data = $p;
		} elseif (is_string($p)) {
			if (strlen($p) !== self::$_resSize) {
				$this->setCharset($p);
				return;
			}
			$this->_meta = unpack(self::$_resFormat, $p);
		}
		if ($d !== null && is_string($d)) {
			$this->setCharset($d);
		}
	}

	/**
	 * 魔术方法 __get
	 * 实现以对象属性方式获取文档字段值
	 * @param string $name 字段名称
	 * @return mixed 字段值, 若不存在返回 null
	 */
	public function __get($name)
	{
		if (!isset($this->_data[$name])) {
			return null;
		}
		return $this->autoConvert($this->_data[$name]);
	}

	/**
	 * 魔术方法 __set
	 * 实现以对象属性方式设置文档字段值
	 * @param string $name 字段名称
	 * @param mixed $value 字段值
	 */
	public function __set($name, $value)
	{
		if ($this->_meta !== null) {
			throw new XSException('Magick property of result document is read-only');
		}
		$this->setField($name, $value);
	}

	/**
	 * 魔术方法 __call
	 * 实现以函数调用访问搜索结果元数据, 支持: docid, rank, percent, weight, ccount
	 * @param string $name 方法名称
	 * @param array $args 调用时的参数列表 (此处无用)
	 * @throw XSException 若不存在相应元数据则抛出方法未定义的异常
	 */
	public function __call($name, $args)
	{
		if ($this->_meta !== null) {
			$name = strtolower($name);
			if (isset($this->_meta[$name])) {
				return $this->_meta[$name];
			}
		}
		throw new XSException('Call to undefined method `' . get_class($this) . '::' . $name . '()\'');
	}

	/**
	 * 获取文档字符集
	 * @return string 当前设定的字符集(已大写), 若未曾设置则返回 null
	 */
	public function getCharset()
	{
		return $this->_charset;
	}

	/**
	 * 设置文档字符集
	 * @param string $charset 设置文档字符集
	 */
	public function setCharset($charset)
	{
		$this->_charset = strtoupper($charset);
		if ($this->_charset == 'UTF8') {
			$this->_charset = 'UTF-8';
		}
	}

	/**
	 * 获取字段值
	 * @return array 返回已设置的字段键值数组
	 */
	public function getFields()
	{
		return $this->_data;
	}

	/**
	 * 批量设置字段值
	 * 这里是以合并方式赋值, 即不会清空已赋值并且不在参数中的字段.
	 * @param array $data 字段名及其值组成的数组
	 */
	public function setFields($data)
	{
		if ($data === null) {
			$this->_data = array();
			$this->_meta = $this->_terms = $this->_texts = null;
		} else {
			$this->_data = array_merge($this->_data, $data);
		}
	}

	/**
	 * 设置某个字段的值
	 * @param string $name 字段名称
	 * @param mixed $value 字段值
	 * @param bool $isMeta 是否为元数据字段
	 */
	public function setField($name, $value, $isMeta = false)
	{
		if ($value === null) {
			if ($isMeta) {
				unset($this->_meta[$name]);
			} else {
				unset($this->_data[$name]);
			}
		} else {
			if ($isMeta) {
				$this->_meta[$name] = $value;
			} else {
				$this->_data[$name] = $value;
			}
		}
	}

	/**
	 * 获取文档字段的值
	 * @param string $name 字段名称
	 * @return mixed 字段值, 若不存在则返回 null
	 */
	public function f($name)
	{
		return $this->__get(strval($name));
	}

	/**
	 * 获取字段的附加索引词列表 (仅限索引文档)
	 * @param string $field 字段名称
	 * @return array 索引词列表(词为键, 词重为值), 若无则返回 null
	 */
	public function getAddTerms($field)
	{
		$field = strval($field);
		if ($this->_terms === null || !isset($this->_terms[$field])) {
			return null;
		}
		$terms = array();
		foreach ($this->_terms[$field] as $term => $weight) {
			$term = $this->autoConvert($term);
			$terms[$term] = $weight;
		}
		return $terms;
	}

	/**
	 * 获取字段的附加索引文本 (仅限索引文档)
	 * @param string $field 字段名称
	 * @return string 文本内容, 若无则返回 null
	 */
	public function getAddIndex($field)
	{
		$field = strval($field);
		if ($this->_texts === null || !isset($this->_texts[$field])) {
			return null;
		}
		return $this->autoConvert($this->_texts[$field]);
	}

	/**
	 * 给字段增加索引词 (仅限索引文档)
	 * @param string $field 词条所属字段名称
	 * @param string $term 词条内容, 不超过 255字节
	 * @param int $weight 词重, 默认为 1
	 */
	public function addTerm($field, $term, $weight = 1)
	{
		$field = strval($field);
		if (!is_array($this->_terms)) {
			$this->_terms = array();
		}
		if (!isset($this->_terms[$field])) {
			$this->_terms[$field] = array($term => $weight);
		} elseif (!isset($this->_terms[$field][$term])) {
			$this->_terms[$field][$term] = $weight;
		} else {
			$this->_terms[$field][$term] += $weight;
		}
	}

	/**
	 * 给字段增加索引文本 (仅限索引文档)
	 * @param string $field 文本所属的字段名称
	 * @param string $text 文本内容
	 */
	public function addIndex($field, $text)
	{
		$field = strval($field);
		if (!is_array($this->_texts)) {
			$this->_texts = array();
		}
		if (!isset($this->_texts[$field])) {
			$this->_texts[$field] = strval($text);
		} else {
			$this->_texts[$field] .= "\n" . strval($text);
		}
	}

	/**
	 * IteratorAggregate 接口, 以支持 foreach 遍历访问字段列表
	 */
	public function getIterator()
	{
		if ($this->_charset !== null && $this->_charset !== 'UTF-8') {
			$from = $this->_meta === null ? $this->_charset : 'UTF-8';
			$to = $this->_meta === null ? 'UTF-8' : $this->_charset;
			return new ArrayIterator(XS::convert($this->_data, $to, $from));
		}
		return new ArrayIterator($this->_data);
	}

	/**
	 * ArrayAccess 接口, 判断字段是否存在, 勿直接调用
	 * @param string $name 字段名称
	 * @return bool 存在返回 true, 若不存在返回 false
	 */
	public function offsetExists($name)
	{
		return isset($this->_data[$name]);
	}

	/**
	 * ArrayAccess 接口, 取得字段值, 勿直接调用
	 * @param string $name 字段名称
	 * @return mixed 字段值, 若不存在返回 null
	 * @see __get
	 */
	public function offsetGet($name)
	{
		return $this->__get($name);
	}

	/**
	 * ArrayAccess 接口, 设置字段值, 勿直接调用
	 * @param string $name 字段名称
	 * @param mixed $value 字段值
	 * @see __set
	 */
	public function offsetSet($name, $value)
	{
		if (!is_null($name)) {
			$this->__set(strval($name), $value);
		}
	}

	/**
	 * ArrayAccess 接口, 删除字段值, 勿直接调用
	 * @param string $name 字段名称
	 */
	public function offsetUnset($name)
	{
		unset($this->_data[$name]);
	}

	/**
	 * 重写接口, 在文档提交到索引服务器前调用
	 * 继承此类进行重写该方法时, 必须调用 parent::beforeSave($index) 以确保正确
	 * @param XSIndex $index 索引操作对象
	 * @return bool 默认返回 true, 若返回 false 将阻止该文档提交到索引服务器
	 */
	public function beforeSubmit(XSIndex $index)
	{
		if ($this->_charset === null) {
			$this->_charset = $index->xs->getDefaultCharset();
		}
		return true;
	}

	/**
	 * 重写接口, 在文档成功提交到索引服务器后调用
	 * 继承此类进行重写该方法时, 强烈建议要调用 parent::afterSave($index) 以确保完整.
	 * @param XSIndex $index 索引操作对象
	 */
	public function afterSubmit($index)
	{
		
	}

	/**
	 * 智能字符集编码转换
	 * 将 XS 内部用的 UTF-8 与指定的文档编码按需相互转换
	 * 索引文档: ... -> UTF-8, 搜索结果文档: ... <-- UTF-8
	 * @param string $value 要转换的字符串	 
	 * @return string 转好的字符串
	 * @see setCharset
	 */
	private function autoConvert($value)
	{
		// Is the value need to convert
		if ($this->_charset === null || $this->_charset == 'UTF-8'
				|| !is_string($value) || !preg_match('/[\x81-\xfe]/', $value)) {
			return $value;
		}

		// _meta === null ? index document : search result document
		$from = $this->_meta === null ? $this->_charset : 'UTF-8';
		$to = $this->_meta === null ? 'UTF-8' : $this->_charset;

		return XS::convert($value, $to, $from);
	}
}

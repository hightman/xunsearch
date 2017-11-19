<?php
/**
 * XSSearch 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * XS 搜索类, 执行搜索功能
 * 有部分方法支持串接操作
 * <pre>
 * $xs->search->setQuery($str)->setLimit(10, 10)->search();
 * $xs->close();
 * </pre>
 *
 * @property string $query 默认搜索语句
 * @property-read int $dbTotal 数据库内的数据总量
 * @property-read int $lastCount 最近那次搜索的匹配总量估值
 * @property-read array $hotQuerys 热门搜索词列表
 * @property-read array $relatedQuerys 相关搜索词列表
 * @property-read array $expandedQuerys 展开前缀的搜索词列表
 * @property-read array $corredtedQuerys 修正后的建议搜索词列表
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSSearch extends XSServer
{
	/**
	 * 搜索结果默认分页数量
	 */
	const PAGE_SIZE = 10;
	const LOG_DB = 'log_db';

	private $_defaultOp = XS_CMD_QUERY_OP_AND;
	private $_prefix, $_fieldSet, $_resetScheme = false;
	private $_query, $_terms, $_count;
	private $_lastCount, $_highlight;
	private $_curDb, $_curDbs = array();
	private $_lastDb, $_lastDbs = array();
	private $_facets = array();
	private $_limit = 0, $_offset = 0;
	private $_charset = 'UTF-8';

	/**
	 * 连接搜索服务端并初始化
	 * 每次重新连接后所有的搜索语句相关设置均被还原
	 * @param string $conn
	 * @see XSServer::open
	 */
	public function open($conn)
	{
		parent::open($conn);
		$this->_prefix = array();
		$this->_fieldSet = false;
		$this->_lastCount = false;
	}

	/**
	 * 设置字符集
	 * 默认字符集是 UTF-8, 如果您提交的搜索语句和预期得到的搜索结果为其它字符集, 请先设置
	 * @param string $charset
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setCharset($charset)
	{
		$this->_charset = strtoupper($charset);
		if ($this->_charset == 'UTF8') {
			$this->_charset = 'UTF-8';
		}
		return $this;
	}

	/**
	 * 开启模糊搜索
	 * 默认情况只返回包含所有搜索词的记录, 通过本方法可以获得更多搜索结果
	 * @param bool $value 设为 true 表示开启模糊搜索, 设为 false 关闭模糊搜索
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setFuzzy($value = true)
	{
		$this->_defaultOp = $value === true ? XS_CMD_QUERY_OP_OR : XS_CMD_QUERY_OP_AND;
		return $this;
	}

	/**
	 * 设置百分比/权重剔除参数
	 * 通常是在开启 {@link setFuzzy} 或使用 OR 连接搜索语句时才需要设置此项
	 * @param int $percent 剔除匹配百分比低于此值的文档, 值范围 0-100
	 * @param float $weight 剔除权重低于此值的文档, 值范围 0.1-25.5, 0 表示不剔除
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @see setFuzzy
	 */
	public function setCutOff($percent, $weight = 0)
	{
		$percent = max(0, min(100, intval($percent)));
		$weight = max(0, (intval($weight * 10) & 255));
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_CUTOFF, $percent, $weight);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置在搜索结果文档中返回匹配词表
	 * 请在 {@link search} 前调用本方法, 然后使用 {@link XSDocument::matched} 获取
	 * @param bool $value 设为 true 表示开启返回, 设为 false 关闭该功能, 默认是不开启
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.4.8
	 */
	public function setRequireMatchedTerm($value = true)
	{
		$arg1 = XS_CMD_SEARCH_MISC_MATCHED_TERM;
		$arg2 = $value === true ? 1 : 0;
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_MISC, $arg1, $arg2);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置检索匹配的权重方案
	 * 目前支持三种权重方案: 0=BM25/1=Bool/2=Trad
	 * @param int $scheme 匹配权重方案
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.4.11
	 */
	public function setWeightingScheme($scheme) {
		$arg1 = XS_CMD_SEARCH_MISC_WEIGHT_SCHEME;
		$arg2 = intval($scheme);
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_MISC, $arg1, $arg2);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 开启自动同义词搜索功能
	 * @param bool $value 设为 true 表示开启同义词功能, 设为 false 关闭同义词功能
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.3.0
	 */
	public function setAutoSynonyms($value = true)
	{
		$flag = XS_CMD_PARSE_FLAG_BOOLEAN | XS_CMD_PARSE_FLAG_PHRASE | XS_CMD_PARSE_FLAG_LOVEHATE;
		if ($value === true) {
			$flag |= XS_CMD_PARSE_FLAG_AUTO_MULTIWORD_SYNONYMS;
		}
		$cmd = array('cmd' => XS_CMD_QUERY_PARSEFLAG, 'arg' => $flag);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置同义词搜索的权重比例
	 * @param float $value 取值范围 0.01-2.55, 1 表示不调整
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @notice scws 的复合分词也是以同义词方式呈现的
	 * @since 1.4.7
	 */
	public function setSynonymScale($value)
	{
		$arg1 = XS_CMD_SEARCH_MISC_SYN_SCALE;
		$arg2 = max(0, (intval($value * 100) & 255));
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_MISC, $arg1, $arg2);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 获取当前库内的全部同义词列表
	 * @param int $limit 数量上限, 若设为 0 则启用默认值 100 个
	 * @param int $offset 偏移量, 即跳过的结果数量, 默认为 0
	 * @param bool $stemmed 是否包含处理过的词根同义词, 默认为 false 表示否
	 * @return array 同义词记录数组, 每个词条为键, 同义词条组成的数组为值
	 * @since 1.3.0
	 */
	public function getAllSynonyms($limit = 0, $offset = 0, $stemmed = false)
	{
		$page = $limit > 0 ? pack('II', intval($offset), intval($limit)) : '';
		$cmd = array('cmd' => XS_CMD_SEARCH_GET_SYNONYMS, 'buf1' => $page);
		$cmd['arg1'] = $stemmed == true ? 1 : 0;
		$res = $this->execCommand($cmd, XS_CMD_OK_RESULT_SYNONYMS);
		$ret = array();
		if (!empty($res->buf)) {
			foreach (explode("\n", $res->buf) as $line) {
				$value = explode("\t", $line);
				$key = array_shift($value);
				$ret[$key] = $value;
			}
		}
		return $ret;
	}

	/**
	 * 获取指定词汇的同义词列表
	 * @param string $term 要查询同义词的原词
	 * @return array 同义词记录数组, 不存在同义词则返回空数组
	 * @since 1.4.9
	 */
	public function getSynonyms($term)
	{
		$term = strval($term);
		if (strlen($term) === 0) {
			return false;
		}
		$cmd = array('cmd' => XS_CMD_SEARCH_GET_SYNONYMS, 'arg1' => 2, 'buf' => $term);
		$res = $this->execCommand($cmd, XS_CMD_OK_RESULT_SYNONYMS);
		$ret = $res->buf === '' ? array() : explode("\n", $res->buf);
		return $ret;
	}

	/**
	 * 获取解析后的搜索语句
	 * @param string $query 搜索语句, 若传入 null 使用默认语句
	 * @return string 返回解析后的搜索语句
	 */
	public function getQuery($query = null)
	{
		$query = $query === null ? '' : $this->preQueryString($query);
		$cmd = new XSCommand(XS_CMD_QUERY_GET_STRING, 0, $this->_defaultOp, $query);
		$res = $this->execCommand($cmd, XS_CMD_OK_QUERY_STRING);
		if (strpos($res->buf, 'VALUE_RANGE') !== false) {
			$regex = '/(VALUE_RANGE) (\d+) (\S+) (.+?)(?=\))/';
			$res->buf = preg_replace_callback($regex, array($this, 'formatValueRange'), $res->buf);
		}
		if (strpos($res->buf, 'VALUE_GE') !== false || strpos($res->buf, 'VALUE_LE') !== false) {
			$regex = '/(VALUE_[GL]E) (\d+) (.+?)(?=\))/';
			$res->buf = preg_replace_callback($regex, array($this, 'formatValueRange'), $res->buf);
		}
		return XS::convert($res->buf, $this->_charset, 'UTF-8');
	}

	/**
	 * 设置默认搜索语句
	 * 用于不带参数的 {@link count} 或 {@link search} 以及 {@link terms} 调用
	 * 可与 {@link addWeight} 组合运用
	 * @param string $query 搜索语句, 设为 null 则清空搜索语句, 最大长度为 80 字节
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setQuery($query)
	{
		$this->clearQuery();
		if ($query !== null) {
			$this->_query = $query;
			$this->addQueryString($query);
		}
		return $this;
	}

	/**
	 * 设置地理位置距离排序方式
	 *
	 * 请务必先以 numeric 类型字段定义经纬度坐标字段，例如用 lon 代表经度、lat 代表纬度，
	 * 那么设置排序代码如下，必须将经度定义在前纬度在后：
	 * <pre>
	 * $search->setGeodistSort(array('lon' => 39.18, 'lat' => 120.51));
	 * </pre>
	 * @param array $fields 在此定义地理位置信息原点坐标信息，数组至少必须包含2个值
	 * @param bool $reverse 是否由远及近排序, 默认为由近及远
	 * @param bool $relevance_first 是否优先相关性排序, 默认为否
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.4.10
	 */
	public function setGeodistSort($fields, $reverse = false, $relevance_first = false)
	{
		if (!is_array($fields) || count($fields) < 2) {
			throw new XSException("Fields of `setGeodistSort' should be an array contain two or more elements");
		}
		// [vno][vlen][vbuf] ...
		$buf = '';
		foreach ($fields as $key => $value) {
			$field = $this->xs->getField($key, true);
			if (!$field->isNumeric()) {
				throw new XSException("Type of GeoField `$key' shoud be numeric");
			}
			$vno = $field->vno;
			$vbuf = strval(floatval($value));
			$vlen = strlen($vbuf);
			if ($vlen >= 255) {
				throw new XSException("Value of `$key' too long");
			}
			$buf .= chr($vno) . chr($vlen) . $vbuf;
		}
		$type = XS_CMD_SORT_TYPE_GEODIST;
		if ($relevance_first) {
			$type |= XS_CMD_SORT_FLAG_RELEVANCE;
		}
		if (!$reverse) {
			$type |= XS_CMD_SORT_FLAG_ASCENDING;
		}
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_SORT, $type, 0, $buf);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置多字段组合排序方式
	 * 当您需要根据多个字段的值按不同的方式综合排序时, 请使用这项
	 * @param array $fields 排序依据的字段数组, 以字段名称为键, true/false 为值表示正序或逆序
	 * @param bool $reverse 是否为倒序显示, 默认为正向, 此处和 {@link setSort} 略有不同
	 * @param bool $relevance_first 是否优先相关性排序, 默认为否
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.1.0
	 */
	public function setMultiSort($fields, $reverse = false, $relevance_first = false)
	{
		if (!is_array($fields)) {
			return $this->setSort($fields, !$reverse, $relevance_first);
		}

		// [vno][0/1] (0:reverse,1:asc)
		$buf = '';
		foreach ($fields as $key => $value) {
			if (is_bool($value)) {
				$vno = $this->xs->getField($key, true)->vno;
				$asc = $value;
			} else {
				$vno = $this->xs->getField($value, true)->vno;
				$asc = false;
			}
			if ($vno != XSFieldScheme::MIXED_VNO) {
				$buf .= chr($vno) . chr($asc ? 1 : 0);
			}
		}
		if ($buf !== '') {
			$type = XS_CMD_SORT_TYPE_MULTI;
			if ($relevance_first) {
				$type |= XS_CMD_SORT_FLAG_RELEVANCE;
			}
			if (!$reverse) {
				$type |= XS_CMD_SORT_FLAG_ASCENDING;
			}
			$cmd = new XSCommand(XS_CMD_SEARCH_SET_SORT, $type, 0, $buf);
			$this->execCommand($cmd);
		}
		return $this;
	}

	/**
	 * 设置搜索结果的排序方式
	 * 注意, 每当调用 {@link setDb} 或 {@link addDb} 修改当前数据库时会重置排序设定
	 * 此函数第一参数的用法与 {@link setMultiSort} 兼容, 即也可以用该方法实现多字段排序
	 * @param string $field 依据指定字段的值排序, 设为 null 则用默认顺序
	 * @param bool $asc 是否为正序排列, 即从小到大, 从少到多, 默认为反序
	 * @param bool $relevance_first 是否优先相关性排序, 默认为否
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setSort($field, $asc = false, $relevance_first = false)
	{
		if (is_array($field)) {
			return $this->setMultiSort($field, $asc, $relevance_first);
		}
		if ($field === null) {
			$cmd = new XSCommand(XS_CMD_SEARCH_SET_SORT, XS_CMD_SORT_TYPE_RELEVANCE);
		} else {
			$type = XS_CMD_SORT_TYPE_VALUE;
			if ($relevance_first) {
				$type |= XS_CMD_SORT_FLAG_RELEVANCE;
			}
			if ($asc) {
				$type |= XS_CMD_SORT_FLAG_ASCENDING;
			}
			$vno = $this->xs->getField($field, true)->vno;
			$cmd = new XSCommand(XS_CMD_SEARCH_SET_SORT, $type, $vno);
		}
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置结果按索引入库先后排序
	 * 注意, 此项排序不影响相关排序, 权重高的仍会在前面, 主要适合用于布尔检索
	 * @param bool $asc 是否为正序排列, 即从先到后, 默认为反序
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setDocOrder($asc = false)
	{
		$type = XS_CMD_SORT_TYPE_DOCID | ($asc ? XS_CMD_SORT_FLAG_ASCENDING : 0);
		$cmd = new XSCommand(XS_CMD_SEARCH_SET_SORT, $type);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 设置折叠搜索结果
	 * 注意, 每当调用 {@link setDb} 或 {@link addDb} 修改当前数据库时会重置此项设置
	 * @param string $field 依据该字段的值折叠搜索结果, 设为 null 则取消折叠
	 * @param int $num 折叠后只是返最匹配的数据数量, 默认为 1, 最大值 255
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setCollapse($field, $num = 1)
	{
		$vno = $field === null ? XSFieldScheme::MIXED_VNO : $this->xs->getField($field, true)->vno;
		$max = min(255, intval($num));

		$cmd = new XSCommand(XS_CMD_SEARCH_SET_COLLAPSE, $max, $vno);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 添加搜索过滤区间或范围
	 * @param string $field
	 * @param mixed $from 起始值(不包含), 若设为 null 则相当于匹配 <= to (字典顺序)
	 * @param mixed $to 结束值(包含), 若设为 null 则相当于匹配 >= from (字典顺序)
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function addRange($field, $from, $to)
	{
		if ($from === '' || $from === false) {
			$from = null;
		}
		if ($to === '' || $to === false) {
			$to = null;
		}
		if ($from !== null || $to !== null) {
			if (strlen($from) > 255 || strlen($to) > 255) {
				throw new XSException('Value of range is too long');
			}

			$vno = $this->xs->getField($field)->vno;
			$from = XS::convert($from, 'UTF-8', $this->_charset);
			$to = XS::convert($to, 'UTF-8', $this->_charset);
			if ($from === null) {
				$cmd = new XSCommand(XS_CMD_QUERY_VALCMP, XS_CMD_QUERY_OP_FILTER, $vno, $to, chr(XS_CMD_VALCMP_LE));
			} elseif ($to === null) {
				$cmd = new XSCommand(XS_CMD_QUERY_VALCMP, XS_CMD_QUERY_OP_FILTER, $vno, $from, chr(XS_CMD_VALCMP_GE));
			} else {
				$cmd = new XSCommand(XS_CMD_QUERY_RANGE, XS_CMD_QUERY_OP_FILTER, $vno, $from, $to);
			}
			$this->execCommand($cmd);
		}
		return $this;
	}

	/**
	 * 添加权重索引词
	 * 无论是否包含这种词都不影响搜索匹配, 但会参与计算结果权重, 使结果的相关度更高
	 * @param string $field 索引词所属的字段
	 * @param string $term 索引词
	 * @param float $weight 权重计算缩放比例
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @see addQueryTerm
	 */
	public function addWeight($field, $term, $weight = 1)
	{
		return $this->addQueryTerm($field, $term, XS_CMD_QUERY_OP_AND_MAYBE, $weight);
	}

	/**
	 * 设置分面搜索记数
	 * 用于记录匹配搜索结果中按字段值分组的数量统计, 每次调用 {@link search} 后会还原设置
	 * 对于多次调用 $exact 参数以最后一次为准, 只支持字段值不超过 255 字节的情况
	 *
	 * 自 v1.4.10 起自动对空值的字段按 term 分面统计（相当于多值）
	 * @param mixed $field 要进行分组统计的字段或字段组成的数组, 最多同时支持 8 个
	 * @param bool $exact 是否要求绝对精确搜索, 这会造成较大的系统开销
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @throw XSException 在非字符串字段建立分面搜索会抛出异常
	 * @since 1.1.0
	 */
	public function setFacets($field, $exact = false)
	{
		$buf = '';
		if (!is_array($field)) {
			$field = array($field);
		}
		foreach ($field as $name) {
			$ff = $this->xs->getField($name);
			if ($ff->type !== XSFieldMeta::TYPE_STRING) {
				throw new XSException("Field `$name' cann't be used for facets search, can only be string type");
			}
			$buf .= chr($ff->vno);
		}
		$cmd = array('cmd' => XS_CMD_SEARCH_SET_FACETS, 'buf' => $buf);
		$cmd['arg1'] = $exact === true ? 1 : 0;
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 读取最近一次分面搜索记数
	 * 必须在某一次 {@link search} 之后调用本函数才有意义
	 * @param string $field 读取分面记数的字段, 若为 null 则返回全部分面搜索记录
	 * @return array 返回由值和计数组成的关联数组, 若不存在或未曾登记过则返回空数组
	 * @since 1.1.0
	 */
	public function getFacets($field = null)
	{
		if ($field === null) {
			return $this->_facets;
		}
		return isset($this->_facets[$field]) ? $this->_facets[$field] : array();
	}

	/**
	 * 设置当前搜索语句的分词复合等级
	 * 复合等级是 scws 分词粒度控制的一个重要参数, 是长词细分处理依据, 默认为 3, 值范围 0~15
	 * 注意: 这个设置仅直对本次搜索有效, 仅对设置之后的 {@link setQuery} 起作用, 由于 query
	 * 设计的方式问题, 目前无法支持搜索语句单字切分, 但您可以在模糊检索时设为 0 来关闭复合分词
	 * @param int $level 要设置的分词复合等级
	 * @return XSSearch 返回自身对象以支持串接操作
	 * @since 1.4.7
	 */
	public function setScwsMulti($level)
	{
		$level = intval($level);
		if ($level >= 0 && $level < 16) {
			$cmd = array('cmd' => XS_CMD_SEARCH_SCWS_SET, 'arg1' => XS_CMD_SCWS_SET_MULTI, 'arg2' => $level);
			$this->execCommand($cmd);
		}
		return $this;
	}

	/**
	 * 设置搜索结果的数量和偏移
	 * 用于搜索结果分页, 每次调用 {@link search} 后会还原这2个变量到初始值
	 * @param int $limit 数量上限, 若设为 0 则启用默认值 self::PAGE_SIZE
	 * @param int $offset 偏移量, 即跳过的结果数量, 默认为 0
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setLimit($limit, $offset = 0)
	{
		$this->_limit = intval($limit);
		$this->_offset = intval($offset);
		return $this;
	}

	/**
	 * 设置要搜索的数据库名
	 * 若未设置, 使用默认数据库, 数据库必须位于服务端用户目录下
	 * 对于远程数据库, 请使用 stub 文件来支持
	 * @param string $name
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setDb($name)
	{
		$name = strval($name);
		$this->execCommand(array('cmd' => XS_CMD_SEARCH_SET_DB, 'buf' => strval($name)));
		$this->_lastDb = $this->_curDb;
		$this->_lastDbs = $this->_curDbs;
		$this->_curDb = $name;
		$this->_curDbs = array();
		return $this;
	}

	/**
	 * 添加搜索的数据库名, 支持多库同时搜索
	 * @param string $name
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @see setDb
	 */
	public function addDb($name)
	{
		$name = strval($name);
		$this->execCommand(array('cmd' => XS_CMD_SEARCH_ADD_DB, 'buf' => $name));
		$this->_curDbs[] = $name;
		return $this;
	}

	/**
	 * 标记字段方案重置
	 * @see XS::setScheme
	 */
	public function markResetScheme()
	{
		$this->_resetScheme = true;
	}

	/**
	 * 获取搜索语句中的高亮词条列表
	 * @param string $query 搜索语句, 若传入 null 使用默认语句, 最大长度为 80 字节
	 * @param bool $convert 是否进行编码转换, 默认为 true
	 * @return array 可用于高亮显示的词条列表
	 */
	public function terms($query = null, $convert = true)
	{
		$query = $query === null ? '' : $this->preQueryString($query);

		if ($query === '' && $this->_terms !== null) {
			$ret = $this->_terms;
		} else {
			$cmd = new XSCommand(XS_CMD_QUERY_GET_TERMS, 0, $this->_defaultOp, $query);
			$res = $this->execCommand($cmd, XS_CMD_OK_QUERY_TERMS);
			$ret = array();

			$tmps = explode(' ', $res->buf);
			for ($i = 0; $i < count($tmps); $i++) {
				if ($tmps[$i] === '' || strpos($tmps[$i], ':') !== false) {
					continue;
				}
				$ret[] = $tmps[$i];
			}
			if ($query === '') {
				$this->_terms = $ret;
			}
		}
		return $convert ? XS::convert($ret, $this->_charset, 'UTF-8') : $ret;
	}

	/**
	 * 估算搜索语句的匹配数据量
	 * @param string $query 搜索语句, 若传入 null 使用默认语句, 调用后会还原默认排序方式
	 *        如果搜索语句和最近一次 {@link search} 的语句一样, 请改用 {@link getLastCount} 以提升效率
	 *        最大长度为 80 字节
	 * @return int 匹配的搜索结果数量, 估算数值
	 */
	public function count($query = null)
	{
		$query = $query === null ? '' : $this->preQueryString($query);
		if ($query === '' && $this->_count !== null) {
			return $this->_count;
		}

		$cmd = new XSCommand(XS_CMD_SEARCH_GET_TOTAL, 0, $this->_defaultOp, $query);
		$res = $this->execCommand($cmd, XS_CMD_OK_SEARCH_TOTAL);
		$ret = unpack('Icount', $res->buf);

		if ($query === '') {
			$this->_count = $ret['count'];
		}
		return $ret['count'];
	}

	/**
	 * 获取匹配的搜索结果文档
	 * 默认提取最匹配的前 self::PAGE_SIZE 个结果
	 * 如需分页请参见 {@link setLimit} 设置, 每次调用本函数后都会还原 setLimit 的设置
	 * @param string $query 搜索语句, 若传入 null 使用默认语句, 最大长度为 80 字节
	 * @param boolean $saveHighlight 是否存储查询词用于高亮处理, 默认为 true
	 * @return XSDocument[] 匹配的搜索结果文档列表
	 */
	public function search($query = null, $saveHighlight = true)
	{
		if ($this->_curDb !== self::LOG_DB && $saveHighlight) {
			$this->_highlight = $query;
		}
		$query = $query === null ? '' : $this->preQueryString($query);
		$page = pack('II', $this->_offset, $this->_limit > 0 ? $this->_limit : self::PAGE_SIZE);

		// get result header
		$cmd = new XSCommand(XS_CMD_SEARCH_GET_RESULT, 0, $this->_defaultOp, $query, $page);
		$res = $this->execCommand($cmd, XS_CMD_OK_RESULT_BEGIN);
		$tmp = unpack('Icount', $res->buf);
		$this->_lastCount = $tmp['count'];

		// load vno map to name of fields
		$ret = $this->_facets = array();
		$vnoes = $this->xs->getScheme()->getVnoMap();

		// get result documents
		while (true) {
			$res = $this->getRespond();
			if ($res->cmd == XS_CMD_SEARCH_RESULT_FACETS) {
				$off = 0;
				while (($off + 6) < strlen($res->buf)) {
					$tmp = unpack('Cvno/Cvlen/Inum', substr($res->buf, $off, 6));
					if (isset($vnoes[$tmp['vno']])) {
						$name = $vnoes[$tmp['vno']];
						$value = substr($res->buf, $off + 6, $tmp['vlen']);
						if (!isset($this->_facets[$name])) {
							$this->_facets[$name] = array();
						}
						$this->_facets[$name][$value] = $tmp['num'];
					}
					$off += $tmp['vlen'] + 6;
				}
			} elseif ($res->cmd == XS_CMD_SEARCH_RESULT_DOC) {
				// got new doc
				$doc = new XSDocument($res->buf, $this->_charset);
				$ret[] = $doc;
			} elseif ($res->cmd == XS_CMD_SEARCH_RESULT_FIELD) {
				// fields of doc
				if (isset($doc)) {
					$name = isset($vnoes[$res->arg]) ? $vnoes[$res->arg] : $res->arg;
					$doc->setField($name, $res->buf);
				}
			} elseif ($res->cmd == XS_CMD_SEARCH_RESULT_MATCHED) {
				// matched terms
				if (isset($doc)) {
					$doc->setField('matched', explode(' ', $res->buf), true);
				}
			} elseif ($res->cmd == XS_CMD_OK && $res->arg == XS_CMD_OK_RESULT_END) {
				// got the end
				break;
			} else {
				$msg = 'Unexpected respond in search {CMD:' . $res->cmd . ', ARG:' . $res->arg . '}';
				throw new XSException($msg);
			}
		}

		if ($query === '') {
			$this->_count = $this->_lastCount;
			// trigger log & highlight
			if ($this->_curDb !== self::LOG_DB) {
				$this->logQuery();
				if ($saveHighlight) {
					$this->initHighlight();
				}
			}
		}
		$this->_limit = $this->_offset = 0;
		return $ret;
	}

	/**
	 * 获取最近那次搜索的匹配总数估值
	 * @return int 匹配数据量, 如从未搜索则返回 false
	 * @see search
	 */
	public function getLastCount()
	{
		return $this->_lastCount;
	}

	/**
	 * 获取搜索数据库内的数据总量
	 * @return int 数据总量
	 */
	public function getDbTotal()
	{
		$cmd = new XSCommand(XS_CMD_SEARCH_DB_TOTAL);
		$res = $this->execCommand($cmd, XS_CMD_OK_DB_TOTAL);
		$tmp = unpack('Itotal', $res->buf);
		return $tmp['total'];
	}

	/**
	 * 获取热门搜索词列表
	 * @param int $limit 需要返回的热门搜索数量上限, 默认为 6, 最大值为 50
	 * @param string $type 排序类型, 默认为 total(搜索总量), 可选值还有 lastnum(上周), currnum(本周)
	 * @return array 返回以搜索词为键, 搜索指数为值的关联数组
	 */
	public function getHotQuery($limit = 6, $type = 'total')
	{
		$ret = array();
		$limit = max(1, min(50, intval($limit)));

		// query from log_db
		$this->xs->setScheme(XSFieldScheme::logger());
		try {
			$this->setDb(self::LOG_DB)->setLimit($limit);
			if ($type !== 'lastnum' && $type !== 'currnum') {
				$type = 'total';
			}
			$result = $this->search($type . ':1');
			foreach ($result as $doc) /* @var $doc XSDocument */ {
				$body = $doc->body;
				$ret[$body] = $doc->f($type);
			}
			$this->restoreDb();
		} catch (XSException $e) {
			if ($e->getCode() != XS_CMD_ERR_XAPIAN) {
				throw $e;
			}
		}
		$this->xs->restoreScheme();

		return $ret;
	}

	/**
	 * 获取相关搜索词列表
	 * @param string $query 搜索语句, 若传入 null 使用默认语句
	 * @param int $limit 需要返回的相关搜索数量上限, 默认为 6, 最大值为 20
	 * @return array 返回搜索词组成的数组
	 */
	public function getRelatedQuery($query = null, $limit = 6)
	{
		$ret = array();
		$limit = max(1, min(20, intval($limit)));

		// Simple to disable query with field filter
		if ($query === null) {
			$query = $this->cleanFieldQuery($this->_query);
		}

		if (empty($query) || strpos($query, ':') !== false) {
			return $ret;
		}

		// Search the log database
		$op = $this->_defaultOp;
		$this->xs->setScheme(XSFieldScheme::logger());
		try {
			$result = $this->setDb(self::LOG_DB)->setFuzzy()->setLimit($limit + 1)->search($query);
			foreach ($result as $doc) /* @var $doc XSDocument */ {
				$doc->setCharset($this->_charset);
				$body = $doc->body;
				if (!strcasecmp($body, $query)) {
					continue;
				}
				$ret[] = $body;
				if (count($ret) == $limit) {
					break;
				}
			}
		} catch (XSException $e) {
			if ($e->getCode() != XS_CMD_ERR_XAPIAN) {
				throw $e;
			}
		}
		$this->restoreDb();
		$this->xs->restoreScheme();
		$this->_defaultOp = $op;

		return $ret;
	}

	/**
	 * 获取展开的搜索词列表
	 * @param string $query 需要展开的前缀, 可为拼音、英文、中文
	 * @param int $limit 需要返回的搜索词数量上限, 默认为 10, 最大值为 20
	 * @return array 返回搜索词组成的数组
	 */
	public function getExpandedQuery($query, $limit = 10)
	{
		$ret = array();
		$limit = max(1, min(20, intval($limit)));

		try {
			$buf = XS::convert($query, 'UTF-8', $this->_charset);
			$cmd = array('cmd' => XS_CMD_QUERY_GET_EXPANDED, 'arg1' => $limit, 'buf' => $buf);
			$res = $this->execCommand($cmd, XS_CMD_OK_RESULT_BEGIN);

			// echo "Raw Query: " . $res->buf . "\n";
			// get result documents
			while (true) {
				$res = $this->getRespond();
				if ($res->cmd == XS_CMD_SEARCH_RESULT_FIELD) {
					$ret[] = XS::convert($res->buf, $this->_charset, 'UTF-8');
				} elseif ($res->cmd == XS_CMD_OK && $res->arg == XS_CMD_OK_RESULT_END) {
					// got the end
					// echo "Parsed Query: " . $res->buf . "\n";
					break;
				} else {
					$msg = 'Unexpected respond in search {CMD:' . $res->cmd . ', ARG:' . $res->arg . '}';
					throw new XSException($msg);
				}
			}
		} catch (XSException $e) {
			if ($e->getCode() != XS_CMD_ERR_XAPIAN) {
				throw $e;
			}
		}

		return $ret;
	}

	/**
	 * 获取修正后的搜索词列表
	 * 通常当某次检索结果数量偏少时, 可以用该函数设计 "你是不是要找: ..." 功能
	 * @param string $query 需要展开的前缀, 可为拼音、英文、中文
	 * @return array 返回搜索词组成的数组
	 */
	public function getCorrectedQuery($query = null)
	{
		$ret = array();

		try {
			if ($query === null) {
				if ($this->_count > 0 && $this->_count > ceil($this->getDbTotal() * 0.001)) {
					return $ret;
				}
				$query = $this->cleanFieldQuery($this->_query);
			}
			if (empty($query) || strpos($query, ':') !== false) {
				return $ret;
			}
			$buf = XS::convert($query, 'UTF-8', $this->_charset);
			$cmd = array('cmd' => XS_CMD_QUERY_GET_CORRECTED, 'buf' => $buf);
			$res = $this->execCommand($cmd, XS_CMD_OK_QUERY_CORRECTED);
			if ($res->buf !== '') {
				$ret = explode("\n", XS::convert($res->buf, $this->_charset, 'UTF-8'));
			}
		} catch (XSException $e) {
			if ($e->getCode() != XS_CMD_ERR_XAPIAN) {
				throw $e;
			}
		}

		return $ret;
	}

	/**
	 * 添加搜索日志关键词到缓冲区里
	 * 需要调用 {@link XSIndex::flushLogging} 才能确保立即刷新, 否则要隔一段时间
	 * @param string $query 需要记录的数据
	 * @param int $wdf 需要记录的次数, 默认为 1
	 * @since 1.1.1
	 */
	public function addSearchLog($query, $wdf = 1)
	{
		$cmd = array('cmd' => XS_CMD_SEARCH_ADD_LOG, 'buf' => $query);
		if ($wdf > 1) {
			$cmd['buf1'] = pack('i', $wdf);
		}
		$this->execCommand($cmd, XS_CMD_OK_LOGGED);
	}

	/**
	 * 搜索结果字符串高亮处理
	 * 对搜索结果文档的字段进行高亮、飘红处理, 高亮部分加上 em 标记
	 * @param string $value 需要处理的数据
	 * @return string 高亮后的数据
	 */
	public function highlight($value, $strtr = false)
	{
		// return empty value directly
		if (empty($value)) {
			return $value;
		}

		// initlize the highlight replacements
		if (!is_array($this->_highlight)) {
			$this->initHighlight();
		}

		// process replace
		if (isset($this->_highlight['pattern'])) {
			$value = preg_replace($this->_highlight['pattern'], $this->_highlight['replace'], $value);
		}
		if (isset($this->_highlight['pairs'])) {
			$value = $strtr ?
				strtr($value, $this->_highlight['pairs']) :
				str_replace(array_keys($this->_highlight['pairs']), array_values($this->_highlight['pairs']), $value);
		}
		return $value;
	}

	/**
	 * 记录搜索语句
	 * 主要是用于相关搜索, 修正搜索等功能, 为避免记录一些杂乱无用的搜索信息,
	 * 系统会先检测这条语句是否符合记录需求, 力争记录一些规范清洁的数据
	 * @param string $query 用于记录的搜索词
	 */
	private function logQuery($query = null)
	{
		if ($this->isRobotAgent()) {
			return;
		}
		if ($query !== '' && $query !== null) {
			$terms = $this->terms($query, false);
		} else {
			// 无结果、包含 OR、XOR、NOT/-、默认 fuzzy
			$query = $this->_query;
			if (!$this->_lastCount || ($this->_defaultOp == XS_CMD_QUERY_OP_OR && strpos($query, ' '))
				|| strpos($query, ' OR ') || strpos($query, ' NOT ') || strpos($query, ' XOR ')) {
				return;
			}
			$terms = $this->terms(null, false);
		}
		// purify the query statement to log
		$log = '';
		$pos = $max = 0;
		foreach ($terms as $term) {
			$pos1 = ($pos > 3 && strlen($term) === 6) ? $pos - 3 : $pos;
			if (($pos2 = strpos($query, $term, $pos1)) === false) {
				continue;
			}
			if ($pos2 === $pos) {
				$log .= $term;
			} elseif ($pos2 < $pos) {
				$log .= substr($term, 3);
			} else {
				if (++$max > 3 || strlen($log) > 42) {
					break;
				}
				$log .= ' ' . $term;
			}
			$pos = $pos2 + strlen($term);
		}
		// run the command, filter for single word character
		$log = trim($log);
		if (strlen($log) < 2 || (strlen($log) == 3 && ord($log[0]) > 0x80)) {
			return;
		}
		$this->addSearchLog($log);
	}

	/**
	 * 清空默认搜索语句
	 */
	private function clearQuery()
	{
		$cmd = new XSCommand(XS_CMD_QUERY_INIT);
		if ($this->_resetScheme === true) {
			$cmd->arg1 = 1;
			$this->_prefix = array();
			$this->_fieldSet = false;
			$this->_resetScheme = false;
		}
		$this->execCommand($cmd);
		$this->_query = $this->_count = $this->_terms = null;
	}

	/**
	 * 增加默认搜索语句
	 * @param string $query 搜索语句
	 * @param int $addOp 与旧语句的结合操作符, 如果无旧语句或为空则这此无意义, 支持的操作符有:
	 *        XS_CMD_QUERY_OP_AND
	 *        XS_CMD_QUERY_OP_OR
	 *        XS_CMD_QUERY_OP_AND_NOT
	 *        XS_CMD_QUERY_OP_XOR
	 *        XS_CMD_QUERY_OP_AND_MAYBE
	 *        XS_CMD_QUERY_OP_FILTER
	 * @param float $scale 权重计算缩放比例, 默认为 1表示不缩放, 其它值范围 0.xx ~ 655.35
	 * @return string 修正后的搜索语句
	 */
	public function addQueryString($query, $addOp = XS_CMD_QUERY_OP_AND, $scale = 1)
	{
		$query = $this->preQueryString($query);
		$bscale = ($scale > 0 && $scale != 1) ? pack('n', intval($scale * 100)) : '';

		$cmd = new XSCommand(XS_CMD_QUERY_PARSE, $addOp, $this->_defaultOp, $query, $bscale);
		$this->execCommand($cmd);
		return $query;
	}

	/**
	 * 增加默认搜索词汇
	 * @param string $field 索引词所属的字段, 若为混合区词汇可设为 null 或 body 型的字段名
	 * @param string|array $term 索引词或列表
	 * @param int $addOp 与旧语句的结合操作符, 如果无旧语句或为空则这此无意义, 支持的操作符有:
	 * @param float $scale 权重计算缩放比例, 默认为 1表示不缩放, 其它值范围 0.xx ~ 655.35
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @see addQueryString
	 *
	 * 注：自 v1.4.10 起，允许传入数组，多词之间通过 defaultOp 连接，并且这些词不会再被分词。
	 */
	public function addQueryTerm($field, $term, $addOp = XS_CMD_QUERY_OP_AND, $scale = 1)
	{
		$term = XS::convert($term, 'UTF-8', $this->_charset);
		$bscale = ($scale > 0 && $scale != 1) ? pack('n', intval($scale * 100)) : '';
		$vno = $field === null ? XSFieldScheme::MIXED_VNO : $this->xs->getField($field, true)->vno;
		$cmd = XS_CMD_QUERY_TERM;
		if (is_array($term)) {
			if (count($term) === 0) {
				return $this;
			} elseif (count($term) === 1) {
				$term = current($term);
			} else {
				$term = implode("\t", $term);
				$cmd = XS_CMD_QUERY_TERMS;
			}
		}
		$cmd = new XSCommand($cmd, $addOp, $vno, $term, $bscale);
		$this->execCommand($cmd);
		return $this;
	}

	/**
	 * 还原搜索 DB
	 * 常用于因需改变当前 db 为 LOG_DB 后还原
	 */
	private function restoreDb()
	{
		$db = $this->_lastDb;
		$dbs = $this->_lastDbs;
		$this->setDb($db);
		foreach ($dbs as $name) {
			$this->addDb($name);
		}
	}

	/**
	 * 搜索语句的准备工作
	 * 登记相关的字段前缀并给非布尔字段补上括号, 首次搜索必须通知服务端关于 cutlen, numeric 字段的设置
	 * @param string $query 要准备的搜索语句
	 * @return string 准备好的搜索语句
	 */
	private function preQueryString($query)
	{
		// check to register prefix
		$query = trim($query);
		//if ($query === '')
		//	throw new XSException('Query string cann\'t be empty');
		// force to clear query with resetScheme
		if ($this->_resetScheme === true) {
			$this->clearQuery();
		}
		// init special field here
		$this->initSpecialField();

		$newQuery = '';
		$parts = preg_split('/[ \t\r\n]+/', $query);
		foreach ($parts as $part) {
			if ($part === '') {
				continue;
			}
			if ($newQuery != '') {
				$newQuery .= ' ';
			}
			if (($pos = strpos($part, ':', 1)) !== false) {
				for ($i = 0; $i < $pos; $i++) {
					if (strpos('+-~(', $part[$i]) === false) {
						break;
					}
				}
				$name = substr($part, $i, $pos - $i);
				if (($field = $this->xs->getField($name, false)) !== false
					&& $field->vno != XSFieldScheme::MIXED_VNO) {
					$this->regQueryPrefix($name);
					if ($field->hasCustomTokenizer()) {
						$prefix = $i > 0 ? substr($part, 0, $i) : '';
						$suffix = '';
						// force to lowercase for boolean terms
						$value = substr($part, $pos + 1);
						if (substr($value, -1, 1) === ')') {
							$suffix = ')';
							$value = substr($value, 0, -1);
						}
						$terms = array();
						$tokens = $field->getCustomTokenizer()->getTokens($value);
						foreach ($tokens as $term) {
							$terms[] = strtolower($term);
						}
						$terms = array_unique($terms);
						$newQuery .= $prefix . $name . ':' . implode(' ' . $name . ':', $terms) . $suffix;
					} elseif (substr($part, $pos + 1, 1) != '(' && preg_match('/[\x81-\xfe]/', $part)) {
						// force to add brackets for default scws tokenizer
						$newQuery .= substr($part, 0, $pos + 1) . '(' . substr($part, $pos + 1) . ')';
					} else {
						$newQuery .= $part;
					}
					continue;
				}
			}
			if (strlen($part) > 1 && ($part[0] == '+' || $part[0] == '-') && $part[1] != '('
				&& preg_match('/[\x81-\xfe]/', $part)) {
				$newQuery .= substr($part, 0, 1) . '(' . substr($part, 1) . ')';
				continue;
			}
			$newQuery .= $part;
		}
		return XS::convert($newQuery, 'UTF-8', $this->_charset);
	}

	/**
	 * 登记搜索语句中的字段
	 * @param string $name 字段名称
	 */
	private function regQueryPrefix($name)
	{
		if (!isset($this->_prefix[$name])
			&& ($field = $this->xs->getField($name, false))
			&& ($field->vno != XSFieldScheme::MIXED_VNO)) {
			$type = $field->isBoolIndex() ? XS_CMD_PREFIX_BOOLEAN : XS_CMD_PREFIX_NORMAL;
			$cmd = new XSCommand(XS_CMD_QUERY_PREFIX, $type, $field->vno, $name);
			$this->execCommand($cmd);
			$this->_prefix[$name] = true;
		}
	}

	/**
	 * 设置字符型字段及裁剪长度
	 */
	private function initSpecialField()
	{
		if ($this->_fieldSet === true) {
			return;
		}
		foreach ($this->xs->getAllFields() as $field) /* @var $field XSFieldMeta */ {
			if ($field->cutlen != 0) {
				$len = min(127, ceil($field->cutlen / 10));
				$cmd = new XSCommand(XS_CMD_SEARCH_SET_CUT, $len, $field->vno);
				$this->execCommand($cmd);
			}
			if ($field->isNumeric()) {
				$cmd = new XSCommand(XS_CMD_SEARCH_SET_NUMERIC, 0, $field->vno);
				$this->execCommand($cmd);
			}
		}
		$this->_fieldSet = true;
	}

	/**
	 * 清除查询语句中的字段名、布尔字段条件
	 * @param string $query 查询语句
	 * @return string 净化后的语句
	 */
	private function cleanFieldQuery($query)
	{
		$query = strtr($query, array(' AND ' => ' ', ' OR ' => ' '));
		if (strpos($query, ':') !== false) {
			$regex = '/(^|\s)([0-9A-Za-z_\.-]+):([^\s]+)/';
			return preg_replace_callback($regex, array($this, 'cleanFieldCallback'), $query);
		}
		return $query;
	}

	/**
	 * 清除布尔字段查询语句和非布尔的字段名
	 * 用于正则替换回调函数, 净化 {@link getCorrectedQuery} 和 {@link getRelatedQuery} 中的搜索语句
	 * @param array $match 正则匹配的部分, [1]:prefix [2]:field, [3]:data
	 */
	private function cleanFieldCallback($match)
	{
		if (($field = $this->xs->getField($match[2], false)) === false) {
			return $match[0];
		}
		if ($field->isBoolIndex()) {
			return '';
		}
		if (substr($match[3], 0, 1) == '(' && substr($match[3], -1, 1) == ')') {
			$match[3] = substr($match[3], 1, -1);
		}
		return $match[1] . $match[3];
	}

	/**
	 * 初始始化高亮替换数据
	 */
	private function initHighlight()
	{
		$terms = array();
		$tmps = $this->terms($this->_highlight, false);
		for ($i = 0; $i < count($tmps); $i++) {
			if (strlen($tmps[$i]) !== 6 || ord(substr($tmps[$i], 0, 1)) < 0xc0) {
				$terms[] = XS::convert($tmps[$i], $this->_charset, 'UTF-8');
				continue;
			}

			// auto fixed duality in libscws
			// ABC => AB,BC => ABC,BC,AB
			// ABCD => AB,BC,CD => CD,ABC,BC,AB
			// ABCDE => AB,BC,CD,DE => CDE,DE,CD,ABC,BC,AB
			for ($j = $i + 1; $j < count($tmps); $j++) {
				if (strlen($tmps[$j]) !== 6 || substr($tmps[$j], 0, 3) !== substr($tmps[$j - 1], 3, 3)) {
					break;
				}
			}
			if (($k = ($j - $i)) === 1) {
				$terms[] = XS::convert($tmps[$i], $this->_charset, 'UTF-8');
			} else {
				$i = $j - 1;
				while ($k--) {
					$j--;
					if ($k & 1) {
						$terms[] = XS::convert(substr($tmps[$j - 1], 0, 3) . $tmps[$j], $this->_charset, 'UTF-8');
					}
					$terms[] = XS::convert($tmps[$j], $this->_charset, 'UTF-8');
				}
			}
		}

		$pattern = $replace = $pairs = array();
		foreach ($terms as $term) {
			if (!preg_match('/[a-zA-Z]/', $term)) {
				$pairs[$term] = '<em>' . $term . '</em>';
			} else {
				$pattern[] = '/' . strtr($term, array('+' => '\\+', '/' => '\\/')) . '/i';
				$replace[] = '<em>$0</em>';
			}
		}

		$this->_highlight = array();
		if (count($pairs) > 0) {
			$this->_highlight['pairs'] = $pairs;
		}
		if (count($pattern) > 0) {
			$this->_highlight['pattern'] = $pattern;
			$this->_highlight['replace'] = $replace;
		}
	}

	/**
	 * Format the value range/ge
	 * @param array $match
	 * @return string
	 */
	private function formatValueRange($match)
	{
		// VALUE_[GL]E 0 xxx yyy
		$field = $this->xs->getField(intval($match[2]), false);
		if ($field === false) {
			return $match[0];
		}
		$val1 = $val2 = '~';
		if (isset($match[4])) {
			$val2 = $field->isNumeric() ? $this->xapianUnserialise($match[4]) : $match[4];
		}
		if ($match[1] === 'VALUE_LE') {
			$val2 = $field->isNumeric() ? $this->xapianUnserialise($match[3]) : $match[3];
		} else {
			$val1 = $field->isNumeric() ? $this->xapianUnserialise($match[3]) : $match[3];
		}
		return $field->name . ':[' . $val1 . ',' . $val2 . ']';
	}

	private function numfromstr($str, $index)
	{
		return $index < strlen($str) ? ord($str[$index]) : 0;
	}

	/**
	 * Convert a string encoded by xapian to a floating point number
	 * @param string $value
	 * @return double unserialised number
	 */
	private function xapianUnserialise($value)
	{
		if ($value === "\x80") {
			return 0.0;
		}
		if ($value === str_repeat("\xff", 9)) {
			return INF;
		}
		if ($value === '') {
			return -INF;
		}
		$i = 0;
		$c = ord($value[0]);
		$c ^= ($c & 0xc0) >> 1;
		$negative = !($c & 0x80) ? 1 : 0;
		$exponent_negative = ($c & 0x40) ? 1 : 0;
		$explen = !($c & 0x20) ? 1 : 0;
		$exponent = $c & 0x1f;
		if (!$explen) {
			$exponent >>= 2;
			if ($negative ^ $exponent_negative) {
				$exponent ^= 0x07;
			}
		} else {
			$c = $this->numfromstr($value, ++$i);
			$exponent <<= 6;
			$exponent |= ($c >> 2);
			if ($negative ^ $exponent_negative) {
				$exponent &= 0x07ff;
			}
		}

		$word1 = ($c & 0x03) << 24;
		$word1 |= $this->numfromstr($value, ++$i) << 16;
		$word1 |= $this->numfromstr($value, ++$i) << 8;
		$word1 |= $this->numfromstr($value, ++$i);

		$word2 = 0;
		if ($i < strlen($value)) {
			$word2 = $this->numfromstr($value, ++$i) << 24;
			$word2 |= $this->numfromstr($value, ++$i) << 16;
			$word2 |= $this->numfromstr($value, ++$i) << 8;
			$word2 |= $this->numfromstr($value, ++$i);
		}

		if (!$negative) {
			$word1 |= 1 << 26;
		} else {
			$word1 = 0 - $word1;
			if ($word2 != 0) {
				++$word1;
			}
			$word2 = 0 - $word2;
			$word1 &= 0x03ffffff;
		}

		$mantissa = 0;
		if ($word2) {
			$mantissa = $word2 / 4294967296.0; // 1<<32
		}
		$mantissa += $word1;
		$mantissa /= 1 << ($negative === 1 ? 26 : 27);
		if ($exponent_negative) {
			$exponent = 0 - $exponent;
		}
		$exponent += 8;
		if ($negative) {
			$mantissa = 0 - $mantissa;
		}

		return round($mantissa * pow(2, $exponent), 2);
	}

	/**
	 * @return boolean whether the user agent is a robot or search engine
	 */
	private function isRobotAgent()
	{
		if (isset($_SERVER['HTTP_USER_AGENT'])) {
			$agent = strtolower($_SERVER['HTTP_USER_AGENT']);
			$keys = array('bot', 'slurp', 'spider', 'crawl', 'curl');
			foreach ($keys as $key) {
				if (strpos($agent, $key) !== false) {
					return true;
				}
			}
		}
		return false;
	}
}

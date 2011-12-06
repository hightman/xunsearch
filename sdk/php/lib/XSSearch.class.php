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
	const LOB_DB = 'log_db';

	private $_defaultOp = CMD_QUERY_OP_AND;
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
		if ($this->_charset == 'UTF8')
			$this->_charset = 'UTF-8';
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
		$this->_defaultOp = $value === true ? CMD_QUERY_OP_OR : CMD_QUERY_OP_AND;
		return $this;
	}

	/**
	 * 获取解析后的搜索语句
	 * @param string $query 搜索语句, 若传入 null 使用默认语句
	 * @return string 返回解析后的搜索语句
	 */
	public function getQuery($query = null)
	{
		$query = $query === null ? '' : $this->preQueryString($query);
		$cmd = new XSCommand(CMD_QUERY_GET_STRING, 0, $this->_defaultOp, $query);
		$res = $this->execCommand($cmd, CMD_OK_QUERY_STRING);
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
		if ($query !== null)
		{
			$this->_query = $query;
			$this->addQueryString($query);
		}
		return $this;
	}

	/**
	 * 设置多字段组合排序方式
	 * 当您需要根据多个字段的值按不同的方式综合排序时, 请使用这项
	 * @param array $fields 排序依据的字段数组, 以字段名称为键, true/false 为值表示正序或逆序
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.1.0
	 */
	public function setMultiSort($fields)
	{
		if (!is_array($fields))
			return $this->setSort($fields);

		// [vno][0/1] (0:reverse,1:asc)
		$buf = '';
		foreach ($fields as $key => $value)
		{
			if (is_bool($value))
			{
				$vno = $this->xs->getField($key, true)->vno;
				$asc = $value;
			}
			else
			{
				$vno = $this->xs->getField($value, true)->vno;
				$asc = false;
			}
			if ($vno != XSFieldScheme::MIXED_VNO)
				$buf .= chr($vno) . chr($asc ? 1 : 0);
		}
		if ($buf !== '')
		{
			$cmd = new XSCommand(CMD_SEARCH_SET_SORT, CMD_SORT_TYPE_MULTI, 0, $buf);
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
	 * @return XSSearch 返回对象本身以支持串接操作
	 */
	public function setSort($field, $asc = false)
	{
		if (is_array($field))
			return $this->setMultiSort($field);
		if ($field === null)
			$cmd = new XSCommand(CMD_SEARCH_SET_SORT, CMD_SORT_TYPE_RELEVANCE);
		else
		{
			$type = CMD_SORT_TYPE_VALUE | ($asc ? CMD_SORT_FLAG_ASCENDING : 0);
			$vno = $this->xs->getField($field, true)->vno;

			$cmd = new XSCommand(CMD_SEARCH_SET_SORT, $type, $vno);
		}
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

		$cmd = new XSCommand(CMD_SEARCH_SET_COLLAPSE, $max, $vno);
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
		if ($from !== null || $to !== null)
		{
			if (strlen($from) > 255 || strlen($to) > 255)
				throw new XSException('Value of range is too long');

			$vno = $this->xs->getField($field)->vno;
			$from = XS::convert($from, 'UTF-8', $this->_charset);
			$to = XS::convert($to, 'UTF-8', $this->_charset);
			if ($from === null)
				$cmd = new XSCommand(CMD_QUERY_VALCMP, CMD_QUERY_OP_FILTER, $vno, $to, chr(CMD_VALCMP_LE));
			else if ($to === null)
				$cmd = new XSCommand(CMD_QUERY_VALCMP, CMD_QUERY_OP_FILTER, $vno, $from, chr(CMD_VALCMP_GE));
			else
				$cmd = new XSCommand(CMD_QUERY_RANGE, CMD_QUERY_OP_FILTER, $vno, $from, $to);
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
		$this->addQueryTerm($field, $term, CMD_QUERY_OP_AND_MAYBE, $weight);
		return $this;
	}

	/**
	 * 设置分面搜索记数
	 * 用于记录匹配搜索结果中按字段值分组的数量统计, 每次调用 {@link search} 后会还原设置 
	 * 对于多次调用 $exact 参数以最后一次为准, 只支持字段值不超过 255 字节的情况
	 * @param mixed $field 要进行分组统计的字段或字段组成的数组, 最多同时支持 8 个
	 * @param bool $exact 是否要求绝对精确搜索, 这会造成较大的系统开销
	 * @return XSSearch 返回对象本身以支持串接操作
	 * @since 1.1.0
	 */
	public function setFacets($field, $exact = false)
	{
		$buf = '';
		if (!is_array($field))
			$buf = chr($this->xs->getField($field)->vno);
		else
		{
			foreach ($field as $ff)
			{
				$buf .= chr($this->xs->getField($ff)->vno);
			}
		}
		$cmd = array('cmd' => CMD_SEARCH_SET_FACETS, 'buf' => $buf);
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
		if ($field === null)
			return $this->_facets;
		return isset($this->_facets[$field]) ? $this->_facets[$field] : array();
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
		$this->execCommand(array('cmd' => CMD_SEARCH_SET_DB, 'buf' => strval($name)));
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
		$this->execCommand(array('cmd' => CMD_SEARCH_ADD_DB, 'buf' => $name));
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
	 * @param bool $raw 是否进行编码转换, 默认为 true
	 * @return array 可用于高亮显示的词条列表
	 */
	public function terms($query = null, $convert = true)
	{
		$query = $query === null ? '' : $this->preQueryString($query);

		if ($query === '' && $this->_terms !== null)
			$ret = $this->_terms;
		else
		{
			$cmd = new XSCommand(CMD_QUERY_GET_TERMS, 0, $this->_defaultOp, $query);
			$res = $this->execCommand($cmd, CMD_OK_QUERY_TERMS);
			$ret = array();

			$tmps = explode(' ', $res->buf);
			for ($i = 0; $i < count($tmps); $i++)
			{
				$chr = ord(substr($tmps[$i], 0, 1));
				if ($tmps[$i] === '' || strpos($tmps[$i], ':') !== false)
					continue;
				$ret[] = $tmps[$i];
			}
			if ($query === '')
				$this->_terms = $ret;
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
		if ($query === '' && $this->_count !== null)
			return $this->_count;

		$cmd = new XSCommand(CMD_SEARCH_GET_TOTAL, 0, $this->_defaultOp, $query);
		$res = $this->execCommand($cmd, CMD_OK_SEARCH_TOTAL);
		$ret = unpack('Icount', $res->buf);

		if ($query === '')
			$this->_count = $ret['count'];
		return $ret['count'];
	}

	/**
	 * 获取匹配的搜索结果文档
	 * 默认提取最匹配的前 self::PAGE_SIZE 个结果
	 * 如需分页请参见 {@link setLimit} 设置, 每次调用本函数后都会还原 setLimit 的设置
	 * @param string $query 搜索语句, 若传入 null 使用默认语句, 最大长度为 80 字节
	 * @return XSDocument[] 匹配的搜索结果文档列表
	 */
	public function search($query = null)
	{
		$this->_highlight = $query;
		$query = $query === null ? '' : $this->preQueryString($query);
		$page = pack('II', $this->_offset, $this->_limit > 0 ? $this->_limit : self::PAGE_SIZE);

		// get result header
		$cmd = new XSCommand(CMD_SEARCH_GET_RESULT, 0, $this->_defaultOp, $query, $page);
		$res = $this->execCommand($cmd, CMD_OK_RESULT_BEGIN);
		$tmp = unpack('Icount', $res->buf);
		$this->_lastCount = $tmp['count'];

		// load vno map to name of fields
		$ret = $this->_facets = array();
		$vnoes = $this->xs->getScheme()->getVnoMap();

		// get result documents		
		while (true)
		{
			$res = $this->getRespond();
			if ($res->cmd == CMD_SEARCH_RESULT_FACETS)
			{
				$off = 0;
				while (($off + 6) < strlen($res->buf))
				{
					$tmp = unpack('Cvno/Cvlen/Inum', substr($res->buf, $off, 6));
					if (isset($vnoes[$tmp['vno']]))
					{
						$name = $vnoes[$tmp['vno']];
						$value = substr($res->buf, $off + 6, $tmp['vlen']);
						if (!isset($this->_facets[$name]))
							$this->_facets[$name] = array();
						$this->_facets[$name][$value] = $tmp['num'];
					}
					$off += $tmp['vlen'] + 6;
				}
			}
			else if ($res->cmd == CMD_SEARCH_RESULT_DOC)
			{
				// got new doc
				$doc = new XSDocument($res->buf, $this->_charset);
				$ret[] = $doc;
			}
			else if ($res->cmd == CMD_SEARCH_RESULT_FIELD)
			{
				// fields of doc
				if (isset($doc))
				{
					$name = isset($vnoes[$res->arg]) ? $vnoes[$res->arg] : $res->arg;
					$doc->setField($name, $res->buf);
				}
			}
			else if ($res->cmd == CMD_OK && $res->arg == CMD_OK_RESULT_END)
			{
				// got the end
				break;
			}
			else
			{
				$msg = 'Unexpected respond in search {CMD:' . $res->cmd . ', ARG:' . $res->arg . '}';
				throw new XSException($msg);
			}
		}

		if ($query === '')
		{
			$this->_count = $tmp['count'];
			// trigger to logQuery
			$this->logQuery();
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
		$cmd = new XSCommand(CMD_SEARCH_DB_TOTAL);
		$res = $this->execCommand($cmd, CMD_OK_DB_TOTAL);
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
		try
		{
			$this->setDb(self::LOB_DB)->setLimit($limit);
			if ($type !== 'lastnum' && $type !== 'currnum')
				$type = 'total';
			$result = $this->search($type . ':1');
			foreach ($result as $doc) /* @var $doc XSDocument */
			{
				$body = $doc->body;
				$ret[$body] = $doc->f($type);
			}
			$this->restoreDb();
		}
		catch (XSException $e)
		{
			if ($e->getCode() != CMD_ERR_XAPIAN)
				throw $e;
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
		if ($query === null)
			$query = $this->cleanFieldQuery($this->_query);

		if (empty($query) || strpos($query, ':') !== false)
			return $ret;

		// Search the log database
		$op = $this->_defaultOp;
		$this->xs->setScheme(XSFieldScheme::logger());
		try
		{
			$result = $this->setDb(self::LOB_DB)->setFuzzy()->setLimit($limit + 1)->search($query);
			foreach ($result as $doc) /* @var $doc XSDocument */
			{
				$doc->setCharset($this->_charset);
				$body = $doc->body;
				if (!strcasecmp($body, $query))
					continue;
				$ret[] = $body;
				if (count($ret) == $limit)
					break;
			}
		}
		catch (XSException $e)
		{
			if ($e->getCode() != CMD_ERR_XAPIAN)
				throw $e;
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

		try
		{
			$buf = XS::convert($query, 'UTF-8', $this->_charset);
			$cmd = array('cmd' => CMD_QUERY_GET_EXPANDED, 'arg1' => $limit, 'buf' => $buf);
			$res = $this->execCommand($cmd, CMD_OK_RESULT_BEGIN);

			// echo "Raw Query: " . $res->buf . "\n";			
			// get result documents		
			while (true)
			{
				$res = $this->getRespond();
				if ($res->cmd == CMD_SEARCH_RESULT_FIELD)
				{
					$ret[] = XS::convert($res->buf, $this->_charset, 'UTF-8');
				}
				else if ($res->cmd == CMD_OK && $res->arg == CMD_OK_RESULT_END)
				{
					// got the end
					// echo "Parsed Query: " . $res->buf . "\n";	
					break;
				}
				else
				{
					$msg = 'Unexpected respond in search {CMD:' . $res->cmd . ', ARG:' . $res->arg . '}';
					throw new XSException($msg);
				}
			}
		}
		catch (XSException $e)
		{
			if ($e->getCode() != CMD_ERR_XAPIAN)
				throw $e;
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

		try
		{
			if ($query === null)
			{
				if ($this->_count > 0 && $this->_count > ceil($this->getDbTotal() * 0.001))
					return $ret;
				$query = $this->cleanFieldQuery($this->_query);
			}
			if (empty($query) || strpos($query, ':') !== false)
				return $ret;
			$buf = XS::convert($query, 'UTF-8', $this->_charset);
			$cmd = array('cmd' => CMD_QUERY_GET_CORRECTED, 'buf' => $buf);
			$res = $this->execCommand($cmd, CMD_OK_QUERY_CORRECTED);
			if ($res->buf !== '')
				$ret = explode("\n", XS::convert($res->buf, $this->_charset, 'UTF-8'));
		}
		catch (XSException $e)
		{
			if ($e->getCode() != CMD_ERR_XAPIAN)
				throw $e;
		}

		return $ret;
	}

	/**
	 * 添加搜索日志关键词到缓冲区里
	 * 需要调用 [XSIndex::flushLogging] 才能确保立即刷新, 否则要隔一段时间
	 * @param string $query 需要记录的数据
	 * @param int $wdf 需要记录的次数, 默认为 1
	 */
	public function addSearchLog($query, $wdf = 1)
	{
		$cmd = array('cmd' => CMD_SEARCH_ADD_LOG, 'buf' => $query);
		if ($wdf > 1)
			$cmd['buf1'] = pack('i', $wdf);
		$this->execCommand($cmd, CMD_OK_LOGGED);
	}

	/**
	 * 搜索结果字符串高亮处理
	 * 对搜索结果文档的字段进行高亮、飘红处理, 高亮部分加上 em 标记
	 * @param string $value 需要处理的数据
	 * @return string 高亮后的数据
	 */
	public function highlight($value)
	{
		// return empty value directly
		if (empty($value))
			return $value;

		// initlize the highlight replacements
		if (!is_array($this->_highlight))
			$this->initHighlight();

		// process replace
		if (isset($this->_highlight['pattern']))
			$value = preg_replace($this->_highlight['pattern'], $this->_highlight['replace'], $value);
		if (isset($this->_highlight['pairs']))
			$value = strtr($value, $this->_highlight['pairs']);
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
		if ($query !== '' && $query !== null)
			$terms = $this->terms($query, false);
		else
		{
			// 无结果、包含 OR、XOR、NOT/-、默认 fuzzy		
			$query = $this->_query;
			if (!$this->_lastCount || $this->_defaultOp == CMD_QUERY_OP_OR
				|| strpos($query, ' OR ') || strpos($query, ' NOT ') || strpos($query, ' XOR '))
			{
				return;
			}
			$terms = $this->terms(null, false);
		}
		// purify the query statement to log
		$log = '';
		$pos = $max = 0;
		foreach ($terms as $term)
		{
			$pos1 = ($pos > 3 && strlen($term) === 6) ? $pos - 3 : $pos;
			if (($pos2 = strpos($query, $term, $pos1)) === false)
				break;
			if ($pos2 === $pos)
				$log .= $term;
			else if ($pos2 < $pos)
				$log .= substr($term, 3);
			else
			{
				if (++$max > 3 || strlen($log) > 42)
					break;
				$log .= ' ' . $term;
			}
			$pos = $pos2 + strlen($term);
		}
		// run the command, filter for single word character
		$log = trim($log);
		if (strlen($log) < 2 || (strlen($log) == 3 && ord($log[0]) > 0x80))
			return;
		$this->addSearchLog($log);
	}

	/**
	 * 清空默认搜索语句
	 */
	private function clearQuery()
	{
		$cmd = new XSCommand(CMD_QUERY_INIT);
		if ($this->_resetScheme === true)
		{
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
	 *        CMD_QUERY_OP_AND	
	 *        CMD_QUERY_OP_OR			
	 *        CMD_QUERY_OP_AND_NOT
	 *        CMD_QUERY_OP_XOR
	 *        CMD_QUERY_OP_AND_MAYBE
	 *        CMD_QUERY_OP_FILTER
	 * @param float $scale 权重计算缩放比例, 默认为 1表示不缩放, 其它值范围 0.xx ~ 655.35
	 * @return string 修正后的搜索语句
	 */
	private function addQueryString($query, $addOp = CMD_QUERY_OP_AND, $scale = 1)
	{
		$query = $this->preQueryString($query);
		$bscale = ($scale > 0 && $scale != 1) ? pack('n', intval($scale * 100)) : '';

		$cmd = new XSCommand(CMD_QUERY_PARSE, $addOp, $this->_defaultOp, $query, $bscale);
		$this->execCommand($cmd);
		return $query;
	}

	/**
	 * 增加默认搜索词汇
	 * @param string $field 索引词所属的字段, 若为混合区词汇可设为 null 或 body 型的字段名
	 * @param string $term 索引词 (强制转为小写)
	 * @param int $addOp 与旧语句的结合操作符, 如果无旧语句或为空则这此无意义, 支持的操作符有:
	 * @param float $scale 权重计算缩放比例, 默认为 1表示不缩放, 其它值范围 0.xx ~ 655.35
	 * @see addQueryString
	 */
	private function addQueryTerm($field, $term, $addOp = CMD_QUERY_OP_AND, $scale = 1)
	{
		$term = strtolower($term);
		$term = XS::convert($term, 'UTF-8', $this->_charset);
		$bscale = ($scale > 0 && $scale != 1) ? pack('n', intval($scale * 100)) : '';
		$vno = $field === null ? XSFieldScheme::MIXED_VNO : $this->xs->getField($field, true)->vno;

		$cmd = new XSCommand(CMD_QUERY_TERM, $addOp, $vno, $term, $bscale);
		$this->execCommand($cmd);
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
		foreach ($dbs as $name)
		{
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
		if ($query === '')
			throw new XSException('Query string cann\'t be empty');

		// force to clear query with resetScheme
		if ($this->_resetScheme === true)
			$this->clearQuery();

		$newQuery = '';
		$parts = preg_split('/[ \t\r\n]+/', $query);
		foreach ($parts as $part)
		{
			if ($newQuery != '')
				$newQuery .= ' ';
			if (($pos = strpos($part, ':', 1)) !== false)
			{
				for ($i = 0; $i < $pos; $i++)
				{
					if (strpos('+-~(', $part[$i]) === false)
						break;
				}
				$name = substr($part, $i, $pos - $i);
				if (($field = $this->xs->getField($name, false)) !== false
					&& $field->vno != XSFieldScheme::MIXED_VNO)
				{
					$this->regQueryPrefix($name);
					if (!$field->isBoolIndex() && substr($part, $pos + 1, 1) != '('
						&& preg_match('/[\x81-\xfe]/', $part))
					{
						$newQuery .= substr($part, 0, $pos + 1) . '(' . substr($part, $pos + 1) . ')';
					}
					else if ($field->isBoolIndex())
					{
						// force to lowercase for boolean terms
						$value = substr($part, $pos + 1);
						// Add custom tokenizer supported
						if (!$field->hasCustomTokenizer())
							$newQuery .= substr($part, 0, $pos + 1) . strtolower($value);
						else
						{
							$terms = array();
							$tokens = $field->getCustomTokenizer()->getTokens($value);
							foreach ($tokens as $term)
							{
								$terms[] = strtolower($term);
							}
							$terms = array_unique($terms);
							$newQuery .= $name . ':' . implode(' ' . $name . ':', $terms);
						}
					}
					else
					{
						$newQuery .= $part;
					}
					continue;
				}
			}
			if (($part[0] == '+' || $part[0] == '-') && $part[1] != '('
				&& preg_match('/[\x81-\xfe]/', $part))
			{
				$newQuery .= substr($part, 0, 1) . '(' . substr($part, 1) . ')';
				continue;
			}
			$newQuery .= $part;
		}

		// check to send cutlen/numeric once
		if ($this->_fieldSet !== true)
		{
			foreach ($this->xs->getAllFields() as $field) /* @var $field XSFieldMeta */
			{
				if ($field->cutlen != 0)
				{
					$len = min(127, ceil($field->cutlen / 10));
					$cmd = new XSCommand(CMD_SEARCH_SET_CUT, $len, $field->vno);
					$this->execCommand($cmd);
				}
				if ($field->isNumeric())
				{
					$cmd = new XSCommand(CMD_SEARCH_SET_NUMERIC, 0, $field->vno);
					$this->execCommand($cmd);
				}
			}
			$this->_fieldSet = true;
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
			&& ($field->vno != XSFieldScheme::MIXED_VNO))
		{
			$type = $field->isBoolIndex() ? CMD_PREFIX_BOOLEAN : CMD_PREFIX_NORMAL;
			$cmd = new XSCommand(CMD_QUERY_PREFIX, $type, $field->vno, $name);
			$this->execCommand($cmd);
			$this->_prefix[$name] = true;
		}
	}

	/**
	 * 清除查询语句中的字段名、布尔字段条件
	 * @param string $query 查询语句
	 * @return string 净化后的语句
	 */
	private function cleanFieldQuery($query)
	{
		$query = strtr($query, array(' AND ' => ' ', ' OR ' => ' '));
		if (strpos($query, ':') !== false)
		{
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
		if (($field = $this->xs->getField($match[2], false)) === false)
			return $match[0];
		if ($field->isBoolIndex())
			return '';
		if (substr($match[3], 0, 1) == '(' && substr($match[3], -1, 1) == ')')
			$match[3] = substr($match[3], 1, -1);
		return $match[1] . $match[3];
	}

	/**
	 * 初始始化高亮替换数据
	 */
	private function initHighlight()
	{
		$terms = array();
		$tmps = $this->terms($this->_highlight, false);
		for ($i = 0; $i < count($tmps); $i++)
		{
			if (strlen($tmps[$i]) !== 6 || ord(substr($tmps[$i], 0, 1)) < 0xc0)
			{
				$terms[] = XS::convert($tmps[$i], $this->_charset, 'UTF-8');
				continue;
			}

			// auto fixed duality in libscws
			// ABC => AB,BC => ABC,BC,AB 
			// ABCD => AB,BC,CD => CD,ABC,BC,AB
			// ABCDE => AB,BC,CD,DE => CDE,DE,CD,ABC,BC,AB
			for ($j = $i + 1; $j < count($tmps); $j++)
			{
				if (strlen($tmps[$j]) !== 6 || substr($tmps[$j], 0, 3) !== substr($tmps[$j - 1], 3, 3))
					break;
			}
			if (($k = ($j - $i)) === 1)
				$terms[] = XS::convert($tmps[$i], $this->_charset, 'UTF-8');
			else
			{
				$i = $j - 1;
				while ($k--)
				{
					$j--;
					if ($k & 1)
						$terms[] = XS::convert(substr($tmps[$j - 1], 0, 3) . $tmps[$j], $this->_charset, 'UTF-8');
					$terms[] = XS::convert($tmps[$j], $this->_charset, 'UTF-8');
				}
			}
		}

		$pattern = $replace = $pairs = array();
		foreach ($terms as $term)
		{
			if (!preg_match('/[a-zA-Z]/', $term))
				$pairs[$term] = '<em>' . $term . '</em>';
			else
			{
				$pattern[] = '/' . strtr($term, array('+' => '\\+', '/' => '\\/')) . '/i';
				$replace[] = '<em>$0</em>';
			}
		}

		$this->_highlight = array();
		if (count($pairs) > 0)
			$this->_highlight['pairs'] = $pairs;
		if (count($pattern) > 0)
		{
			$this->_highlight['pattern'] = $pattern;
			$this->_highlight['replace'] = $replace;
		}
	}
}

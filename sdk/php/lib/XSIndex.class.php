<?php
/**
 * XSIndex 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * XS 索引管理
 * 添加/删除/修改索引数据
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS
 */
class XSIndex extends XSServer
{
	private $_buf = '';
	private $_bufSize = 0;
	private $_rebuild = false;
	private static $_adds = array();

	/**
	 * 增加一个同步索引服务器
	 * @param string $conn 索引服务端连接参数
	 * @return XSServer
	 * @throw XSException 出错时抛出异常
	 */
	public function addServer($conn)
	{
		$srv = new XSServer($conn, $this->xs);
		self::$_adds[] = $srv;
		return $srv;
	}

	/**
	 * 执行服务端指令并获取返回值
	 * 重写此方法是为了同步到额外增加的多个索引服务端
	 */
	public function execCommand($cmd, $res_arg = XS_CMD_NONE, $res_cmd = XS_CMD_OK)
	{
		$res = parent::execCommand($cmd, $res_arg, $res_cmd);
		foreach (self::$_adds as $srv) {
			$srv->execCommand($cmd, $res_arg, $res_cmd);
		}
		return $res;
	}

	/**
	 * 完全清空索引数据
	 * 如果当前数据库处于重建过程中将禁止清空
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see beginRebuild
	 */
	public function clean()
	{
		$this->execCommand(XS_CMD_INDEX_CLEAN_DB, XS_CMD_OK_DB_CLEAN);
		return $this;
	}

	/**
	 * 添加文档到索引中
	 * 特别要注意的是: 系统不会自动检测主键是否冲突, 即便已存在相同主键也会添加进去
	 * @param XSDocument $doc
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see update
	 */
	public function add(XSDocument $doc)
	{
		return $this->update($doc, true);
	}

	/**
	 * 更新索引文档
	 * 该方法相当于先根据主键删除已存在的旧文档, 然后添加该文档
	 * 如果你能明确认定是新文档, 则建议使用 {@link add}
	 * @param XSDocument $doc
	 * @param bool $add 是否为新增文档, 已有数据中不存在同一主键的其它数据
	 * @return XSIndex 返回自身对象以支持串接操作
	 */
	public function update(XSDocument $doc, $add = false)
	{
		// before submit
		if ($doc->beforeSubmit($this) === false) {
			return $this;
		}

		// check primary key of document
		$fid = $this->xs->getFieldId();
		$key = $doc->f($fid);
		if ($key === null || $key === '') {
			throw new XSException('Missing value of primary key (FIELD:' . $fid . ')');
		}

		// request cmd
		$cmd = new XSCommand(XS_CMD_INDEX_REQUEST, XS_CMD_INDEX_REQUEST_ADD);
		if ($add !== true) {
			$cmd->arg1 = XS_CMD_INDEX_REQUEST_UPDATE;
			$cmd->arg2 = $fid->vno;
			$cmd->buf = $key;
		}
		$cmds = array($cmd);

		// document cmds
		foreach ($this->xs->getAllFields() as $field) /* @var $field XSFieldMeta */ {
			// value
			if (($value = $doc->f($field)) !== null) {
				$varg = $field->isNumeric() ? XS_CMD_VALUE_FLAG_NUMERIC : 0;
				$value = $field->val($value);
				if (!$field->hasCustomTokenizer()) {
					// internal tokenizer
					$wdf = $field->weight | ($field->withPos() ? XS_CMD_INDEX_FLAG_WITHPOS : 0);
					if ($field->hasIndexMixed()) {
						$cmds[] = new XSCommand(XS_CMD_DOC_INDEX, $wdf, XSFieldScheme::MIXED_VNO, $value);
					}
					if ($field->hasIndexSelf()) {
						$wdf |= $field->isNumeric() ? 0 : XS_CMD_INDEX_FLAG_SAVEVALUE;
						$cmds[] = new XSCommand(XS_CMD_DOC_INDEX, $wdf, $field->vno, $value);
					}
					// add value
					if (!$field->hasIndexSelf() || $field->isNumeric()) {
						$cmds[] = new XSCommand(XS_CMD_DOC_VALUE, $varg, $field->vno, $value);
					}
				} else {
					// add index
					if ($field->hasIndex()) {
						$terms = $field->getCustomTokenizer()->getTokens($value, $doc);
						// self: [bool term, NOT weight, NOT stem, NOT pos]
						if ($field->hasIndexSelf()) {
							$wdf = $field->isBoolIndex() ? 1 : ($field->weight | XS_CMD_INDEX_FLAG_CHECKSTEM);
							foreach ($terms as $term) {
								if (strlen($term) > 200) {
									continue;
								}
								$term = strtolower($term);
								$cmds[] = new XSCommand(XS_CMD_DOC_TERM, $wdf, $field->vno, $term);
							}
						}
						// mixed: [use default tokenizer]
						if ($field->hasIndexMixed()) {
							$mtext = implode(' ', $terms);
							$cmds[] = new XSCommand(XS_CMD_DOC_INDEX, $field->weight, XSFieldScheme::MIXED_VNO, $mtext);
						}
					}
					// add value
					$cmds[] = new XSCommand(XS_CMD_DOC_VALUE, $varg, $field->vno, $value);
				}
			}
			// process add terms
			if (($terms = $doc->getAddTerms($field)) !== null) {
				// ignore weight for bool index
				$wdf1 = $field->isBoolIndex() ? 0 : XS_CMD_INDEX_FLAG_CHECKSTEM;
				foreach ($terms as $term => $wdf) {
					$term = strtolower($term);
					if (strlen($term) > 200) {
						continue;
					}
					$wdf2 = $field->isBoolIndex() ? 1 : $wdf * $field->weight;
					while ($wdf2 > XSFieldMeta::MAX_WDF) {
						$cmds[] = new XSCommand(XS_CMD_DOC_TERM, $wdf1 | XSFieldMeta::MAX_WDF, $field->vno, $term);
						$wdf2 -= XSFieldMeta::MAX_WDF;
					}
					$cmds[] = new XSCommand(XS_CMD_DOC_TERM, $wdf1 | $wdf2, $field->vno, $term);
				}
			}
			// process add text
			if (($text = $doc->getAddIndex($field)) !== null) {
				if (!$field->hasCustomTokenizer()) {
					$wdf = $field->weight | ($field->withPos() ? XS_CMD_INDEX_FLAG_WITHPOS : 0);
					$cmds[] = new XSCommand(XS_CMD_DOC_INDEX, $wdf, $field->vno, $text);
				} else {
					// NOT pos
					$wdf = $field->isBoolIndex() ? 1 : ($field->weight | XS_CMD_INDEX_FLAG_CHECKSTEM);
					$terms = $field->getCustomTokenizer()->getTokens($text, $doc);
					foreach ($terms as $term) {
						if (strlen($term) > 200) {
							continue;
						}
						$term = strtolower($term);
						$cmds[] = new XSCommand(XS_CMD_DOC_TERM, $wdf, $field->vno, $term);
					}
				}
			}
		}

		// submit cmd
		$cmds[] = new XSCommand(XS_CMD_INDEX_SUBMIT);

		// execute cmd
		if ($this->_bufSize > 0) {
			$this->appendBuffer(implode('', $cmds));
		} else {
			for ($i = 0; $i < count($cmds) - 1; $i++) {
				$this->execCommand($cmds[$i]);
			}
			$this->execCommand($cmds[$i], XS_CMD_OK_RQST_FINISHED);
		}

		// after submit
		$doc->afterSubmit($this);
		return $this;
	}

	/**
	 * 删除索引中的数据
	 * <pre>
	 * $index->del('123');	// 删除主键为 123 的记录
	 * $index->del(array('123', '789', '456')); // 删除主键为 123, 789, 456 的记录
	 * $index->del('abc', 'field'); // 删除字段 field 上带有索引词 abc 的所有记录
	 * $index->del(array('abc', 'def'), 'field'); // 删除字段 field 上带有索引词 abc 或 def 的所有记录
	 * </pre>	 
	 * @param mixed $term 单个主键或指定字段的索引词, 或多个组成的数组, 编码与 {@link xs} 默认字符集一致
	 * @param string $field 索引词所属的字段名称, 默认不指定则为主键字段 (类型为ID)
	 * @return XSIndex 返回自身对象以支持串接操作
	 */
	public function del($term, $field = null)
	{
		// get field
		$field = $field === null ? $this->xs->getFieldId() : $this->xs->getField($field);

		// get commands
		$cmds = array();
		$terms = is_array($term) ? array_unique($term) : array($term);
		$terms = XS::convert($terms, 'UTF-8', $this->xs->getDefaultCharset());
		foreach ($terms as $term) {
			$cmds[] = new XSCommand(XS_CMD_INDEX_REMOVE, 0, $field->vno, strtolower($term));
		}

		// combine multi commands into exdata
		if ($this->_bufSize > 0) {
			$this->appendBuffer(implode('', $cmds));
		} elseif (count($cmds) == 1) {
			$this->execCommand($cmds[0], XS_CMD_OK_RQST_FINISHED);
		} else {
			$cmd = array('cmd' => XS_CMD_INDEX_EXDATA, 'buf' => implode('', $cmds));
			$this->execCommand($cmd, XS_CMD_OK_RQST_FINISHED);
		}
		return $this;
	}

	/**
	 * 批量提交索引命令封包数据
	 * 把多个命令封包内容连续保存为文件或变量, 然后一次性提交以减少网络开销提升性能
	 * @param string $data 要提交的命令封包数据, 或存储命令封包的文件路径, 编码必须已经是 UTF-8
	 * @param bool $check_file 是否检测参数为文件的情况
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @throw XSException 出错时抛出异常
	 */
	public function addExdata($data, $check_file = true)
	{
		if (strlen($data) < 255 && $check_file
				&& file_exists($data) && ($data = file_get_contents($data)) === false) {
			throw new XSException('Failed to read exdata from file');
		}

		// try to check allowed (BUG: check the first cmd only):
		// XS_CMD_IMPORT_HEADER, XS_CMD_INDEX_REQUEST, XS_CMD_INDEX_REMOVE, XS_CMD_INDEX_EXDATA
		$first = ord(substr($data, 0, 1));
		if ($first != XS_CMD_IMPORT_HEADER
				&& $first != XS_CMD_INDEX_REQUEST && $first != XS_CMD_INDEX_SYNONYMS
				&& $first != XS_CMD_INDEX_REMOVE && $first != XS_CMD_INDEX_EXDATA) {
			throw new XSException('Invalid start command of exdata (CMD:' . $first . ')');
		}

		// create cmd & execute it
		$cmd = array('cmd' => XS_CMD_INDEX_EXDATA, 'buf' => $data);
		$this->execCommand($cmd, XS_CMD_OK_RQST_FINISHED);
		return $this;
	}

	/**
	 * 添加同义词
	 * @param string $raw 需要同义的原词, 英文词汇支持用空格分开多个单词并强制被转换为小写
	 * @param string $synonym 同义词条, 最小语素, 勿带空格等分隔符
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @throw XSException 出错时抛出异常
	 * @since 1.3.0
	 */
	public function addSynonym($raw, $synonym)
	{
		$raw = strval($raw);
		$synonym = strval($synonym);
		if ($raw !== '' && $synonym !== '') {
			$cmd = new XSCommand(XS_CMD_INDEX_SYNONYMS, XS_CMD_INDEX_SYNONYMS_ADD, 0, $raw, $synonym);
			if ($this->_bufSize > 0) {
				$this->appendBuffer(strval($cmd));
			} else {
				$this->execCommand($cmd, XS_CMD_OK_RQST_FINISHED);
			}
		}
		return $this;
	}

	/**
	 * 删除某个同义词
	 * @param string $raw 需要同义的原词, 英文词汇支持用空格分开多个单词并强制被转换为小写
	 * @param string $synonym 要删除的同义词条, 默认 null 表示删除原词下的所有同义词
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @throw XSException 出错时抛出异常
	 * @since 1.3.0
	 */
	public function delSynonym($raw, $synonym = null)
	{
		$raw = strval($raw);
		$synonym = $synonym === null ? '' : strval($synonym);
		if ($raw !== '') {
			$cmd = new XSCommand(XS_CMD_INDEX_SYNONYMS, XS_CMD_INDEX_SYNONYMS_DEL, 0, $raw, $synonym);
			if ($this->_bufSize > 0) {
				$this->appendBuffer(strval($cmd));
			} else {
				$this->execCommand($cmd, XS_CMD_OK_RQST_FINISHED);
			}
		}
		return $this;
	}

	/**
	 * 设置当前索引库的分词复合等级
	 * 复合等级是 scws 分词粒度控制的一个重要参数, 是长词细分处理依据, 默认为 3, 值范围 0~15
	 * 注意: 这个设置仅直对当前索引库有效, 多次调用设置值被覆盖仅最后那次设置有效,
	 * 而且仅对设置之后提交的索引数据起作用, 如需对以前的索引数据生效请重建索引.
	 * @param int $level 要设置的分词复合等级
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @since 1.4.7
	 * @throw XSException 出错时抛出异常
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
	 * 获取当前索引库的分词复合等级
	 * @return int 返回当前库的分词复合等级
	 * @see setScwsMulti
	 * @since 1.4.7
	 */
	public function getScwsMulti()
	{
		$cmd = array('cmd' => XS_CMD_SEARCH_SCWS_GET, 'arg1' => XS_CMD_SCWS_GET_MULTI);
		$res = $this->execCommand($cmd, XS_CMD_OK_INFO);
		return intval($res->buf);
	}

	/**
	 * 开启索引命令提交缓冲区
	 * 为优化网络性能, 有必要先将本地提交的 add/update/del 等索引变动指令缓存下来, 
	 * 当总大小达到参数指定的 size 时或调用 {@link closeBuffer} 时再真正提交到服务器
	 * 注意: 此举常用于需要大批量更新索引时, 此外重复调用本函数是无必要的
	 * @param int $size 缓冲区大小, 单位: MB 默认为 4MB
	 * @return XSIndex 返回自身对象以支持串接操作
	 */
	public function openBuffer($size = 4)
	{
		if ($this->_buf !== '') {
			$this->addExdata($this->_buf, false);
		}
		$this->_bufSize = intval($size) << 20;
		$this->_buf = '';
		return $this;
	}

	/**
	 * 提交所有指令并关闭缓冲区
	 * 若未曾打开缓冲区, 调用本方法是无意义的
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see openBuffer
	 */
	public function closeBuffer()
	{
		return $this->openBuffer(0);
	}

	/**
	 * 开始重建索引
	 * 此后所有的索引更新指令将写到临时库, 而不是当前搜索库, 重建完成后调用
	 * {@link endRebuild} 实现平滑重建索引, 重建过程仍可搜索旧的索引库,
	 * 如直接用 {@link clean} 清空数据, 则会导致重建过程搜索到不全的数据
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see endRebuild
	 */
	public function beginRebuild()
	{
		$this->execCommand(array('cmd' => XS_CMD_INDEX_REBUILD, 'arg1' => 0), XS_CMD_OK_DB_REBUILD);
		$this->_rebuild = true;
		return $this;
	}

	/**
	 * 完成并关闭重建索引
	 * 重建完成后调用, 用重建好的索引数据代替旧的索引数据
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see beginRebuild
	 */
	public function endRebuild()
	{
		if ($this->_rebuild === true) {
			$this->_rebuild = false;
			$this->execCommand(array('cmd' => XS_CMD_INDEX_REBUILD, 'arg1' => 1), XS_CMD_OK_DB_REBUILD);
		}
		return $this;
	}

	/**
	 * 中止索引重建
	 * 丢弃重建临时库的所有数据, 恢复成当前搜索库, 主要用于偶尔重建意外中止的情况
	 * @return XSIndex 返回自身对象以支持串接操作
	 * @see beginRebuild
	 * @since 1.3.4
	 */
	public function stopRebuild()
	{
		try {
			$this->execCommand(array('cmd' => XS_CMD_INDEX_REBUILD, 'arg1' => 2), XS_CMD_OK_DB_REBUILD);
			$this->_rebuild = false;
		} catch (XSException $e) {
			if ($e->getCode() !== XS_CMD_ERR_WRONGPLACE) {
				throw $e;
			}
		}
		return $this;
	}

	/**
	 * 更改存放索引数据的目录
	 * 默认索引数据保存到服务器上的 db 目录, 通过此方法修改数据目录名
	 * @param string $name 数据库名称
	 * @return XSIndex 返回自身对象以支持串接操作
	 */
	public function setDb($name)
	{
		$this->execCommand(array('cmd' => XS_CMD_INDEX_SET_DB, 'buf' => $name), XS_CMD_OK_DB_CHANGED);
		return $this;
	}

	/**
	 * 强制刷新服务端当前项目的搜索日志
	 * @return bool 刷新成功返回 true, 失败则返回 false
	 */
	public function flushLogging()
	{
		try {
			$this->execCommand(XS_CMD_FLUSH_LOGGING, XS_CMD_OK_LOG_FLUSHED);
		} catch (XSException $e) {
			if ($e->getCode() === XS_CMD_ERR_BUSY) {
				return false;
			}
			throw $e;
		}
		return true;
	}

	/**
	 * 强制刷新服务端的当前库的索引缓存
	 * @return bool 刷新成功返回 true, 失败则返回 false
	 */
	public function flushIndex()
	{
		try {
			$this->execCommand(XS_CMD_INDEX_COMMIT, XS_CMD_OK_DB_COMMITED);
		} catch (XSException $e) {
			if ($e->getCode() === XS_CMD_ERR_BUSY || $e->getCode() === XS_CMD_ERR_RUNNING) {
				return false;
			}
			throw $e;
		}
		return true;
	}

	/**
	 * 获取自定义词典内容
	 * @return string 自定义词库内容
	 * @throw XSException 出错时抛出异常
	 */
	public function getCustomDict()
	{
		$res = $this->execCommand(XS_CMD_INDEX_USER_DICT, XS_CMD_OK_INFO);
		return $res->buf;
	}

	/**
	 * 设置自定义词典内容
	 * @param string $content 新的词典内容
	 * @throw XSException 出错时抛出异常
	 */
	public function setCustomDict($content)
	{
		$cmd = array('cmd' => XS_CMD_INDEX_USER_DICT, 'arg1' => 1, 'buf' => $content);
		$this->execCommand($cmd, XS_CMD_OK_DICT_SAVED);
	}

	/**
	 * 关闭索引服务端连接
	 */
	public function close($ioerr = false)
	{
		$this->closeBuffer();
		parent::close($ioerr);
	}

	/**
	 * 追加缓冲区命令数据
	 * 若增加后的数据长度达到缓冲区最大值则触发一次服务器提交.
	 * @param string $buf 命令封包数据
	 */
	private function appendBuffer($buf)
	{
		$this->_buf .= $buf;
		if (strlen($this->_buf) >= $this->_bufSize) {
			$this->addExdata($this->_buf, false);
			$this->_buf = '';
		}
	}

	/**
	 * 析构函数
	 * 在此自动关闭开启的 rebuild
	 */
	public function __destruct()
	{
		if ($this->_rebuild === true) {
			try {
				$this->endRebuild();
			} catch (Exception $e) {
				
			}
		}
		foreach (self::$_adds as $srv) {
			$srv->close();
		}
		self::$_adds = array();
		parent::__destruct();
	}
}

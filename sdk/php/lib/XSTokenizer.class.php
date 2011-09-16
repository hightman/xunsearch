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
		if ($arg !== null && $arg !== '')
			$this->arg = $arg;
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		if (strlen($arg) > 1 && substr($arg, 0, 1) == '/' && substr($arg, -1, 1) == '/')
			return preg_split($this->arg, $value);
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
		if ($arg !== null && $arg !== '')
		{
			$this->arg = intval($arg);
			if ($this->arg < 1 || $this->arg > 255)
				throw new XSException('Invalid argument for ' . __CLASS__ . ': ' . $arg);
		}
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		$terms = array();
		for ($i = 0; $i < strlen($value); $i += $this->arg)
		{
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
		if ($arg !== null && $arg !== '')
		{
			$this->arg = intval($arg);
			if ($this->arg < 1 || $this->arg > 255)
				throw new XSException('Invalid argument for ' . __CLASS__ . ': ' . $arg);
		}
	}

	public function getTokens($value, XSDocument $doc = null)
	{
		$terms = array();
		$i = $this->arg;
		while (true)
		{
			$terms[] = substr($value, 0, $i);
			if ($i >= strlen($value))
				break;
			$i += $this->arg;
		}
		return $terms;
	}
}

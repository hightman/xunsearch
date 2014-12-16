<?php
/**
 * Yii-Xunsearch class file
 *
 * @author Hightman <hightman2[at]yahoo[dot]com[dot]cn>
 * @link http://www.xunsearch.com/
 * @version 1.0
 */

/**
 * Xunsearch wrapper as an application component for YiiFramework
 *
 * @method XSIndex getIndex()
 * @method XSSearch getSearch()
 *
 * @author hightman
 * @version $Id$
 * @since 1.0
 */
class EXunSearch extends CApplicationComponent
{
	public $xsRoot, $project, $charset;
	private $_xs, $_scws;

	public function __call($name, $parameters)
	{
		// check methods of xs
		if ($this->_xs !== null && method_exists($this->_xs, $name)) {
			return call_user_func_array(array($this->_xs, $name), $parameters);
		}
		// check methods of index object
		if ($this->_xs !== null && method_exists('XSIndex', $name)) {
			$ret = call_user_func_array(array($this->_xs->index, $name), $parameters);
			if ($ret === $this->_xs->index) {
				return $this;
			}
			return $ret;
		}
		// check methods of search object
		if ($this->_xs !== null && method_exists('XSSearch', $name)) {
			$ret = call_user_func_array(array($this->_xs->search, $name), $parameters);
			if ($ret === $this->_xs->search) {
				return $this;
			}
			return $ret;
		}
		return parent::__call($name, $parameters);
	}

	public function init()
	{
		if ($this->xsRoot === null) {
			$lib = dirname(__FILE__) . '/../../lib/XS.class.php';
		} else {
			if (strpos($this->xsRoot, '.') !== false && strpos($this->xsRoot, DIRECTORY_SEPARATOR) === false) {
				$this->xsRoot = Yii::getPathOfAlias($this->xsRoot);
			}
			$lib = $this->xsRoot . '/' . (is_dir($this->xsRoot . '/sdk') ? '' : 'xunsearch-') . 'sdk/php/lib/XS.php';
		}
		if (!file_exists($lib)) {
			throw new CException('"XS.php" or "XS.class.php" not found, please check value of ' . __CLASS__ . '::$xsRoot');
		}
		if (($path = Yii::getPathOfAlias($this->project)) !== false) {
			$this->project = $path . '.ini';
		}
		require_once $lib;
		$this->_xs = new XS($this->project);
		$this->_xs->setDefaultCharset($this->charset);
	}

	/**
	 * Quickly add a new document (without checking key conflicts)
	 * @param mixed $data XSDocument object or data array to be added
	 */
	public function add($data)
	{
		$this->update($data, true);
	}

	/**
	 * @param mixed $data XSDocument object or data array to be updated
	 * @param boolean $add whether to add directly, default to false
	 */
	public function update($data, $add = false)
	{
		if ($data instanceof XSDocument) {
			$this->_xs->index->update($data, $add);
		} else {
			$doc = new XSDocument($data);
			$this->_xs->index->update($doc, $add);
		}
	}

	/**
	 * @return XSTokenizerScws get scws tokenizer
	 */
	public function getScws()
	{
		if ($this->_scws === null) {
			$this->_scws = new XSTokenizerScws;
		}
		return $this->_scws;
	}
}

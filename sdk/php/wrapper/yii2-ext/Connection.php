<?php
/**
 * xunsearch Connection class file
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2014 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
namespace hightman\xunsearch;

use Yii;
use yii\base\Component;

/**
 * xunsearch Connection is used to manage XS objects
 *
 * @property-read string $version the version of xunsearch sdk.
 *
 * @author xjflyttp <xjflyttp@gmail.com>
 * @author hightman <hightman@twomice.net>
 * @since 1.4.9
 */
class Connection extends Component
{
	/**
	 * @event Event an event that is triggered before a XS instance is created
	 */
	const EVENT_BEFORE_OPEN = 'beforeOpen';

	/**
	 * @var string xunsearch ini file directory
	 * Alias supportted, i.e: '@app/config' or '/path/to/config'
	 * Default to: @vendor/hightman/xunsearch/app
	 */
	public $iniDirectory = '@vendor/hightman/xunsearch/app';

	/**
	 * @var string charset of current app, deafult to utf-8
	 */
	public $charset = 'utf-8';

	/**
	 * @var Database[]
	 */
	private $_databases = [];

	/**
	 * Initializes the object
	 */
	public function init()
	{
		parent::init();
		if (substr($this->iniDirectory, 0, 1) === '@') {
			$this->iniDirectory = Yii::getAlias($this->iniDirectory);
		}
	}

	/**
	 * Get database via calling object self
	 * @param string $name database name
	 * @return Database
	 */
	public function __invoke($name)
	{
		return $this->getDatabase($name);
	}

	/**
	 * @return string sdk version
	 */
	public function getVersion()
	{
		if (!defined('XS_PACKAGE_VERSION')) {
			new \XSException('');
		}
		return XS_PACKAGE_VERSION;
	}

	/**
	 * Get database
	 * @param string $name database name
	 * @param boolean $refresh whether to reestablish the database connection even if it is found in the cache.
	 * @return Database
	 * @throws \XSException
	 */
	public function getDatabase($name, $refresh = false)
	{
		if ($refresh || !array_key_exists($name, $this->_databases)) {
			$this->_databases[$name] = $this->openDatabase($name);
		}
		return $this->_databases[$name];
	}

	/**
	 * Open database
	 * @param string $name database name.
	 * @return Database
	 * @throws \XSException
	 */
	protected function openDatabase($name)
	{
		$this->trigger(self::EVENT_BEFORE_OPEN);
		$iniFile = $this->iniDirectory . '/' . $name . '.ini';
		return Yii::createObject([
				'class' => Database::className(),
				'charset' => $this->charset,
				'iniFile' => $iniFile,
		]);
	}
}

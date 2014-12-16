<?php
/**
 * Web 版开发工具
 * 是一个基于 YiiFramework 的 WebApplication
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
define('YII_DEBUG', true);

// Application configuration
$config = array(
	'basePath' => dirname(__FILE__),
	'name' => 'Xunsearch PHP-SDK',
	'language' => 'zh_cn',
	'sourceLanguage' => 'zh_cn',
	'runtimePath' => dirname(__FILE__) . '/tmp',
	'import' => array(
	),
	'components' => array(
		'assetManager' => array(
			'class' => 'CAssetManager',
			'basePath' => dirname(__FILE__) . '/tmp',
			'baseUrl' => '/xs-php/dev/tmp'
		),
		'urlManager' => array(
			'urlFormat' => 'path',
			'rules' => array(
				'api/<name:.+>' => array('site/api', 'urlSuffix' => '.html'),
				'doc/<name:.+>' => 'site/doc',
				'guide/<name:.+>' => 'site/guide',
			),
		),
	),
);

// Path of YiiFramework, try to read from env variable
$yiiPath = (getenv('YII_PATH') !== false) ? getenv('YII_PATH') : '/Users/hightman/Projects/yii';
require_once($yiiPath . '/framework/yii.php');

// Create the app & run it
Yii::createWebApplication($config)->run();

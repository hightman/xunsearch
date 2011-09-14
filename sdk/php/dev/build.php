#!/usr/bin/env php
<?php
/**
 * 开发工具命令入口文件
 * 是一个基于 YiiFramework 的 ConsoleApplication
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
// Application configuration
$config = array(
	'basePath' => dirname(__FILE__),
	'commandMap' => array(
		'api' => 'application.commands.DocCommand',
	),
);

// Path of YiiFramework, try to read from env variable
$yiiPath = (getenv('YII_PATH') !== false) ? getenv('YII_PATH') : '/Users/hightman/Projects/yii';
require_once($yiiPath . '/framework/yii.php');

// Create the app & run it
Yii::createConsoleApplication($config)->run();

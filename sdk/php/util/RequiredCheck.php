#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 运行条件检测
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
require_once dirname(__FILE__) . '/../lib/XS.php';
require dirname(__FILE__) . '/XSUtil.class.php';

// magick output charset
XSUtil::parseOpt(array('c', 'charset'));
$charset = XSUtil::getOpt('c', 'charset');
XSUtil::setCharset($charset);

// result number record
$result = array(
	'PHP 版本' => array(
		'type' => (version_compare(PHP_VERSION, '5.2.0', '>=') ? '' : 'ERROR:') . PHP_VERSION,
		'used' => 'XS(core)',
		'note' => 'PHP 5.2.0 或更高版本是必须的。',
	),
	'SPL 扩展' => array(
		'type' => extension_loaded('spl') ? 'OK' : 'ERROR',
		'used' => 'XS(core)',
		'note' => 'SPL 扩展用于自动加载和对象戏法',
	),
	'PCRE 扩展' => array(
		'type' => extension_loaded('pcre') ? 'OK' : 'ERROR',
		'used' => 'XSDocument, XSSearch',
		'note' => '用于字符串切割、判断',
	),
	'编码转换' => array(
		'type' => check_conv(),
		'used' => 'XSDocument, XSSearch',
		'note' => '用于支持非 UTF-8 字符集',
	),
	'缓存模块' => array(
		'type' => check_cache(),
		'used' => 'XS',
		'note' => '用于缓存项目配置文件的解析结果',
	),
	'JSON 扩展' => array(
		'type' => extension_loaded('json') ? 'OK' : 'WARNING',
		'used' => 'util.Quest, util.Indexer',
		'note' => '用于读取或输出 JSON 格式的数据',
	),
	'XML 扩展' => array(
		'type' => extension_loaded('xml') ? 'OK' : 'WARNING',
		'used' => 'util.Indexer',
		'note' => '用于读取导入 XML 格式的数据',
	),
	'MySQL 扩展' => array(
		'type' => check_mysql(),
		'used' => 'util.Indexer',
		'note' => '用于读取导入 MySQL 的数据库',
	),
	'SQLite 扩展' => array(
		'type' => check_sqlite(),
		'used' => 'util.Indexer',
		'note' => '用于读取导入 SQLite 的数据库',
	),
);

// output
?>
Xunsearch PHP-SDK 运行需求检查
==============================

检查内容
--------

本程序用于确认您的服务器配置是否能满足运行 Xunsearch PHP-SDK 的要求。
它将检查服务器所运行的 PHP 版本，查看是否安装了合适的PHP扩展模块，以及
确认 php.ini 文件是否正确设置。

<?php
out_line();
out_line('项目', '结果', '用于', '备注');
out_line();
$num_ok = $num_warning = $num_error = 0;
foreach ($result as $key => $val) {
	if (substr($val['type'], 0, 7) == 'WARNING')
		$num_warning++;
	elseif (substr($val['type'], 0, 5) == 'ERROR')
		$num_error++;
	else
		$num_ok++;
	out_line($key, $val['type'], $val['used'], $val['note']);
}
out_line();
?>

检查结果
--------

共计 <?php echo $num_ok; ?> 项通过，<?php echo $num_warning; ?> 项警告，<?php echo $num_error; ?> 项错误。

<?php if ($num_error > 0): ?>
	您的服务器配置不符合 Xunsearch/PHP-SDK 的最低要求。
	请仔细查看上面表格中结果为 ERROR 的项目，并针对性的做出修改和调整。
<?php else: ?>
	您的服务器配置符合 Xunsearch/PHP-SDK 的最低要求。
	<?php if ($num_warning > 0): ?>
		如果您需要使用特定的功能，请关注上述的 WARNING 项。 
	<?php endif; ?>
<?php endif; ?>
<?php

// check conv
function check_conv()
{
	$rec = array();
	if (function_exists('mb_convert_encoding')) {
		$rec[] = 'mbstring';
	}
	if (function_exists('iconv')) {
		$rec[] = 'iconv';
	}
	if (count($rec) === 0) {
		return 'WARNING';
	}
	return current($rec);
}

// check cache
function check_cache()
{
	$rec = array();
	if (function_exists('apc_fetch')) {
		$rec[] = 'apc';
	}
	if (function_exists('xcache_get')) {
		$rec[] = 'xcache';
	}
	if (function_exists('eaccelerator_get')) {
		$rec[] = 'eAccelerator';
	}
	if (count($rec) === 0) {
		return 'WARNING';
	}
	return current($rec);
}

// check mysql
function check_mysql()
{
	$rec = array();
	if (function_exists('mysql_connect')) {
		$rec[] = 'mysql';
	}
	if (class_exists('mysqli')) {
		$rec[] = 'mysqli';
	}
	if (extension_loaded('pdo_mysql')) {
		$rec[] = 'PDO_MySQL';
	}
	if (count($rec) === 0) {
		return 'WARNING';
	}
	return current($rec);
}

// check sqlite
function check_sqlite()
{
	$rec = array();
	if (function_exists('sqlite_open')) {
		$rec[] = 'sqlite';
	}
	if (class_exists('sqlite3')) {
		$rec[] = 'sqlite3';
	}
	if (extension_loaded('pdo_sqlite')) {
		$rec[] = 'PDO_SQLite';
	}
	if (count($rec) === 0) {
		return 'WARNING';
	}
	return current($rec);
}

// output line
function out_line()
{
	$args = func_get_args();
	if (count($args) == 4) {
		printf("| %s | %s | %s | %s |\n", XSUtil::fixWidth($args[0], 10), XSUtil::fixWidth($args[1], 10),
				XSUtil::fixWidth($args[2], 24), XSUtil::fixWidth($args[3], 30));
	} else {
		printf("+-%s-+-%s-+-%s-+-%s-+\n", XSUtil::fixWidth('', 10, '-'), XSUtil::fixWidth('', 10, '-'),
				XSUtil::fixWidth('', 24, '-'), XSUtil::fixWidth('', 30, '-'));
	}
}

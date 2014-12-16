#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 项目配置文件创建、修改向导
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
// check argument
if (!isset($_SERVER['argv'][1])) {
	echo "IniWizzard - Xunsearch 项目配置文件生成、修改工具\n";
	echo "用法：{$_SERVER['argv'][0]} <file>\n";
	echo "例如：{$_SERVER['argv'][0]} ../etc/demo.ini\n";
	exit(-1);
}

// check to write
if (file_exists($_SERVER['argv'][1])) {
	if (!is_writable($_SERVER['argv'][1])) {
		echo "错误：无权限改写配置文件 `}{$_SERVER['argv'][1]}\n";
		exit(-1);
	}
} else {
	if (!@touch($_SERVER['argv'][1])) {
		echo "错误：无权限创建配置文件 `}{$_SERVER['argv'][1]}\n";
		exit(-1);
	}
	unlink($_SERVER['argv'][1]);
}

// @TODO
echo "很抱歉，此功能尚未实现！\n";

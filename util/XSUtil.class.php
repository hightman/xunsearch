<?php
/**
 * XSUtil 类定义文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * XSUtil 工具程序通用代码
 * 
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.util
 */
class XSUtil
{
	private static $optind, $options = null;
	private static $charset = null;

	/**
	 * 将项目参数转换为有效的 ini 文件
	 * @param string $project 用户输入的项目参数
	 * @return string 有效的 ini 配置文件路径
	 */
	public static function toProjectIni($project)
	{
		if (!is_file($project)) {
			$appRoot = getenv('XS_APP_ROOT');
			if ($appRoot === false) {
				$appRoot = defined('XS_APP_ROOT') ? XS_APP_ROOT : dirname(__FILE__) . '/../app';
			}
			return $appRoot . '/' . $project . '.ini';
		} else {
			return $project;
		}
	}

	/**
	 * 修正字符串至固定宽度
	 * 其中一个全角符号、汉字的宽度为半角字符的 2 倍。
	 * @param string $text 要修正的字符串
	 * @param int $size 修正的目标宽度
	 * @param string $pad 用于填充补足的字符
	 * @return string
	 */
	public static function fixWidth($text, $size, $pad = ' ')
	{
		for ($i = $j = 0; $i < strlen($text) && $j < $size; $i++, $j++) {
			if ((ord($text[$i]) & 0xe0) === 0xe0) {
				if (($size - $j) == 1) {
					break;
				}
				$j++;
				$i += 2;
			}
		}
		return substr($text, 0, $i) . str_repeat($pad, $size - $j);
	}

	/**
	 * 设置输出、输入编码
	 * 默认输出的中文编码均为 UTF-8
	 * @param string $charset 期望得到的字符集
	 */
	public static function setCharset($charset)
	{
		if ($charset !== null && strcasecmp($charset, 'utf8') && strcasecmp($charset, 'utf-8')) {
			self::$charset = $charset;
			ob_start(array(__CLASS__, 'convertOut'));
		}
	}

	/**
	 * 把 UTF-8 字符串转换为用户编码
	 * @param string $buf 要转换字符串
	 * @return string 转换后的字符串
	 */
	public static function convertOut($buf)
	{
		if (self::$charset !== null) {
			return XS::convert($buf, self::$charset, 'UTF-8');
		}
		return $buf;
	}

	/**
	 * 把用户输入的字符串转换为 UTF-8 编码
	 * @param string $buf 要转换字符串
	 * @return string 转换后的字符串
	 */
	public static function convertIn($buf)
	{
		if (self::$charset !== null) {
			return XS::convert($buf, 'UTF-8', self::$charset);
		}
		return $buf;
	}

	/**
	 * 解析命令行参数
	 * @param array $valued 需要附加值的参数列表
	 * @return array 解析完的参数数组，未指定 - 开头的选项统一放入 '-' 的子数组
	 */
	public static function parseOpt($valued = array())
	{
		$result = array('-' => array());
		$params = isset($_SERVER['argv']) ? $_SERVER['argv'] : array();
		for ($i = 0; $i < count($params); $i++) {
			if ($params[$i] === '--') {
				for ($i = $i + 1; $i < count($params); $i++) {
					$result['-'][] = $params[$i];
				}
				break;
			} elseif ($params[$i][0] === '-') {
				$value = true;
				$pname = substr($params[$i], 1);
				if ($pname[0] === '-') {
					$pname = substr($pname, 1);
					if (($pos = strpos($pname, '=')) !== false) {
						$value = substr($pname, $pos + 1);
						$pname = substr($pname, 0, $pos);
					}
				} elseif (strlen($pname) > 1) {
					for ($j = 1; $j < strlen($params[$i]); $j++) {
						$pname = substr($params[$i], $j, 1);
						if (in_array($pname, $valued)) {
							$value = substr($params[$i], $j + 1);
							break;
						} elseif (($j + 1) != strlen($params[$i])) {
							$result[$pname] = true;
						}
					}
				}
				if ($value === true && in_array($pname, $valued) && isset($params[$i + 1])) {
					$value = $params[$i + 1];
					$i++;
				}
				$result[$pname] = $value;
			} else {
				$result['-'][] = $params[$i];
			}
		}
		self::$options = $result;
		self::$optind = 1;
		return $result;
	}

	/**
	 * 取得命令行参数
	 * 要求事先调用 parseOpt, 否则会自动以默认参数调用它。
	 * @param string $short 短参数名
	 * @param string $long 长参数名
	 * @param bool $extra 是否补用默认顺序的参数
	 * @return string 返回可用的参数值，若不存在则返回 null
	 * @see parseOpt
	 */
	public static function getOpt($short, $long = null, $extra = false)
	{
		if (self::$options === null) {
			self::parseOpt();
		}

		$value = null;
		$options = self::$options;
		if ($long !== null && isset($options[$long])) {
			$value = $options[$long];
		} elseif ($short !== null && isset($options[$short])) {
			$value = $options[$short];
		} elseif ($extra === true && isset($options['-'][self::$optind])) {
			$value = $options['-'][self::$optind];
			self::$optind++;
		}
		return $value;
	}

	/**
	 * 刷新标准输出缓冲区
	 */
	public static function flush()
	{
		flush();
		if (ob_get_level() > 0) {
			ob_flush();
		}
	}

	/**
	 * 拷贝一个目录及其子目录文件
	 */
	public static function copyDir($src, $dst)
	{
		if (!($dir = @dir($src)) || (!is_dir($dst) && !@mkdir($dst, 0755, true))) {
			return false;
		}
		while (($entry = $dir->read()) !== false) {
			if ($entry === '.' || $entry === '..') {
				continue;
			}
			$psrc = $src . DIRECTORY_SEPARATOR . $entry;
			$pdst = $dst . DIRECTORY_SEPARATOR . $entry;
			if (is_dir($pdst)) {
				self::copyDir($psrc, $pdst);
			} else {
				@copy($psrc, $pdst);
			}
		}
		$dir->close();
		return true;
	}
}

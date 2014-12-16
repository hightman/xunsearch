<?php
/**
 * PinyinCommand 文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 该指令用于生成拼音库，并提供拼音转换、分割测试
 * 
 * 拼音数据采用 UTF-8 编码，每行一条记录，多音及原文之音用空格分隔，位于 ../data/py.txt ；
 * 音节数据每行一条记录，位于 ../data/yj.txt 。
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.dev
 */
class PinyinCommand extends CConsoleCommand
{
	const MAX_WORD_LEN = 12;

	private $inputPath, $dictFile;

	public function __construct($name, $runner)
	{
		parent::__construct($name, $runner);
		$this->inputPath = realpath(dirname(__FILE__) . '/..') . '/data';
		$this->dictFile = realpath(dirname(__FILE__) . '/../../../../etc') . '/py.xdb';
	}

	public function getHelp()
	{
		return <<<EOF
用法
  {$_SERVER['argv'][0]} pinyin make [--output=词典路径] [--py=拼音文件] [--yj=音节文件]
  {$_SERVER['argv'][0]} pinyin test <拼音、中文字符串>

描述
  这条命令用于生成拼音库，或测试转换、分割拼音。

参数
 * output: 输出的词典路径，默认为：{$this->dictFile}
 * py: 文本格式的拼音记录文件，默认为：data/py.txt
 * yj: 文本格式的拼音音节文件，默认为：data/yj.txt

EOF;
	}

	public function actionIndex()
	{
		echo $this->getHelp();
	}

	public function actionTest($args = array())
	{
		if (!isset($args[0]) || $args[0] === '')
			$this->usageError('请指定要测试的拼音、中文字符串！');

		$input = $args[0];
		require_once Yii::getPathOfAlias('application.vendors.PinYin') . '.class.php';
		if (preg_match('/[\x80-\xff]/', $input)) {
			echo "拼音转换结果：$input\n";
			$py = new PinYin($this->dictFile);
			print_r($py->convert($input));
		} else {
			echo "拼音分割结果：$input\n";
			print_r(PinYin::segment($input));
		}
	}

	public function actionMake($output = null, $py = null, $yj = null)
	{
		if ($output === null)
			$output = $this->dictFile;
		if ($py === null)
			$py = $this->inputPath . '/py.txt';
		if ($yj === null)
			$yj = $this->inputPath . '/yj.txt';

		if (file_exists($output) && !unlink($output))
			$this->usageError('输出文件已存在并且不可删除，请用 --output=... 指定路径！');

		if (!file_exists($py) || !($fd = fopen($py, 'r')))
			$this->usageError('拼音文件不存在或打开失败，请用 --py=... 来指定！');

		require_once Yii::getPathOfAlias('application.vendors.xdb') . '.class.php';
		$xdb = new XTreeDB;
		if (!$xdb->Open($output, 'w')) {
			fclose($fd);
			$this->usageError('无法以写入方式打开输出文件，请使用 --output=... 指定路径！');
		}

		echo "拼音文件：" . $py . "\n";
		echo "输出文件：" . $output . "\n";
		echo "开始制作拼音库，正在加载拼音列表 ... ";

		$znum = 0;
		$mchars = $words = array();
		while ($line = fgets($fd, 256)) {
			$line = trim($line);
			if (substr($line, 0, 1) === '#' || $line === '')
				continue;
			list($key, $value) = explode(' ', $line, 2);
			$value = trim($value);
			if (strlen($key) > 3) {
				if (($pos = strpos($value, ' ')) !== false)
					$value = substr($value, 0, $pos);
				$words[$key] = $value;
			}
			else {
				if (strpos($value, ' ') !== false) {
					$values = array_unique(preg_split('/\s+/', $value));
					if (count($values) > 1)
						$mchars[$key] = implode(' ', $values);
					$value = $values[0];
				}
				$xdb->Put($key, $value);
				$znum++;
			}
		}
		fclose($fd);
		echo "完成，共 " . $znum . " 个字，" . count($mchars) . " 个多音字，" . count($words) . " 个词组。\n";
		echo "开始分析包含多音字的词组 ... ";

		$add_num = $skip_num = $max_len = 0;
		foreach ($words as $word => $value) {
			$save = false;
			for ($off = 0; $off < strlen($word); $off += 3) {
				$char = substr($word, $off, 3);
				if (isset($mchars[$char])) {
					$save = true;
					break;
				}
			}
			if (!$save || strlen($word) > self::MAX_WORD_LEN)
				$skip_num++;
			else {
				$add_num++;
				$xdb->Put($word, $value);
				if (strlen($word) > $max_len)
					$max_len = strlen($word);
			}
		}
		echo "完成，共添加 $add_num 个，跳过 $skip_num 个，最大长词为 $max_len 字节。\n";

		if (file_exists($yj)) {
			echo "开始加载音节数据 ... ";
			$lines = file($yj);
			$yinjie = array();
			foreach ($lines as $line) {
				$line = trim($line);
				if ($line === '')
					continue;
				if (isset($yinjie[$line]))
					$yinjie[$line] |= 0x01;
				else
					$yinjie[$line] = 0x01;
				for ($i = 1; $i < strlen($line); $i++) {
					$part = substr($line, 0, $i);
					if (isset($yinjie[$part]))
						$yinjie[$part] |= 0x02;
					else
						$yinjie[$part] = 0x02;
				}
			}
			foreach ($yinjie as $key => $value) {
				$xdb->Put($key, $value);
			}
			echo "完成，共计 " . count($lines) . " 个拼音，合计 " . count($yinjie) . " 条记录。\n";
		}
		echo "正在优化整理数据库 ... ";

		$xdb->Optimize();
		$xdb->Close();
		echo "完成！\n";
	}
}

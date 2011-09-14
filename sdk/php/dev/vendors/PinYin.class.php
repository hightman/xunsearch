<?php
/**
 * PinYin 拼音操作类
 * 注意: 本代码并不完美, 目前 XS 没有采用, 而是用 纯C 实现
 * 
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 拼音操作类
 * 提供拼音转换功能, 将中英、拼音混合的字符串智能转换为拼音、模糊间、缩写
 * 
 * 转换示例:
 * <code>
 * 中国 => zhongguo + zhonguo + zg
 * 西安 => xi'an + NULL + xa
 * php教程 => phpjiaocheng + phpjiaochen + phpjc
 * php mysql => php mysql + NULL + NULL
 * 新 php => xinphp + NULL + xphp
 * xin php => xin_php
 * 中国renmin => zhongguoren + zhonguorenmin + zgrm
 * 
 * 注: 非拼音的部分加上前缀 _
 * </code>
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.tools
 */
class PinYin
{
	/**
	 * 词拼音的最大长度
	 */
	const MAX_WORD_LEN = 12;

	/**
	 * @staticvar array 声音母列
	 */
	private static $sm = array(
		'b' => 1, 'p' => 1, 'm' => 1, 'f' => 1, 'd' => 1,
		't' => 1, 'n' => 1, 'l' => 1, 'g' => 1, 'k' => 1,
		'h' => 1, 'j' => 1, 'q' => 1, 'x' => 1, 'r' => 1,
		'z' => 1, 'c' => 1, 's' => 1, 'y' => 1, 'w' => 1,
		'zh' => 1, 'ch' => 1, 'sh' => 1
	);
	/**
	 * @staticvar array 韵母列表
	 * 其值含议: 0x01-完整韵母, 0x02-后面还有更多, 0x04-独立音节
	 */
	private static $ym = array(
		'a' => 7, 'o' => 7, 'e' => 7, 'ai' => 5, 'ei' => 5,
		'er' => 5, 'ao' => 5, 'ou' => 5, 'an' => 7, 'en' => 7,
		'ang' => 5, 'i' => 3, 'u' => 3, 'v' => 3, 'ia' => 3,
		'ua' => 3, 'uo' => 1, 'ie' => 1, 've' => 1, 'uai' => 1,
		'uei' => 1, 'ui' => 1, 'iao' => 1, 'iou' => 1, 'ian' => 3,
		'uan' => 3, 'in' => 3, 'uen' => 3, 'un' => 1, 'iang' => 1,
		'uang' => 1, 'eng' => 1, 'ing' => 1, 'ueng' => 1, 'ong' => 1,
		'iong' => 1, 'iu' => 1, 'ue' => 3, 'io' => 2, 'on' => 2, 'ion' => 2
	);
	private $xdb = null;

	/**
	 * 构造函数
	 * 打开并加载拼音词库
	 */
	public function __construct($file = null)
	{
		require_once dirname(__FILE__) . '/xdb.class.php';
		
		if ($file === null)
			$file = dirname(__FILE__) . '/py.xdb';
		$xdb = new XTreeDB;
		if (!$xdb->Open($file, 'r'))
			trigger_error('Failed to load pinyin database: ' . $file, E_USER_ERROR);
		$this->xdb = $xdb;
	}

	/**
	 * 析构函数
	 * 关闭和释放词典资源
	 */
	public function __destruct()
	{
		if (is_object($this->xdb))
			$this->xdb->Close();
		$this->xdb = null;
	}

	/**
	 * 转换字符串中的中文为拼音
	 * @param string $str 要转换的字符串
	 * @return array 转换后的原始拼音、模糊音、缩写
	 */
	public function convert($str)
	{
		$raw = $abbr = $blur = '';
		$lastmb = true;
		$parts = self::coarseSplit($str);
		foreach ($parts as $part)
		{
			if ((ord($part[0]) & 0xc0) === 0xc0)
			{
				for ($i = 0; $i < strlen($part); $i += 3)
				{
					$j = min(strlen($part), $i + self::MAX_WORD_LEN);
					while ($j > $i)
					{
						// query from py lib
						$res = $this->xdb->Get(substr($part, $i, $j - $i));
						$j = $j - 3;
						if ($res === false)
							continue;
						// check result
						if ($raw !== '' && !isset(self::$sm[$res[0]]))
							$raw .= "'";
						$raw .= $res;
						$pys = ($j > $i) ? self::segment($res) : array($res);
						foreach ($pys as $tmp)
						{
							if (substr($tmp, 0, 1) === ' ')
								break;
							$abbr .= substr($tmp, 0, 1);
							if (substr($tmp, 1, 1) == 'h')
								$tmp = substr($tmp, 0, 1) . substr($tmp, 2);
							if (substr($tmp, -2, 2) == 'ng')
								$tmp = substr($tmp, 0, -1);
							$blur .= $tmp;
						}
						$i = $j;
						break;
					}
				}
				$lastmb = true;
			}
			else
			{
				if ($lastmb || self::isFullPy($part))
				{
					$raw .= $part;
					$abbr .= $part;
					$blur .= $part;
				}
				else
				{
					$raw .= ' ' . $part;
					$abbr .= ' ' . $part;
					$blur .= ' ' . $part;
				}
				$lastmb = false;
			}
		}

		// return value
		$result = array('raw' => $raw);
		$result['blur'] = $raw === $blur ? null : $blur;
		$result['abbr'] = $raw === $abbr ? null : $abbr;
		return $result;
	}

	/**
	 * 把粘合在一起的拼音分段
	 * @param string $str 要切分的拼音组合
	 * @return array 分好的数组 (无法切分的部分以下划线开头)
	 */
	public static function segment($str)
	{
		$ret = array();
		for ($i = 0; $i < strlen($str); $i++)
		{
			$ch = $str[$i];
			if ($ch === "'" && $str[$i + 1] !== "'")
				continue;
			// fetch SM
			$sm = '';
			if (isset(self::$sm[$ch]))
			{
				// fetch SM				
				$sm = substr($str, $i, 2);
				$i += 2;
				if (!isset(self::$sm[$sm]))
				{
					$sm = $ch;
					$i--;
				}
			}

			//echo "Get SM: $sm\n";
			// fetch YM
			$ym0 = $ym1 = 0;
			for ($j = $i; $j < strlen($str); $j++)
			{
				$yy = substr($str, $i, $j - $i + 1);
				$ym1 = isset(self::$ym[$yy]) ? self::$ym[$yy] : 0;

				//echo "Check YM: $yy\n";	
				// invalid YM
				if ($ym1 === 0 || ($sm === '' && !($ym1 & 0x04)))
				{
					$j--;
					break;
				}
				// end with SM char, & next is not SM char
				if (isset(self::$sm[$str[$j]]) && ($j + 1) !== strlen($str) && ($ym0 & 0x01)
					&& !isset(self::$sm[$str[$j + 1]]) && isset(self::$ym[$str[$j + 1]]))
				{
					$j--;
					break;
				}
				// stop check YM
				$ym0 = $ym1;
				if (!($ym1 & 0x02))
					break;
			}
			if ($j < $i || $i >= strlen($str) || !($ym0 & 0x01))
			{				
				//echo "Invalid SM: " . $sm . "\n";
				$ret[] = ' ' . $sm . substr($str, $i);
				break;
			}
			else
			{
				// check invalid YM
				$ret[] = $sm . substr($str, $i, $j - $i + 1);
				$i = $j;
			}
		}
		return $ret;
	}

	/**
	 * 按字节粗分字符串
	 * @param string $str 要切割的字符串 (UTF-8)
	 * @return array 粗分的结果
	 */
	private static function coarseSplit($str)
	{
		$ret = array();
		for ($i = 0; $i < strlen($str); $i++)
		{
			$ch = ord($str[$i]);
			if ($ch <= 0x20 || ($ch & 0xc0) === 0x80)
				continue;
			if (($ch & 0xc0) === 0xc0)
			{
				// multi-bytes (3)
				for ($j = $i + 3; $j < strlen($str); $j += 3)
				{
					if ((ord($str[$j]) & 0xc0) !== 0xc0)
						break;
				}
				$ret[] = substr($str, $i, $j - $i);
			}
			else
			{
				// single-byte (1)
				for ($j = $i + 1; $j < strlen($str); $j++)
				{
					$ch = ord($str[$j]);
					if ($ch <= 0x20 || ($ch & 0x80))
						break;
				}
				$ret[] = substr($str, $i, $j - $i);
			}
			$i = $j - 1;
		}
		return $ret;
	}

	/**
	 * 判断一个字符串是否为完整的音节
	 * @param string $str 要判断的字符串
	 * @return bool 成功返回 true, 失败返回 false
	 */
	private static function isFullPy($str)
	{
		$ret = self::segment($str);
		return count($ret) > 0 && substr($ret[count($ret) - 1], 0, 1) !== ' ';
	}
}


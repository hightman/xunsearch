<?php
/**
 * LiteCommand 文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */

/**
 * 该指令把 xs 类库文件合并一起生成单一文件，并删除其中的各种注释
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.dev
 */
class LiteCommand extends CConsoleCommand
{

	public function getHelp()
	{
		return <<<EOF
用法
  {$_SERVER['argv'][0]} lite

描述
  这条命令合并 ../lib/*.class.php 文件生成单一的库文件 XS.php ，
  同时移除源码中的注释和多余的字符。这个文件用于发布，作为开发者的唯一包含
  文件，方便移动到其它位置。

EOF;
	}

	public function run($args)
	{
		$update = date('Y/m/d H:i:s');
		$comment = <<<EOF
/**
 * Xunsearch PHP-SDK 引导文件
 *
 * 这个文件是由开发工具中的 'build lite' 指令智能合并类定义的源码文件
 * 并删除所有注释而自动生成的。
 * 
 * 当您编写搜索项目时，先通过 require 引入该文件即可使用所有的 PHP-SDK
 * 功能。合并的主要目的是便于拷贝，只要复制这个库文件即可，而不用拷贝一
 * 大堆文件。详细文档请阅读 {@link:http://www.xunsearch.com/doc/php/}
 * 
 * 切勿手动修改本文件！生成时间：{$update} 
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version \$Id\$
 */
EOF;
		echo "开始加载文件 ... ";
		$root = dirname(__FILE__) . '/../../lib';
		$content = @file_get_contents($root . '/xs_cmd.inc.php');
		if ($content === false || strpos($content, 'CMD_NONE') === false)
		{
			echo "ERROR: xs_cmd.inc.php 文件不存在或包含无效内容。\n";
			exit(0);
		}
		$files = glob($root . '/*.class.php');
		sort($files);
		foreach ($files as $file)
		{
			$content .= "\n" . file_get_contents($file);
		}
		echo "完成，共计 " . (count($files) + 1) . " 个文件 (" . number_format(strlen($content)) . " 字节)\n";
		echo "开始清除注释和空行 ... ";
		$content = "<?php\n" . preg_replace('/^(\?>|<\?php)/m', '', $content) . "\n";
		$content = $this->stripComments($content);
		$content = preg_replace('/^include(_once)?.*\s*;\s*$/m', '', $content);
		$content = preg_replace('/^spl_autoload_register\s*\(.*$/m', '', $content);		
		$content = $this->stripEmptyLines($content);
		$content = substr_replace($content, $comment . "\n", 6, 0);
		echo "完成，最终大小为 " . number_format(strlen($content)) . " 字节\n";
		echo "开始写入结果文件 XS.php ... ";
		file_put_contents($root . '/XS.php', $content);
		echo "完成！\n";
	}

	private function stripComments($source)
	{
		$tokens = token_get_all($source);
		$output = '';
		foreach ($tokens as $token)
		{
			if (is_string($token))
				$output .= $token;
			else
			{
				list($id, $text) = $token;
				switch ($id)
				{
					case T_DOC_COMMENT:
						break;
					default: $output .= $text;
						break;
				}
			}
		}
		$output = preg_replace('/^\s*\/[\/\*].*/m', '', $output);
		return $output;
	}

	private function stripEmptyLines($string)
	{
		$string = preg_replace("/[\r\n]+[\s\t]*[\r\n]+/", "\n", $string);
		$string = preg_replace("/^[\s\t]*[\r\n]+/", "", $string);
		return $string;
	}
}

<?php
/**
 * DocCommand 文件
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
require_once Yii::getPathOfAlias('system') . '/../build/commands/ApiCommand.php';

/**
 * 该指令生成 xs 类库的 API 文档，还将 markdown 格式的文档转换为 HTML
 *
 * @author hightman <hightman@twomice.net>
 * @version 1.0.0
 * @package XS.dev
 */
class DocCommand extends ApiCommand
{
	public $baseSourcePath;
	public $baseSourceUrl = 'https://github.com/hightman/xunsearch/blob/master/';
	private $root;

	public function getHelp()
	{
		return <<<EOF
用法
  {$_SERVER['argv'][0]} api [mode]

描述
  这条命令分析 ../lib/*.class.php 的文档注释并生成 HTML 格式文档，方便最终浏览。
  生成的 HTML 文档位于 ../doc/api

参数
  mode 可选的，其值为 offline 或 online 默认为 offline

EOF;
	}

	public function run($args)
	{
		$options = array(
			'fileTypes' => array('php'),
			'exclude' => array(
				'.svn',
				'CVS',
				'/XS.php',
			),
		);

		$this->root = realpath(dirname(__FILE__) . '/../..');
		$input = $this->root . '/lib';
		$output = $this->root . '/doc';
		$themePath = dirname(__FILE__) . '/api';
		$vfile = $this->root . '/../../VERSION';

		include_once $input . '/xs_cmd.inc.php';
		$this->version = file_exists($vfile) ? trim(file_get_contents($vfile)) : PACKAGE_VERSION;
		$this->pageTitle = 'API 文档参考 (Xunsearch/PHP-SDK)';
		$this->baseSourcePath = dirname(dirname(($this->root)));

		echo "\n创建项目.. : " . $this->pageTitle . "\n";
		echo "版本...... : " . $this->version . "\n";
		echo "源码目录.. : " . $input . "\n";
		echo "源码网址.. : " . $this->baseSourceUrl . "\n\n";

		echo "生成类对象文档 ... \n";
		$model = $this->buildModel($input, $options);
		$this->classes = $model->classes;
		$this->packages = $model->packages;

		echo "清理旧有 API 文档 ...\n";
		exec('rm -rf ' . $output . '/api');

		echo "生成页面索引 ... ";
		if (isset($args[0]) && $args[0] == 'online')
		{
			$this->buildOnlinePages($output . '/api', $themePath);
			$this->buildKeywords($output);
			$this->buildPackages($output);
		}
		else
		{
			$this->buildOfflinePages($output . '/api', $themePath);
		}
		echo "完成\n\n";
	}

	public function renderSourceLink($sourcePath, $line = null)
	{
		if (!strncmp($sourcePath, $this->baseSourcePath, strlen($this->baseSourcePath)))
			$sourcePath = substr($sourcePath, strlen($this->baseSourcePath) + 1);
		if ($line === null)
			return CHtml::link($sourcePath, $this->baseSourceUrl . $sourcePath, array('class' => 'sourceLink'));
		else
			return CHtml::link($sourcePath . '#L' . $line, $this->baseSourceUrl . $sourcePath . '#L' . $line, array('class' => 'sourceLink'));
	}

	public function renderPartial($view, $data = null, $return = false)
	{
		if ($view !== 'sourceCode')
			return parent::renderPartial($view, $data, $return);
		$object = $data['object'];
		$html = CHtml::openTag('div', array('class' => 'sourceCode'));
		$html .= CHtml::tag('b', array(), '源码：') . ' ';
		$html .= $this->renderSourceLink($object->sourcePath, $object->startLine);
		$html .= ' (<b><a href="#" class="show">显示</a></b>)';
		$html .= CHtml::tag('div', array('class' => 'code'), $this->highlight($this->getSourceCode($object)));
		$html .= CHtml::closeTag('div');
		if ($return)
			return $html;
		echo $html;
	}

	protected function getSourceCode($object)
	{
		$sourcePath = (file_exists($object->sourcePath) ? $object->sourcePath : YII_PATH . $object->sourcePath);
		$lines = file($sourcePath);
		return implode("", array_slice($lines, $object->startLine - 1, $object->endLine - $object->startLine + 1));
	}

	protected function buildModel($sourcePath, $options)
	{
		$files = CFileHelper::findFiles($sourcePath, $options);
		$files[] = $this->root . '/util/XSDataSource.class.php';
		$files[] = $this->root . '/util/XSUtil.class.php';
		$model = new ApiModel;
		$model->build($files);
		return $model;
	}

	protected function fixOnlineLink($matches)
	{
		if (($pos = strpos($matches[1], '::')) !== false)
		{
			$className = substr($matches[1], 0, $pos);
			$method = substr($matches[1], $pos + 2);
			if ($className === 'index')
				return "<a href=\"/doc/php/api/#{$method}\">{$matches[2]}</a>";
			else
				return "<a href=\"/doc/php/api/{$className}#{$method}\">{$matches[2]}</a>";
		}
		else
		{
			if ($matches[1] === 'index')
				return "<a href=\"/doc/php/api/\">{$matches[2]}</a>";
			else
				return "<a href=\"/doc/php/api/{$matches[1]}\">{$matches[2]}</a>";
		}
	}
}

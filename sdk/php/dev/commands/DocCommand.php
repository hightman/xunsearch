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
	private $root, $output, $online;
	private static $_parser;

	public function getHelp()
	{
		return <<<EOF
用法
  {$_SERVER['argv'][0]} doc [mode] [output_dir]

描述
  这条命令用于自动根据代码注释和 Markdown 文档生成 HTML 格式文档、CHM 索引文件。
  方便最终浏览和使用。

参数
  mode 可选参数，其值为 offline 或 online ，默认为 offline  
  output_dir 文档输出目录，可选参数，在此目录下生成相关的 HTML 文档
  当 mode 为 online 时，输出目录默认为 ../doc 并且只生成 API 文档；
  当 mode 为 offline 时，输出目录默认为 ../doc/html 并在此目录下生成 api/guide 全部文档。

EOF;
	}

	public function run($args)
	{
		// basic
		$this->root = realpath(dirname(__FILE__) . '/../..');
		$this->online = (isset($args[0]) && !strcasecmp($args[0], 'online')) ? true : false;

		include_once $this->root . '/lib/xs_cmd.inc.php';
		$vfile = $this->root . '/../../VERSION';
		$this->version = file_exists($vfile) ? trim(file_get_contents($vfile)) : PACKAGE_VERSION;

		if (isset($args[1])) {
			$output = $args[1];
		} else {
			$output = $this->root . '/doc';
			if (!$this->online) {
				$output .= '/html';
			}
		}

		// create output dir
		if (!is_dir($output) && !@mkdir($output))
			$this->usageError("输出目录 {$output} 不存在，并且无法创建！");
		$this->output = $output;

		echo "基础目录.. : " . $this->root . "\n";
		echo "输出目录.. : " . $this->output . "\n";
		echo "版本...... : " . $this->version . "(" . ($this->online ? "线上" : "线下") . ")\n";
		echo "代码网址.. : " . $this->baseSourceUrl . "\n\n";


		// 1. 生成 API 文档
		$themePath = dirname(__FILE__) . '/api';
		$this->pageTitle = 'Xunsearch PHP-SDK API 文档';
		$this->baseSourcePath = dirname(dirname(($this->root)));

		echo "分析类对象文档 ... ";
		$model = $this->buildModel($this->root . '/lib', array(
			'fileTypes' => array('php'),
			'exclude' => array('/XS.php'))
		);
		$this->classes = $model->classes;
		$this->packages = $model->packages;
		echo "共 " . count($this->packages) . " 个包，" . count($this->classes) . " 个类对象\n";

		echo "生成 HTML 页面 ... ";
		if ($this->online) {
			$this->buildOnlinePages($output . '/api', $themePath);
			$this->buildKeywords($output);
			$this->buildPackages($output);
		} else {
			$this->buildOfflinePages($output . '/api', $themePath);
		}
		echo "完成\n\n";

		// 2. 离线方式，增加 guide 文档
		if ($this->online === false) {
			$data = array('guides' => array(), 'others' => array());
			echo "生成权威指南 ...\n";
			$this->themePath = $themePath;

			@mkdir($this->output . '/guide');
			$this->buildGuidePage('toc');
			$guides = $this->loadGuideList();
			for ($i = $j = 0; $i < count($guides); $i++) {
				if (!isset($guides[$i]['name'])) {
					$data['guides'][] = array('label' => $guides[$i]['label'], 'items' => array());
					continue;
				}
				$k = count($data['guides']) - 1;
				$data['guides'][$k]['items'][$guides[$i]['name']] = $guides[$i]['label'];
				$options = array();
				if ($j !== 0)
					$options['prev'] = $guides[$j];
				if (isset($guides[$i + 1])) {
					if (isset($guides[$i + 1]['name']))
						$options['next'] = $guides[$i + 1];
					elseif (isset($guides[$i + 2]))
						$options['next'] = $guides[$i + 2];
				}
				$this->buildGuidePage($guides[$i]['name'], $options);
				$j = $i;
			}

			// 3. 其它文档			
			echo "生成其它相关文档 ...\n";
			$this->buildGuidePage('README.md', array('name' => 'index'));
			$others = array('ABOUT', 'FEATURE', 'ARCHITECTURE', 'DOWNLOAD', 'SUPPORT', 'LICENSE');
			foreach ($others as $name) {
				$this->buildGuidePage($name);
				$data['others'][$name] = $this->pageTitle;
			}

			// 4. 创建 CHM 总索引
			$content = $this->render('chmProject2', null, true, null);
			file_put_contents($this->output . '/xs_php_manual.hhp', $content);

			$content = $this->render('chmIndex2', $data, true, null);
			file_put_contents($this->output . '/xs_php_manual.hhk', $content);

			$content = $this->render('chmContents2', $data, true, null);
			file_put_contents($this->output . '/xs_php_manual.hhc', $content);
		}
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

	public function render($view, $data = null, $return = false, $layout = 'main')
	{
		if ($layout === null)
			$html = $this->renderPartial($view, $data, true);
		else
			$html = parent::render($view, $data, true, $layout);
		if ($this->online === false) {
			$html = mb_convert_encoding($html, 'GBK', 'UTF-8');
			$html = str_replace('; charset=utf-8"', '; charset=gbk"', $html);
		}
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
		if (($pos = strpos($matches[1], '::')) !== false) {
			$className = substr($matches[1], 0, $pos);
			$method = substr($matches[1], $pos + 2);
			if ($className === 'index')
				return "<a href=\"/doc/php/api/#{$method}\">{$matches[2]}</a>";
			else
				return "<a href=\"/doc/php/api/{$className}#{$method}\">{$matches[2]}</a>";
		}
		else {
			if ($matches[1] === 'index')
				return "<a href=\"/doc/php/api/\">{$matches[2]}</a>";
			else
				return "<a href=\"/doc/php/api/{$matches[1]}\">{$matches[2]}</a>";
		}
	}

	protected function loadGuideList()
	{
		// dot flag: *, -
		$list = array();
		$lines = file($this->root . '/doc/guide/toc.txt');
		foreach ($lines as $line) {
			$line = trim($line);
			if ($line === '')
				continue;
			if ($line[0] === '*')
				$list[] = array('label' => substr($line, 2));
			elseif ($line[0] === '-') {
				list ($label, $name) = explode('](', substr($line, 3, -1), 2);
				$list[] = array('label' => $label, 'name' => $name);
			}
		}
		return $list;
	}

	protected function buildGuidePage($name, $options = array())
	{
		$ord = ord(substr($name, 0, 1));
		$name2 = isset($options['name']) ? $options['name'] : $name;
		$input = $this->root . '/doc/';
		$output = $this->output . '/';
		if ($ord >= 65 && $ord <= 90) {
			$input .= $name;
			$output .= $name2 . '.html';
		} else {
			$input .= 'guide/' . $name . '.txt';
			$output .= 'guide/' . $name2 . '.html';
		}

		$options['content'] = preg_replace('#\]\(([a-z]+\.[a-z]+)\)#', ']($1.html)', @file_get_contents($input));
		$this->pageTitle = trim(substr($options['content'], 0, strpos($options['content'], '===')));
		$content = $this->render('guide', $options, true, null);
		if ($ord >= 65 && $ord <= 90)
			$content = str_replace('../api/css/', 'api/css/', $content);
		if ($name == 'README.md')
			$content = preg_replace('#"http://www.xunsearch.com/doc/php/(.+?)"#', '"$1.html"', $content);
		file_put_contents($output, $content);
	}

	protected function formatMarkdown($data)
	{
		if (self::$_parser === null) {
			Yii::import('application.vendors.XMarkdown', true);
			self::$_parser = new XMarkdown;
		}
		return self::$_parser->transform($data);
	}
}


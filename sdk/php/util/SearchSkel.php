#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 搜索骨架代码生成工具
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
$lib_file = dirname(__FILE__) . '/../lib/XS.php';
if (!file_exists($lib_file)) {
	$lib_file = dirname(__FILE__) . '/../lib/XS.class.php';
}
require_once $lib_file;
require_once dirname(__FILE__) . '/XSUtil.class.php';

// check arguments
XSUtil::parseOpt(array('p', 'o', 'project', 'output'));
$project = XSUtil::getOpt('p', 'project', true);

if (XSUtil::getOpt('h', 'help') !== null || !is_string($project)) {
	$version = XS_PACKAGE_NAME . '/' . XS_PACKAGE_VERSION;
	echo <<<EOF
SearchSkel - 搜索骨架代码生成工具 ($version)

用法
    {$_SERVER['argv'][0]} [options] [-p|--project] <project> [[-o|--output] <dir>]
	
选项说明
    --project=<name|ini>
    -p <project> 用于指定要搜索的项目名称或项目配置文件的路径，
                 如果指定的是名称，则使用 ../app/<name>.ini 作为配置文件
    --output=<dir>	
    -o <dir>     指定生成的骨架代码存储位置，默认为当前目录
    -h|--help    显示帮助信息

EOF;
	exit(0);
}

// output dir
$output = XSUtil::getOpt('o', 'output', true);
if ($output === null) {
	$output = '.';
}
if (!is_dir($output)) {
	echo "错误：输出目录 ($output) 不是一个有效的目录。\n";
	exit(-1);
}
if (!is_writable($output)) {
	echo "错误：输出目录 ($output) 不可写入，请注意检查权限。\n";
	exit(-1);
}

// create xs project
$ini = XSUtil::toProjectIni($project);
if (!file_exists($ini)) {
	echo "错误：无效的项目名称 ($project)，不存在相应的配置文件。\n";
	exit(-1);
}

// execute the search
try {
	// create xs object
	echo "初始化项目对象 ...\n";
	$xs = new XS($ini);

	// generate varialbes
	echo "解析字段，生成变量清单 ...\n";
	$vars = array();

	// timezone
	if (!ini_get('date.timezone')) {
		date_default_timezone_set('Asia/Chongqing');
	}

	// basic
	$vars['@project@'] = is_file($project) ? realpath($project) : $project;
	$vars['@charset@'] = $xs->getDefaultCharset();
	if ($vars['@charset@'] !== 'GB2312' && $vars['@charset@'] !== 'GBK') {
		$vars['@charset@'] = 'UTF-8';
	}
	$vars['@xs_lib_root@'] = XS_LIB_ROOT;
	$vars['@xs_lib_file@'] = realpath($lib_file);
	$vars['@date_time@'] = date('Y-m-d H:i:s');
	$vars['@project_name@'] = ucfirst($xs->name);
	$vars['@package_name@'] = XS_PACKAGE_NAME;
	$vars['@package_version@'] = XS_PACKAGE_VERSION;

	// fields
	$vars['@set_filter@'] = '';
	$vars['@set_sort@'] = '';
	$vars['@field_id@'] = $xs->getFieldId()->name;
	if (($field = $xs->getFieldTitle()) !== false) {
		$vars['@field_title@'] = $field->name;
	}
	if (($field = $xs->getFieldBody()) !== false) {
		$vars['@field_body@'] = $field->name;
	}
	$vars['@field_info@'] = '';
	foreach ($xs->getAllFields() as $field) /* @var $field XSFieldMeta */ {
		if ($field->hasIndexSelf() && $field->type != XSFieldMeta::TYPE_BODY && !$field->isBoolIndex()) {
			$vars['@set_filter@'] .= "\t\t\t<label class=\"radio inline\"><input type=\"radio\" name=\"f\" value=\"{$field->name}\" <?php echo \$f_{$field->name}; ?> />" . ucfirst($field->name) . "</label>\n";
		}
		if ($field->isNumeric()) {
			$vars['@set_sort@'] .= "\t\t\t\t\t<option value=\"" . $field->name . "_DESC\" <?php echo \$s_{$field->name}_DESC; ?>>" . ucfirst($field->name) . "从大到小</option>\n";
			$vars['@set_sort@'] .= "\t\t\t\t\t<option value=\"" . $field->name . "_ASC\" <?php echo \$s_{$field->name}_ASC; ?>>" . ucfirst($field->name) . "从小到大</option>\n";
		}
		if ($field->isSpeical()) {
			continue;
		}
		if ($field->type == XSFieldMeta::TYPE_STRING) {
			if (!isset($vars['@field_title@'])) {
				$vars['@field_title@'] = $field->name;
				continue;
			}
			if (!isset($vars['@field_body@'])) {
				$vars['@field_body@'] = $field->name;
				continue;
			}
		}
		$vars['@field_info@'] .= "\t\t\t\t<span><strong>" . ucfirst($field->name) . ":</strong><?php echo htmlspecialchars(\$doc->" . $field->name . "); ?></span>\n";
	}

	$vars['@set_filter@'] = trim($vars['@set_filter@']);
	$vars['@set_sort@'] = trim($vars['@set_sort@']);
	$vars['@field_info@'] = trim($vars['@field_info@']);
	if (!isset($vars['@field_title@'])) {
		$vars['@field_title@'] = 'title';
	}
	if (!isset($vars['@field_body@'])) {
		$vars['@field_body@'] = 'body';
	}

	// output dir
	echo "检测并创建输出目录 ...\n";
	$output .= '/' . $xs->name;
	if (!is_dir($output) && !mkdir($output)) {
		throw new XSException('Failed to create output directory: ' . $output);
	}

	// loop to write-in files
	$input = dirname(__FILE__) . '/skel';
	$dir = dir($input);
	while (($entry = $dir->read()) !== false) {
		if ($entry === '.' || $entry === '..') {
			continue;
		}
		if (substr($entry, -3) === '.in') {
			echo "正在生成 " . substr($entry, 0, -3) . " ...\n";
			$file = $output . '/' . substr($entry, 0, -3);
			if (file_exists($file)) {
				copy($file, $file . '.bak');
			}
			$content = file_get_contents($input . '/' . $entry);
			$content = strtr($content, $vars);
			if ($vars['@charset@'] !== 'UTF-8') {
				$content = XS::convert($content, $vars['@charset@'], 'UTF-8');
			}
			file_put_contents($file, $content);
		} else {
			echo "正在复制 " . $entry . " ...\n";
			$file = $output . '/' . $entry;
			if (is_dir($input . '/' . $entry)) {
				XSUtil::copyDir($input . '/' . $entry, $file);
			} else {
				if (file_exists($file)) {
					copy($file, $file . '.bak');
				}
				copy($input . '/' . $entry, $file);
			}
		}
	}
	$dir->close();
	echo "完成，请将 `$output` 目录转移到 web 可达目录，然后访问 search.php 即可。\n";
} catch (XSException $e) {
	// Exception
	$start = dirname(dirname(__FILE__)) . '/';
	$relative = substr(XSException::getRelPath($start), 0, -1);
	$traceString = $e->getTraceAsString();
	$traceString = str_replace(dirname(__FILE__) . '/', '', $traceString);
	$traceString = str_replace($start, $relative, $traceString);
	echo $e . "\n" . $traceString . "\n";
}

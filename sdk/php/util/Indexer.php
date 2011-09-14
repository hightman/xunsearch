#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 索引管理工具
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
require_once dirname(__FILE__) . '/../lib/XS.php';
require_once dirname(__FILE__) . '/XSUtil.class.php';
require_once dirname(__FILE__) . '/XSDataSource.class.php';

// check arguments
//ini_set('memory_limit', '1024M');
XSUtil::parseOpt(array('p', 'c', 'd', 'project', 'charset', 'db', 'source', 'file', 'sql', 'csv-delimiter'));
$project = XSUtil::getOpt('p', 'project', true);

// magick output charset
$charset = XSUtil::getOpt('c', 'charset');
XSUtil::setCharset($charset);

// long options
$params = array('source', 'file', 'sql', 'rebuild', 'clean', 'flush', 'flush-log', 'info', 'csv-delimiter');
foreach ($params as $_)
{
	$k = strtr($_, '-', '_');
	$$k = XSUtil::getOpt(null, $_);
}

// file & database
$file = XSUtil::getOpt(null, 'file', true);
$db = XSUtil::getOpt('d', 'db');

// help message
if (XSUtil::getOpt('h', 'help') !== null || !is_string($project)
	|| (!$flush && !$flush_log && !$info && !$clean && !$source))
{
	$version = PACKAGE_NAME . '/' . PACKAGE_VERSION;
	echo <<<EOF
Indexer - 索引批量管理、导入工具 ($version)

用法
    {$_SERVER['argv'][0]} [options] [-p|--project] <project> [--file] <file>
	
选项说明
    --project=<name|ini>
    -p <project> 用于指定要搜索的项目名称或项目配置文件的路径，
                 如果指定的是名称，则使用 ../app/<name>.ini 作为配置文件
    --charset=<gbk|utf-8>
    -c <charset> 指定您当前在用以及数据源的字符集，以便系统进行智能转换
    --db=<name>
    -d <db>      指定项目中的数据库名称，默认是名为 db 的库

    --source=mysql://[user[:passwd]@]host/dbname[/table]
                 指定数据源为 mysql
    --source=sqlite:[//]<dbpath>|sqlite3:[//]<dbpath>
                 指定数据源为 sqlite 或 sqlite3
    --source=json指定数据源为 json 格式，每行一条记录
    --source=csv 指定数据源为 csv 格式，逗号分隔字段，每行一条记录，可在首行指定字段名
    --csv-delimiter[=,] 指定 csv 数据源的字段分割符，默认为逗号，支持 \\t\\r\\n..\\xNN
	             使用 \\ 开头及其它与 shell 有岐议的分割符时请使用引号包围。
    --file <file>当数据源为 json 或 csv 格式时指定数据源文件，默认读取标准输入
    --sql <sql>  当数据源为 sql 类型时指定 sql 搜索语句，默认情况下，
                 如果在 --source 包含 table 则载入该表数据

    --rebuild    使用平滑重建方式导入数据，必须与 --source 配合使用
    --clean      清空库内当前的索引数据
    --flush      强制提交刷新索引服务端的缓冲索引，与 --source 分开用
    --flush-log	 强制提交刷新搜索日志，与 --source 分开用
    --info       查看当前索引库在服务端的信息（含数据缓冲、运行进程等）
    -h|--help    显示帮助信息

EOF;
	exit(0);
}

// create xs project
$ini = file_exists($project) ? $project : dirname(__FILE__) . '/../app/' . $project . '.ini';
if (!file_exists($ini))
{
	echo "错误：无效的项目名称 ($project)，不存在相应的配置文件。\n";
	exit(-1);
}

// csv delimiter saved in super global variable: _SERVER
if (is_string($csv_delimiter) && $source == 'csv')
{
	if (substr($csv_delimiter, 0, 1) !== '\\')
		$csv_delimiter = substr($csv_delimiter, 0, 1);
	else
	{
		$char = substr($csv_delimiter, 1, 1);
		switch ($char)
		{
			case '\\':
				$csv_delimiter = '\\';
				break;
			case 't':
				$csv_delimiter = "\t";
				break;
			case 'x':
				$csv_delimiter = chr(hexdec(substr($csv_delimiter, 2)));
				break;
			default:
				$csv_delimiter = ',';
				break;
		}
	}
	$_SERVER['XS_CSV_DELIMITER'] = $csv_delimiter;
	printf("注意：CSV 字段分割符被修改为 `%c` (ASCII: 0x%02x)\n", ord($csv_delimiter), ord($csv_delimiter));
}

// execute the indexer
try
{
	// create xs object
	$xs = new XS($ini);
	$index = $xs->index;
	if ($db !== null)
		$index->setDb($db);

	// special actions
	if ($info !== null)
	{
		$res = $index->execCommand(CMD_INDEX_GET_DB);
		$res = json_decode($res->buf);
		echo "数据库名：" . $res->name . "\n";
		echo "队列数据：" . $res->count . "条\n";
		echo "导入进程：" . ($res->pid > 0 ? '#' . $res->pid : '无') . "\n";
	}
	else if ($flush_log !== null)
	{
		echo "刷新搜索日志 ... \n";
		if (($res = $index->flushLogging()) === false)
			echo "失败\n";
		else
			echo "成功，注意：后台更新需要一些时间，并不是真正立即完成。\n";
	}
	else if ($flush !== null)
	{
		echo "刷新索引缓冲 ... \n";
		if (($res = $index->flushIndex()) === false)
			echo "失败\n";
		else
			echo "成功，注意：后台更新需要一些时间，并不是真正立即完成。\n";
	}
	else
	{
		// clean
		if ($clean !== null)
		{
			echo "清空现有索引数据 ...\n";
			$index->clean();
		}

		// begin rebuild
		if ($rebuild !== null)
		{
			echo "开始重建索引 ...\n";
			$index->beginRebuild();
		}

		// import data from source
		if (!empty($source))
		{
			echo "初始化数据源 ... $source \n";
			$total = $total_ok = $total_failed = 0;
			$src = XSDataSource::instance($source, strpos($source, ':') ? $sql : $file);
			$dcs = $src->getCharset();
			$doc = new XSDocument($dcs === false ? $charset : $dcs);

			echo "开始批量导入数据 (" . (empty($file) ? "请直接输入数据" : $file) . ") ...\n";
			$index->setTimeout(0);
			$index->openBuffer();
			while ($data = $src->getData())
			{
				if ($source == 'csv')
				{
					$data = csv_transform($data);
					if (is_null($data))
						continue;
				}

				$doc->setFields($data);
				try
				{
					$total++;
					$index->update($doc);
					$total_ok++;
				}
				catch (XSException $e)
				{
					echo "警告：添加第 $total 条数据失败 - " . $e->getMessage() . "\n";
					echo $e->getTraceAsString();
					$total_failed++;
				}
				$doc->setFields(null);
				if (($total % 10000) == 0)
					echo "报告：累计已处理数据 $total 条 ...\n";
			}
			$index->closeBuffer();
			echo "完成索引导入：成功 $total_ok 条，失败 $total_failed 条\n";
		}

		// end rebuild
		if ($rebuild !== null)
		{
			echo "完成重建索引 ...\n";
			$index->endRebuild();
		}
		else
		{
			echo "刷新索引提交 ...\n";
			$index->flushIndex();
		}
	}
}
catch (XSException $e)
{
	// Exception
	$start = getcwd() . '/';
	$relative = XSException::getRelPath($start);
	$traceString = $e->getTraceAsString();
	$traceString = str_replace($start, $relative, $traceString);
	echo $e . "\n" . $traceString . "\n";
}

// translate csv data
function csv_transform($data)
{
	static $fields = null;
	global $xs; /* @var $xs XS */

	// init field set
	if (is_null($fields))
	{
		// load default fields
		$fields = array_keys($xs->scheme->getAllFields());

		// check data is fieldset or not
		$is_header = true;
		foreach ($data as $tmp)
		{
			if (!in_array($tmp, $fields))
			{
				$is_header = false;
				break;
			}
		}
		if ($is_header)
		{
			$fields = $data;
			echo "注意：CSV 数据字段被指定为：" . implode(',', $data) . "\n";
			return null;
		}
	}

	// transform
	$ret = array();
	foreach ($fields as $key)
	{
		$index = count($ret);
		if (!isset($data[$index]))
			break;
		$ret[$key] = $data[$index];
	}
	return $ret;
}

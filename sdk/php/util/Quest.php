#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 搜索测试工具
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
require_once dirname(__FILE__) . '/../lib/XS.php';
require_once dirname(__FILE__) . '/XSUtil.class.php';

// check arguments
XSUtil::parseOpt(array('p', 'q', 'c', 'd', 'project', 'query', 'db', 'limit', 'charset'));
$project = XSUtil::getOpt('p', 'project', true);
$query = XSUtil::getOpt('q', 'query', true);
$hot = XSUtil::getOpt(null, 'hot');

// magick output charset
$charset = XSUtil::getOpt('c', 'charset');
XSUtil::setCharset($charset);
$query = XSUtil::convertIn($query);

if (XSUtil::getOpt('h', 'help') !== null || !is_string($project)
	|| (!$hot && !is_string($query)))
{
	$version = PACKAGE_NAME . '/' . PACKAGE_VERSION;
	echo <<<EOF
Quest - 搜索查询和测试工具 ($version)

用法
    {$_SERVER['argv'][0]} [options] [-p|--project] <project> [[-q|--query] <query>]
	
选项说明
    --project=<name|ini>
    -p <project> 用于指定要搜索的项目名称或项目配置文件的路径，
                 如果指定的是名称，则使用 ../app/<name>.ini 作为配置文件
    --query=<query>	
    -q <query>   指定要搜索的查询语句，如果语句中包含空格请用使用双引号包围起来
    --fuzzy      将搜索默认设为模糊搜索	
    --charset=<gbk|utf-8>
    -c <charset> 指定您当前在用的字符集，以便系统进行智能转换（默认：UTF-8）
    --db=<name[,name2 ...]>
    -d <db[,db2 ...]> 指定项目中的数据库名称，默认是名为 db 的库，多个库之间用逗号分隔
    --hot[=total|last|cur] 
                 用于显示指定项目的热门搜索词，此时 <query> 参数无意义，可省略
                 其值含义分别表示总搜索量、上周搜索量、本周搜索量，默认为总搜索量。
    --suggest    根据当前搜索词展开常用搜索词建议，如查询“中”，即显示“中”开头的词
    --correct    根据当前搜索词进行同音、拼写纠错，输出更合适的关键词
    --related    根据当前搜索词查找相关搜索词
    --limit=<num>用于设置 suggest|hot|related 的返回数量，两者默认值均为 10 个
                 对于普通搜索模式，还支持用 --limit=offset,num 的格式
    -h|--help    显示帮助信息

    若未指定 -p 或 -q 则会依次把附加的参数当作 <project> 和 <query> 处理，例：
    {$_SERVER['argv'][0]} <project> <query>
    {$_SERVER['argv'][0]} --hot <project>

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

// execute the search
try
{
	// params
	$params = array('hot', 'suggest', 'correct', 'related', 'output', 'limit');
	foreach ($params as $_)
	{
		$$_ = XSUtil::getOpt(null, $_);
	}
	$limit1 = $limit === null ? 10 : intval($limit);
	$db = XSUtil::getOpt('d', 'db');

	// create xs object
	$xs = new XS($ini);
	$search = $xs->search;
	$search->setCharset($charset === null ? 'UTF-8' : $charset);
	if ($db !== null)
	{
		$dbs = explode(',', $db);
		$search->setDb(trim($dbs[0]));
		for ($i = 1; $i < count($dbs); $i++)
		{
			$search->addDb(trim($dbs[$i]));
		}
	}

	if ($hot !== null)
	{
		$type = $hot === 'cur' ? 'currnum' : ($hot === 'last' ? 'lastnum' : 'total');
		$result = $search->getHotQuery($limit1, $type);
		if (count($result) === 0)
			echo "暂无相关热门搜索记录。\n";
		else
		{
			$i = 1;
			printf("序  %s %s\n%s\n", XSUtil::fixWidth('搜索关键词(' . $type . ')', 40), XSUtil::fixWidth('次数', 10), XSUtil::fixWidth('', 56, '-'));
			foreach ($result as $word => $freq)
			{
				printf("%2d. %s %d\n", $i, XSUtil::fixWidth($word, 40), $freq);
				$i++;
			}
		}
	}
	else if ($correct !== null)
	{
		$result = $search->getCorrectedQuery($query);
		if (count($result) === 0)
		{
			echo "目前对\033[7m" . $query . "\033[m还没有更好的修正方案。\n";
		}
		else
		{
			echo "您可以试试找：\033[4m" . implode("\033[m \033[4m", $result) . "\033[m\n";
		}
	}
	else if ($suggest !== null)
	{
		$result = $search->getExpandedQuery($query, $limit1);
		if (count($result) === 0)
		{
			echo "目前对\033[7m" . $query . "\033[m还没有任何搜索建议。\n";
		}
		else
		{
			echo "展开\033[7m" . $query . "\033[m得到以下搜索建议：\n";
			for ($i = 0; $i < count($result); $i++)
			{
				printf("%d. %s\n", $i + 1, $result[$i]);
			}
		}
	}
	else if ($related !== null)
	{
		$result = $search->getRelatedQuery($query, $limit1);
		if (count($result) === 0)
		{
			echo "目前还没有与\033[7m" . $query . "\033[m相关的搜索词。\n";
		}
		else
		{
			echo "与\033[7m" . $query . "\033[m相关的搜索词：\n";
			for ($i = 0; $i < count($result); $i++)
			{
				printf("%d. %s\n", $i + 1, $result[$i]);
			}
		}
	}
	else
	{
		// fuzzy search
		if (XSUtil::getOpt(null, 'fuzzy') !== null)
			$search->setFuzzy();
		
		if (($pos = strpos($limit, ',')) === false)
			$offset = 0;
		else
		{
			$limit1 = intval(substr($limit, $pos + 1));
			$offset = intval($limit);
		}

		// special fields
		$fid = $xs->getFieldId();
		$ftitle = $xs->getFieldTitle();
		$fbody = $xs->getFieldBody();
		if ($fbody)
			$xs->getFieldBody()->cutlen = 100;

		// preform search
		$begin = microtime(true);
		$result = $search->setQuery($query)->setLimit($limit1, $offset)->search();
		$cost = microtime(true) - $begin;
		$matched = $search->getLastCount();
		$total = $search->getDbTotal();
		$correct = $search->getCorrectedQuery();
		$related = $search->getRelatedQuery();

		// info
		printf("在 %s 条数据中，大约有 %d 条包含 \033[7m%s\033[m ，第 %d-%d 条，用时：%.4f 秒。\n", number_format($total), $matched, $query, min($matched, $offset + 1), min($matched, $limit1 + $offset), $cost);
		// correct
		if (count($correct) > 0)
			echo "您是不是想找：\033[4m" . implode("\033[m \033[4m", $correct) . "\033[m\n";
		// show result
		foreach ($result as $doc) /* @var $doc XSDocument */
		{
			// body & title
			$body = $title = '';
			if ($ftitle !== false)
			{
				$title = $search->highlight($doc->f($ftitle));
				$title = preg_replace('#<em>(.+?)</em>#', "\033[7m\\1\033[m", $title) . ' ';
			}
			if ($fbody !== false)
			{
				$body = $search->highlight($doc->f($fbody));
				$body = preg_replace('#<em>(.+?)</em>#', "\033[7m\\1\033[m", $body) . "\n";
			}

			// main fields
			printf("\n%d. \033[4m%s#%s# [%d%%]\033[m\n", $doc->rank(), $title, $doc->f($fid), $doc->percent());
			echo $body;

			// other fields
			$line = '';
			foreach ($xs->getAllFields() as $field) /* @var $field XSFieldMeta */
			{
				if ($field->isSpeical())
					continue;
				$tmp = ucfirst($field->name) . ':' . $doc->f($field);
				if ((strlen($tmp) + strlen($line)) > 80)
				{
					if (strlen($line) > 0)
					{
						echo $line . "\n";
						$line = '';
					}
					echo $tmp . "\n";
				}
				else
				{
					$line .= $tmp . ' ';
				}
			}
			if (strlen($line) > 0)
				echo $line . "\n";
		}
		// related
		if (count($related) > 0)
			echo "\n相关搜索：\033[4m" . implode("\033[m \033[4m", $related) . "\033[m\n";
		echo "\n";
	}
}
catch (XSException $e)
{
	// Exception
	$start = dirname(dirname(__FILE__));
	$relative = XSException::getRelPath($start);
	$traceString = $e->getTraceAsString();
	$traceString = str_replace(dirname(__FILE__) . '/', '', $traceString);
	$traceString = str_replace($start . ($relative === '' ? '/' : ''), $relative, $traceString);
	echo $e . "\n" . $traceString . "\n";
}

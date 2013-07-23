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
XSUtil::parseOpt(array('p', 'q', 'c', 'd', 's',
	'project', 'query', 'db', 'limit', 'charset',
	'sort', 'add-weight', 'scws-multi', 'cut-off',
));
$project = XSUtil::getOpt('p', 'project', true);
$query = XSUtil::getOpt('q', 'query', true);
$hot = XSUtil::getOpt(null, 'hot');
$synonyms = XSUtil::getOpt(null, 'list-synonyms');
$terms = XSUtil::getOpt(null, 'terms');
$weights = XSUtil::getOpt(null, 'add-weight');
$info = XSUtil::getOpt(null, 'info');
$scws_multi = XSUtil::getOpt(null, 'scws-multi');
$cut_off = XSUtil::getOpt(null, 'cut-off');

// magick output charset
$charset = XSUtil::getOpt('c', 'charset');
XSUtil::setCharset($charset);
$query = XSUtil::convertIn($query);

// sort scheme
$sort = XSUtil::getOpt('s', 'sort');

if (XSUtil::getOpt('h', 'help') !== null || !is_string($project)
		|| (!$info && !$hot && !$synonyms && !is_string($query))) {
	$version = PACKAGE_NAME . '/' . PACKAGE_VERSION;
	echo <<<EOF
Quest - 搜索查询和测试工具 ($version)

用法
    {$_SERVER['argv'][0]} [options] [-p|--project] <project> [[-q|--query] <query>]
	
选项说明
    --project=<name|ini>
    -p <project> 用于指定要搜索的项目名称或项目配置文件的路径，
                 如果指定的是名称，则使用 ../app/<name>.ini 作为配置文件
    --charset=<gbk|utf-8>
    -c <charset> 指定您当前在用的字符集，以便系统进行智能转换（默认：UTF-8）
    --db=<name[,name2 ...]>
    -d <db[,db2 ...]> 指定项目中的数据库名称，默认是名为 db 的库，多个库之间用逗号分隔
    --query=<query>
    -q <query>   指定要搜索的查询语句，如果语句中包含空格请用使用双引号包围起来
                 在搜索语句中可采用 'field:\$from..\$to' 做区间过滤
    --sort=<field1[,field2[,...]]
    -s <field1[,field2[,...]] 指定排序字段，在字段前加上 ~ 符号表示逆序
    --fuzzy      将搜索默认设为模糊搜索
    --synonym[=scale]
                 开启自动同义词搜索功能，可选择设置同义词权重调整（0.01~2.55）
    --scws-multi=<level>
                 查看或设置搜索语句的 scws 复合分词等级（值：0-15，默认为 3）
    --add-weight=<[field1:]word1[:weight1][,[field2:]word2[:weight2]]>
                 添加搜索权重词汇，词与次数之间用半角冒号分隔
    --hot[=total|last|cur]
                 用于显示指定项目的热门搜索词，此时 <query> 参数无意义，可省略
                 其值含义分别表示总搜索量、上周搜索量、本周搜索量，默认为总搜索量。
    --suggest    根据当前搜索词展开常用搜索词建议，如查询“中”，即显示“中”开头的词
    --correct    根据当前搜索词进行同音、拼写纠错，输出更合适的关键词
    --related    根据当前搜索词查找相关搜索词
    --list-synonyms[=stemmed]
                 列出库内的全部同义词，每行显示一个，可以搭配 --limit 使用，默认显示前 100 个
                 如果设置了 stemmed 值则连同词根同义词也列出
    --limit=<num>用于设置 suggest|hot|related 的返回数量，两者默认值均为 10 个
                 对于普通搜索和列出同义词时，还支持用 --limit=offset,num 的格式
    --show-query 用于在搜索结果显示内部的 Xapian 结构的 query 语句用于调试
    --cut-off=<percent[,weight>
                 设置搜索结果剔除的匹配百分比及权限（百分比：0-100，权重：0.1-25.5）
    --terms      列出搜索词被切分后的词（不含排除及权重词）
    --info       显示当前连接服务端的信息及线程（仅绘制当前 worker 进程）
    -h|--help    显示帮助信息

    若未指定 -p 或 -q 则会依次把附加的参数当作 <project> 和 <query> 处理，例：
    {$_SERVER['argv'][0]} <project> <query>
    {$_SERVER['argv'][0]} --hot <project>

EOF;
	exit(0);
}

// create xs project
$ini = file_exists($project) ? $project : dirname(__FILE__) . '/../app/' . $project . '.ini';
if (!file_exists($ini)) {
	echo "错误：无效的项目名称 ($project)，不存在相应的配置文件。\n";
	exit(-1);
}

// execute the search
try {
	// params
	$params = array('hot', 'suggest', 'correct', 'related', 'output', 'limit');
	foreach ($params as $_) {
		$$_ = XSUtil::getOpt(null, $_);
	}
	$limit1 = $limit === null ? 10 : intval($limit);
	$db = XSUtil::getOpt('d', 'db');

	// create xs object
	$xs = new XS($ini);
	$search = $xs->search;
	$search->setCharset('UTF-8');
	if ($db !== null) {
		$dbs = explode(',', $db);
		$search->setDb(trim($dbs[0]));
		for ($i = 1; $i < count($dbs); $i++) {
			$search->addDb(trim($dbs[$i]));
		}
	}
	if ($scws_multi !== null) {
		$search->setScwsMulti($scws_multi);
	}

	if ($hot !== null) {
		$type = $hot === 'cur' ? 'currnum' : ($hot === 'last' ? 'lastnum' : 'total');
		$result = $search->getHotQuery($limit1, $type);
		if (count($result) === 0) {
			echo "暂无相关热门搜索记录。\n";
		} else {
			$i = 1;
			printf("序  %s %s\n%s\n", XSUtil::fixWidth('搜索关键词(' . $type . ')', 40),
					XSUtil::fixWidth('次数', 10), XSUtil::fixWidth('', 56, '-'));
			foreach ($result as $word => $freq) {
				printf("%2d. %s %d\n", $i, XSUtil::fixWidth($word, 40), $freq);
				$i++;
			}
		}
	} elseif ($info !== null) {
		// server info
		echo "---------- SERVER INFO BEGIN ----------\n";
		$res = $search->execCommand(CMD_DEBUG);
		echo $res->buf;
		echo "\n---------- SERVER INFO END ----------\n";
		// thread pool
		$res = $search->execCommand(CMD_SEARCH_DRAW_TPOOL);
		echo $res->buf;
	} elseif ($synonyms !== null) {
		if ($limit === null) {
			$offset = $limit1 = 0;
		} elseif (($pos = strpos($limit, ',')) === false) {
			$offset = 0;
		} else {
			$limit1 = intval(substr($limit, $pos + 1));
			$offset = intval($limit);
		}

		$synonyms = $search->getAllSynonyms($limit1, $offset, $synonyms === 'stemmed');
		if (count($synonyms) == 0) {
			echo "暂无相关的同义词记录";
			if ($offset != 0) {
				echo "，反正总数不超过 $offset 个";
			}
			echo "。\n";
		} else {
			$i = $offset + 1;
			printf("   %s %s\n%s\n", XSUtil::fixWidth('原词', 32), '同义词', XSUtil::fixWidth('', 56, '-'));
			foreach ($synonyms as $raw => $list) {
				printf("%4d. %s %s\n", $i++, XSUtil::fixWidth($raw, 29), implode(", ", $list));
			}
		}
	} elseif ($terms !== null) {
		$result = $search->terms($query);
		echo "列出\033[7m" . $query . "\033[m的内部切分结果：\n";
		print_r($result);
	} elseif ($correct !== null) {
		$result = $search->getCorrectedQuery($query);
		if (count($result) === 0) {
			echo "目前对\033[7m" . $query . "\033[m还没有更好的修正方案。\n";
		} else {
			echo "您可以试试找：\033[4m" . implode("\033[m \033[4m", $result) . "\033[m\n";
		}
	} elseif ($suggest !== null) {
		$result = $search->getExpandedQuery($query, $limit1);
		if (count($result) === 0) {
			echo "目前对\033[7m" . $query . "\033[m还没有任何搜索建议。\n";
		} else {
			echo "展开\033[7m" . $query . "\033[m得到以下搜索建议：\n";
			for ($i = 0; $i < count($result); $i++) {
				printf("%d. %s\n", $i + 1, $result[$i]);
			}
		}
	} elseif ($related !== null) {
		$result = $search->getRelatedQuery($query, $limit1);
		if (count($result) === 0) {
			echo "目前还没有与\033[7m" . $query . "\033[m相关的搜索词。\n";
		} else {
			echo "与\033[7m" . $query . "\033[m相关的搜索词：\n";
			for ($i = 0; $i < count($result); $i++) {
				printf("%d. %s\n", $i + 1, $result[$i]);
			}
		}
	} else {
		// fuzzy search
		if (XSUtil::getOpt(null, 'fuzzy') !== null) {
			$search->setFuzzy();
		}
		$syn = XSUtil::getOpt(null, 'synonym');
		if ($syn !== null) {
			$search->setAutoSynonyms();
			if ($syn !== true) {
				$search->setSynonymScale(floatval($syn));
			}
		}

		if (($pos = strpos($limit, ',')) === false) {
			$offset = 0;
		} else {
			$limit1 = intval(substr($limit, $pos + 1));
			$offset = intval($limit);
		}

		// sort
		if ($sort !== null) {
			$fields = array();
			$tmps = explode(',', $sort);
			foreach ($tmps as $tmp) {
				$tmp = trim($tmp);
				if ($tmp === '') {
					continue;
				}
				if (substr($tmp, 0, 1) === '~') {
					$fields[substr($tmp, 1)] = false;
				} else {
					$fields[$tmp] = true;
				}
			}
			$search->setMultiSort($fields);
		}

		// special fields
		$fid = $xs->getFieldId();
		$ftitle = $xs->getFieldTitle();
		$fbody = $xs->getFieldBody();
		if ($fbody) {
			$xs->getFieldBody()->cutlen = 100;
		}

		// add range
		$ranges = array();
		if (strpos($query, '..') !== false) {
			$regex = '/(\S+?):(\S*?)\.\.(\S*)/';
			if (preg_match_all($regex, $query, $matches) > 0) {
				for ($i = 0; $i < count($matches[0]); $i++) {
					$ranges[] = array($matches[1][$i],
						$matches[2][$i] === '' ? null : $matches[2][$i],
						$matches[3][$i] === '' ? null : $matches[3][$i]);
					$query = str_replace($matches[0][$i], '', $query);
				}
			}
		}

		// set query
		$search->setQuery($query);
		foreach ($ranges as $range) {
			$search->addRange($range[0], $range[1], $range[2]);
		}

		// add weights
		if ($weights !== null) {
			foreach (explode(',', $weights) as $tmp) {
				$tmp = explode(':', trim($tmp));
				if (count($tmp) === 1) {
					$search->addWeight(null, $tmp[0]);
				} elseif (count($tmp) === 2) {
					if (is_numeric($tmp[1])) {
						$search->addWeight(null, $tmp[0], floatval($tmp[1]));
					} else {
						$search->addWeight($tmp[0], $tmp[1]);
					}
				} else {
					$search->addWeight($tmp[0], $tmp[1], floatval($tmp[2]));
				}
			}
		}

		// cut off
		if ($cut_off !== null) {
			if (($pos = strpos($cut_off, ','))) {
				$search->setCutOff(substr($cut_off, 0, $pos), substr($cut_off, $pos + 1));
			} elseif (strpos($cut_off, '.') !== false) {
				$search->setCutOff(0, $cut_off);
			} else {
				$search->setCutOff($cut_off);
			}
		}

		// preform search
		$begin = microtime(true);
		$result = $search->setLimit($limit1, $offset)->search();
		$cost = microtime(true) - $begin;
		$matched = $search->getLastCount();
		$total = $search->getDbTotal();

		// show query?
		if (XSUtil::getOpt(null, 'show-query') !== null) {
			echo str_repeat("-", 20) . "\n";
			echo "解析后的 QUERY 语句：" . $search->getQuery() . "\n";
			echo str_repeat("-", 20) . "\n";
		}

		// related & corrected
		$correct = $search->getCorrectedQuery();
		$related = $search->getRelatedQuery();

		// info
		printf("在 %s 条数据中，大约有 %d 条包含 \033[7m%s\033[m ，第 %d-%d 条，用时：%.4f 秒。\n", number_format($total),
				$matched, $query, min($matched, $offset + 1), min($matched, $limit1 + $offset), $cost);
		// correct
		if (count($correct) > 0) {
			echo "您是不是想找：\033[4m" . implode("\033[m \033[4m", $correct) . "\033[m\n";
		}
		// show result
		foreach ($result as $doc) /* @var $doc XSDocument */ {
			// body & title
			$body = $title = '';
			if ($ftitle !== false) {
				$title = cliHighlight($doc->f($ftitle));
			}
			if ($fbody !== false) {
				$body = cliHighlight($doc->f($fbody)) . "\n";
			}

			// main fields
			printf("\n%d. \033[4m%s#%s# [%d%%,%.2f]\033[m\n", $doc->rank(), $title, $doc->f($fid),
					$doc->percent(), $doc->weight());
			echo $body;

			// other fields
			$line = '';
			foreach ($xs->getAllFields() as $field) /* @var $field XSFieldMeta */ {
				if ($field->isSpeical()) {
					continue;
				}
				$tmp = ucfirst($field->name) . ':' . cliHighlight($doc->f($field));
				if ((strlen($tmp) + strlen($line)) > 80) {
					if (strlen($line) > 0) {
						echo $line . "\n";
						$line = '';
					}
					echo $tmp . "\n";
				} else {
					$line .= $tmp . ' ';
				}
			}
			if (strlen($line) > 0) {
				echo $line . "\n";
			}
		}
		// related
		if (count($related) > 0) {
			echo "\n相关搜索：\033[4m" . implode("\033[m \033[4m", $related) . "\033[m\n";
		}
		echo "\n";
	}
} catch (XSException $e) {
	// Exception
	$start = dirname(dirname(__FILE__));
	$relative = XSException::getRelPath($start);
	$traceString = $e->getTraceAsString();
	$traceString = str_replace(dirname(__FILE__) . '/', '', $traceString);
	$traceString = str_replace($start . ($relative === '' ? '/' : ''), $relative, $traceString);
	echo $e . "\n" . $traceString . "\n";
}

// local highlight function
function cliHighlight($str)
{
	global $search;
	$str = $search->highlight($str);
	$str = preg_replace('#<em>(.+?)</em>#', "\033[7m\\1\033[m", $str) . ' ';
	$str = strtr($str, array('<em>' => '', '</em>' => ''));
	return $str;
}

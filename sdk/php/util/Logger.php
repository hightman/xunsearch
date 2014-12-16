#!/usr/bin/env php
<?php
/**
 * Xunsearch PHP-SDK 搜索日志管理工具
 * 
 * @author hpxl
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
XSUtil::parseOpt(array('p', 'c', 'project', 'charset', 'hot', 'del', 'put', 'query', 'import', 'limit'));
$project = XSUtil::getOpt('p', 'project', true);
$query = XSUtil::getOpt(null, 'query', true);

// magick output charset
$charset = XSUtil::getOpt('c', 'charset');
XSUtil::setCharset($charset);

// long options
$params = array('import', 'del', 'put', 'flush', 'hot', 'clean', 'limit');
foreach ($params as $_) {
	$k = strtr($_, '-', '_');
	$$k = XSUtil::getOpt(null, $_);
}

if ($query === null && $put === null && $del === null
		&& $flush === null && $import === null
		&& $hot === null && $clean === null) {
	$hot = 'total';
}

// help message
if (XSUtil::getOpt('h', 'help') !== null || !is_string($project)
		|| ($import !== null && !is_string($import)) || ($query !== null && !is_string($query))
		|| ($put !== null && !is_string($put)) || ($del !== null && !is_string($del))) {
	$version = XS_PACKAGE_NAME . '/' . XS_PACKAGE_VERSION;
	echo <<<EOF
Logger - 搜索日志管理工具 ($version)

用法
    {$_SERVER['argv'][0]} [options] [-p|--project] [project] [[--query] <word>]

选项说明
    --project=<name|ini>
    -p <project>        用于指定要搜索的项目名称或项目配置文件的路径，
                        如果指定的是名称，则使用 ../app/<name>.ini 作为配置文件
    --charset=<gbk|utf-8>
    -c <charset>        指定您当前在用以及导入数据源的字符集，以便系统进行
                        智能转换（默认：UTF-8）

    --import=<file>     导入搜索日志文件, 一行一个词, 每行的数据中
                        可以用\\t(Tab键)分开指定次数，没有次数默认为1

    --put=<word1[:wdf1][,word2[:wdf2]]>
                        添加搜索日志词汇，词与次数之间用半角冒号分隔，
                        默认为 1 次。多个词之间用半角的逗号分隔，
                        词之间如果包含空格，请将参数用引号包围起来。

    --del=<word1[,word2[,word3]]>
                        删除搜索日志中的词汇记录，若不存在则会给出提示，
                        多个词之间用半角的逗号分隔，如果包含空格，请将参数用引号包围起来。

    --flush             刷新搜索日志变动，如急着看效果，请调用该选项强制刷新所有提交。
    --clean             清空全部搜索日志内容

    --limit=<num>       用于控制 query 和 hot 选项的返回关键词个数

    --query=<word>      以 word 为关键词查找相关搜索词，用 limit 选项设置个数，默认 6 个
    --hot=<total|last|cur>
                        列出热门搜索词汇，可结合 limit 指定个数，默认 10 个
                        参数依次表示总次数、上期次数、本期次数

    -h|--help           显示帮助信息

EOF;
	exit(0);
}

// create xs project
$ini = XSUtil::toProjectIni($project);
if (!file_exists($ini)) {
	echo "错误：无效的项目名称 ($project)，不存在相应的配置文件。\n";
	exit(-1);
}

try {
	$db = XSSearch::LOG_DB;
	$log_ready = false;
	$xs = new XS($ini);
	$xs->setScheme(XSFieldScheme::logger());

	$search = $xs->search;
	try {
		// NOTE: use setQuery to call preQueryString for preparing fieldset
		$search->setDb($db)->setQuery('dummy');
		$search->setTimeout(0); // sometimes user may import lots of terms
		$log_ready = true;
	} catch (Exception $e) {
		
	}

	// hot, query ==> read-only
	if ($hot !== null) {
		$limit = $limit === null ? 10 : intval($limit);
		$type = $hot === 'cur' ? 'currnum' : ($hot === 'last' ? 'lastnum' : 'total');
		$result = $search->getHotQuery($limit, $type);
		if (count($result) === 0) {
			echo "暂无相关热门搜索记录。\n";
		} else {
			$i = 1;
			printf("序  %s %s\n%s\n", XSUtil::fixWidth('搜索热门关键词(' . $type . ')', 40),
					XSUtil::fixWidth('次数', 10), XSUtil::fixWidth('', 56, '-'));
			foreach ($result as $word => $freq) {
				printf("%2d. %s %d\n", $i, XSUtil::fixWidth($word, 40), $freq);
				$i++;
			}
		}
	} elseif ($query !== null) {
		$query = XSUtil::convertIn($query);
		$limit = $limit === null ? 6 : intval($limit);
		$result = $log_ready ? $search->setFuzzy()->setLimit($limit)->search($query) : array();
		if (count($result) === 0) {
			echo "目前还没有与 \033[7m" . $query . "\033[m 相关的搜索词。\n";
		} else {
			printf("序 %s %s\n%s\n", XSUtil::fixWidth("相关搜索词($query)", 41), XSUtil::fixWidth('次数', 10),
					XSUtil::fixWidth('', 50, '-'));
			for ($i = 0, $total = count($result); $i < $total; $i++) {
				printf("%2d. %s %s\n", $i + 1, XSUtil::fixWidth($result[$i]->body, 40), $result[$i]->total);
			}
		}
	} else {
		// check clean
		if ($clean !== null) {
			echo "清空已有搜索日志数据 ...\n";
			$xs->index->setDb($db)->clean();
		}

		// import from file
		if ($import !== null) {
			if (!file_exists($import) || !($fd = @fopen($import, "r"))) {
				echo "要导入的文件 [$import] 不存在或无法读取！\n";
			} else {
				$search->setTimeout(0);
				echo "开始导入搜索日志文件 ...\n";
				while (($line = fgets($fd, 1024)) !== false) {
					if ($line[0] === '#' || $line[0] === ';') {
						continue;
					}
					if (($pos = strpos($line, "\t")) !== false) {
						$word = trim(substr($line, 0, $pos));
						$wdf = intval(substr($line, $pos + 1));
					} else {
						$word = trim($line);
						$wdf = 1;
					}
					addSearchLog($word, $wdf);
				}
				fclose($fd);
			}
		}

		// delete word
		if ($del !== null) {
			$limit = $limit === null ? 3 : intval($limit);
			$del = XSUtil::convertIn($del);
			foreach (explode(",", $del) as $word) {
				$word = trim($word);
				if ($word === '') {
					continue;
				}
				if ($log_ready) {
					$search->setQuery(null)->addQueryTerm('id', $word);
					if ($search->count() === 1) {
						$xs->index->setDb($db)->del($word);
						echo "成功删除 \033[7m$word\033[m！\n";
						continue;
					}
				}

				echo "不存在 \033[7m$word\033[m，";
				$docs = $log_ready ? $search->setFuzzy()->setLimit($limit)->search($word) : array();
				if (count($docs) === 0) {
					echo "并且没有相关的搜索词。";
				} else {
					echo "相关词：";
					foreach ($docs as $doc) {
						echo "\033[7m" . $doc->body . "\033[m  ";
					}
				}
				echo "\n";
			}
		} elseif ($put !== null) {
			echo "开始增加/更新搜索词 ... \n";
			$put = XSUtil::convertIn($put);

			foreach (explode(',', $put) as $tmp) {
				if (($pos = strpos($tmp, ':')) !== false) {
					$word = trim(substr($tmp, 0, $pos));
					$wdf = intval(substr($tmp, $pos + 1));
				} else {
					$word = trim($tmp);
					$wdf = 1;
				}
				addSearchLog($word, $wdf);
			}
		}

		// check flush
		if ($flush !== null || $del !== null) {
			echo "刷新已提交的日志索引 ...";
			$res = $xs->index->setDb($db)->flushIndex();
			for ($i = 0; $i < 3 && $flush !== null; $i++) {
				echo ".";
				XSUtil::flush();
				sleep(1);
			}
			echo $res ? " 成功\n" : " 失败\n";
		}
		if ($flush !== null || $import !== null || $put !== null) {
			echo "强制刷新未处理的日志记录 ... ";
			echo $xs->index->flushLogging() ? "成功" : "失败";
			echo "\n注意：后台更新需要一些时间，并不是实时完成！\n";
		}
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

// local function add word
function addSearchLog($word, $wdf)
{
	global $search, $log_ready;
	static $record = array();

	if ($word !== '' && $wdf > 0) {
		if (!isset($record[$word]) && $log_ready) {
			$docs = $search->setQuery(null)->addQueryTerm('id', $word)->search();
			if (isset($docs[0])) {
				$record[$word] = $docs[0]->total;
			}
		}
		if (isset($record[$word])) {
			echo "更新 \033[7m$word\033[m 的次数：" . $record[$word] . " + " . $wdf . "\n";
			$record[$word] += $wdf;
		} else {
			echo "新增 \033[7m$word\033[m 次数：$wdf\n";
			$record[$word] = $wdf;
		}
		$search->addSearchLog($word, $wdf);
	}
}

#!/usr/bin/env php
<?php
// Xunsearch server burst test file (standalone)
// Usage: burst.php 
// $Id$

require_once dirname(__FILE__) . '/../util/XSUtil.class.php';

// check arguments
XSUtil::parseOpt(array('i', 'c', 't'));
$test_con = XSUtil::getOpt('c', null, true);
$test_type = XSUtil::getOpt('t');
$test_index = XSUtil::getOpt('i');
if ($test_type !== 'search')
	$test_type = 'index';
if ($test_index === null && $test_con === null)
	$test_con = 100;

// show help
if (XSUtil::getOpt('h', 'help') !== null
	|| ($test_con == null && $test_index == null))
{
	echo <<<EOF
Burst - XS 服务端并发测试工具

用法
    {$_SERVER['argv'][0]} [options] [-c|-i] <num>
	
选项说明
    -c <num>     设置测试的并发总量，用于启动进程，默认 100
    -i <num>     设置测试的序列，用于实际运行进程
    -t <type>    设置测试的类型，其值为 index 或 search，默认为 index
    -h|--help    显示帮助说明

EOF;
	exit(0);
}

// --------------------------------------------
// master process
// --------------------------------------------
if ($test_con !== null)
{
	// open child process
	$total = intval($test_con);
	$pipes = array();
	for ($i = 1; $i <= $total; $i++)
	{
		$cmd = 'php ' . $_SERVER['argv'][0] . ' -t' . $test_type . ' -i' . $i;
		if (!($fd = @popen($cmd, 'r')))
		{
			printf("[%s-%d] run failed\n", $test_type, $i);
			continue;
		}
		stream_set_blocking($fd, 0);
		$pipes[$i] = $fd;
	}
	// loop to read them
	while (count($pipes) > 0)
	{
		$rfds = array_values($pipes);
		$wfds = $xfds = NULL;
		$cc = stream_select($rfds, $wfds, $xfds, NULL);
		if ($cc === false)
		{
			echo "[---] stream_select() failed\n";
			break;
		}
		else if ($cc > 0)
		{
			foreach ($rfds as $fd)
			{
				$buf = fread($fd, 8192);
				if ($buf === false || strlen($buf) === 0)
				{
					$i = array_search($fd, $pipes);
					//printf("[%s-%d] quit\n", $test_type, $i);
					pclose($fd);
					unset($pipes[$i]);
					continue;
				}
				echo $buf;
			}
		}
	}
	exit(0);
}

// --------------------------------------------
// child test process
// --------------------------------------------
require_once dirname(__FILE__) . '/../lib/XS.php';

// app ini
$ini = file_get_contents(dirname(__FILE__) . '/../app/demo.ini');
$ini = str_replace('name = demo', 'name = xs_test', $ini);
$line = sprintf("[%s-%d] ", $test_type, $test_index);

try
{
	$xs = new XS($ini);
	if ($test_type == 'index')
	{
		$data = array('pid' => intval($test_index), 'subject' => 'Hello world', 'chrono' => time());
		$data['message'] = '您好世界';

		$index = $xs->index;
		for ($i = 0; $i < 10; $i++)
		{
			$data2 = $data;
			$data2['pid'] = $data['pid'] + ($i << 16);
			$data2['subject'] .= rand(1, 999);
			$data2['message'] .= rand(1000, 9999);
			$index->update(new XSDocument($data2));
			$line .= " .";
		}
		$line .= " OK";
	}
	else
	{
		$search = $xs->search;
		$query = rand(0, 3) == 0 ? 'subject:world' : '世界';
		for ($i = 0; $i < 10; $i++)
		{
			$search->setQuery($query . ' ' . rand(1, 9999))->search();
			$line .= ' ' . $search->count();
		}
	}
}
catch (XSException $e)
{
	$line .= " ERROR: " . $e;
}
echo $line . "\n";
exit(0);

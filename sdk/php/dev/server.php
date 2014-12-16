#!/usr/bin/env php
<?php
/**
 * XS服务端交互式测试工具
 *
 * @author hightman
 * @link http://www.xunsearch.com/
 * @copyright Copyright &copy; 2011 HangZhou YunSheng Network Technology Co., Ltd.
 * @license http://www.xunsearch.com/license/
 * @version $Id$
 */
error_reporting(E_ALL);
ini_set('display_errors', !extension_loaded('xdebug'));
require_once dirname(__FILE__) . '/../lib/XS.class.php';

// check sapi
if (php_sapi_name() != 'cli') {
	echo "<p>该脚本只能在 cli 模式下运行！</p>\n";
	exit(-1);
}

// load defined constant of commands
$DEFINES = array();
foreach (get_defined_constants() as $key => $value) {
	if (strncmp($key, 'XS_CMD_', 7) || isset($DEFINES[$value])) {
		continue;
	}
	$DEFINES[$value] = $key;
}

// welcome message
echo "欢迎使用 " . XS_PACKAGE_NAME . "/" . XS_PACKAGE_VERSION . " 服务端交互式测试工具!\n";
echo "您可以随时输入 `help' 列出可用命令和使用帮助\n>>";
flush();

// global variable
$server = false;
$stdin = fopen('php://stdin', 'r');
$trace = '';

// loop to read input
while (($line = fgets($stdin, 8192)) !== false) {
	$line = trim($line);
	$args = '';
	if (($pos = strpos($line, ' ')) === false) {
		$cmd = $line;
	} else {
		$cmd = substr($line, 0, $pos);
		$args = substr($line, $pos + 1);
	}

	// quit normal
	if ($cmd == 'exit' || $cmd == 'quit' || $cmd == 'bye') {
		break;
	}

	// execute the cmd
	if ($cmd == 'trace') {
		echo $trace . "\n";
	} elseif (!empty($cmd)) {
		if (!function_exists('cmd_' . $cmd)) {
			$cmd = 'send';
			$args = $line;
		}
		try {
			if ($cmd == 'recv' && (!$server || !$server->hasRespond())) {
				throw new XSException("没有任何可读取的数据");
			}
			if ($cmd == 'open' || $cmd == 'help' || _check_server()) {
				call_user_func('cmd_' . $cmd, $args);
			}
			$trace = '';
		} catch (XSException $e) {
			echo "  " . $e . "\n";
			$trace = $e->getTraceAsString();
		}
	}
	// continue for next cmd
	echo ">>";
	flush();
}

// quit
fclose($stdin);
echo "再见, 已结束!\n";
exit(0);

/**
 * 命令函数: 帮助
 * @param string $args
 */
function cmd_help($args = '')
{
	echo "可用命令:\n";
	echo "  exit|quit|bye  - 离开并退出该工具\n";
	echo "  open [server]  - 打开服务端连接, <server> 的值为: <port>|<host:port>|<path>|<search|index>\n";
	echo "                   当只传入端口时连接主机 localhost, 如果已建过连接, 则 server 参数可省略.\n";
	echo "  close          - 关闭当前服务器连接, 基本上不需要使用, 可以重复 open\n";
	echo "  project <name> [home]\n";
	echo "                 - 设置当前活动的 project name 和 home\n";
	echo "  send <cmd>[ <arg1>[ <arg2>[ <buf>[ <buf1>]]]]\n";
	echo "                 - 往服务器端发送指令并获取响应, 若不需要响应请用 send2 \n";
	echo "                   <cmd>是整数或是 XS_CMD_xxx 这样的预定义变量, 其中 XS_CMD_可以省略\n";
	echo "                   <arg1>,<arg2> 均为0~255的数值\n";
	echo "                   <buf> 字符串, 若包含空格则必须用引号括起来, 中间的引号则需用\\\"进行转义\n";
	echo "                   <buf1> 同上 (最大长度 255, 跟 buf 一样均不得包含换行, 如需换行请用\\n)\n";
	echo "                   字符串转义支持: \\\" \\n \\t \\r \\\\ \\' \\xNN\n";
	echo "  send2 ...      - 参数用法和 send 一样, 但不需要等待响应结果\n";
	echo "  recv [cmd]     - 等待并获取响应指令, 若无任何响应可能会阻塞并等待\n";
	echo "                   若传入cmd则收到cmd或出错为止\n";
	echo "  help           - 显示帮助页面\n";
	echo "  trace          - 显示最近一次抛出 XSException 异常时的调用栈\n";
	echo "  其它 ...       - 自动作为参数调用 send, 并将全部列表作为参数\n";
}

/**
 * 命令函数: 重连或新建服务端连接
 * @param string $args
 */
function cmd_open($args)
{
	global $server;

	if (empty($args)) {
		if (!$server instanceof XSServer) {
			echo "  请问要打开什么服务端? (索引默认端口:8383, 搜索默认端口:8384)\n";
			echo "  用法: open <port>|<host:port>|<path>|<search|index>\n";
			return;
		}
		$res = $server->reopen();
		echo "  重新打开服务端连接\n";
	} else {
		$args == 'index' && $args = '8383';
		$args == 'search' && $args = '8384';
		$server = new XSServer($args);
		echo "  已打开服务端连接: $args\n";
	}
	$server->setTimeout(0);
}

/**
 * 命令函数: 关闭连接
 * @param string $args
 */
function cmd_close()
{
	global $server;
	$server->close();
	echo "  已关闭服务端.\n";
}

/**
 * 命令函数: 设置项目
 * @param string $args
 */
function cmd_project($args)
{
	global $server;

	$name = _get_send_arg($args);
	$home = _get_send_arg();
	$server->setProject($name, $home === false ? '' : $home);
	echo "  完成, 当前项目名为: $name\n";
}

/**
 * 命令函数: 发送指令
 * @param string $args
 */
function cmd_send2($args)
{
	global $server;

	// get the command
	$cmd = _get_send_arg($args);
	if ($cmd === false) {
		echo "  用法: " . $GLOBALS['cmd'] . " <cmd>[ <arg1>[ <arg2>[ <buf>[ <buf1>]]]]\n";
		return false;
	}
	$cmd = _get_def_cmd($cmd2 = $cmd);
	if ($cmd == 0) {
		echo "  未定义的命令: $cmd2\n";
		return false;
	}

	// parse arguments: arg1, arg2, buf, buf1
	$arg1 = _get_send_arg();
	$buf = null;
	if (!is_numeric($arg1)) {
		$buf = $arg1;
		$arg1 = $arg2 = 0;
	} else {
		$arg2 = _get_send_arg();
		if (!is_numeric($arg2)) {
			$buf = $arg2;
			$arg2 = 0;
		}
	}
	$buf = is_null($buf) ? _get_send_arg() : $buf;
	$buf1 = _get_send_arg();
	$arg1 = $arg1 === false ? 0 : intval($arg1);
	$arg2 = $arg2 === false ? 0 : intval($arg2);
	$buf = $buf === false ? '' : $buf;
	$buf1 = $buf1 === false ? '' : $buf1;

	// show sending command
	printf(">>发送到服务端的指令: {%s, %d, %d, %d, %d}...\n", _get_cmd_def($cmd), $arg1, $arg2, strlen($buf1),
			strlen($buf));
	$cmd = new XSCommand($cmd, $arg1, $arg2, $buf, $buf1);

	// send command
	$server->sendCommand($cmd);

	// dont ans
	if ($cmd->cmd & 0x80) {
		echo "  完成, 不过这条命令无需服务端应答!\n";
		return false;
	}
	return true;
}

/**
 * 命令函数: 收取响应指令
 * @param string $args
 */
function cmd_recv($args = '')
{
	global $server;

	$wait = empty($args) ? 0 : _get_def_cmd($args);
	while (true) {
		$res = $server->getRespond();

		// force to decode som command(unpack)
		if ($res->cmd == XS_CMD_OK && strlen($res->buf) == 4
				&& ($res->arg == XS_CMD_OK_SEARCH_TOTAL || $res->arg == XS_CMD_OK_DB_TOTAL)) {
			$tmp = unpack('Icount', $res->buf);
			$res->buf = '{count:' . $tmp['count'] . '}';
		}
		if ($res->cmd == XS_CMD_SEARCH_RESULT_DOC && strlen($res->buf) == 20) {
			$tmp = unpack('Idocid/Irank/Iccount/ipercent/fweight', $res->buf);
			$res->buf = sprintf('{docid:%u, rank:%d, ccount:%d, percent:%d%%, weight:%.2f}', $tmp['docid'],
					$tmp['rank'], $tmp['ccount'], $tmp['percent'], $tmp['weight']);
		}
		// output
		printf("<<<CMD: %s\n<<<ARG: %s\n<<<BUF1(%d): %s\n<<<BUF(%d): %s\n<<<END\n",
				_get_cmd_def($res->cmd),
				$res->cmd == XS_CMD_SEARCH_RESULT_FIELD ? $res->arg : _get_cmd_def($res->arg), strlen($res->buf1),
				$res->buf1, strlen($res->buf), $res->buf);
		// break
		if ($wait == 0 || $res->cmd == XS_CMD_ERR || $res->cmd == $wait) {
			break;
		}
	}
}

/**
 * 命令函数: 运行指令
 * @param string $args
 */
function cmd_send($args)
{
	if (cmd_send2($args)) {
		cmd_recv();
	}
}

/**
 * 检查服务器是否初始化
 * @global XunServer $server
 * @return bool
 */
function _check_server()
{
	global $server;
	if ($server instanceof XSServer) {
		return true;
	}
	echo "  尚未打开服务端连接, 请先先使用 `open' 命令\n";
	return false;
}

/**
 * 把 XS_CMD_xxx 定义的整型值显示为字符串
 * @param int $cmd
 * @return string
 */
function _get_cmd_def($cmd)
{
	global $DEFINES;
	if (isset($DEFINES[$cmd])) {
		return $DEFINES[$cmd];
	}
	return strval($cmd);
}

/**
 * 将输入的字符串转换成有效的 cmd 值
 * @param string $cmd (numeric/string)
 * @return integer
 */
function _get_def_cmd($cmd)
{
	$cmd2 = strtoupper($cmd);
	if (is_numeric($cmd)) {
		global $DEFINES;
		$cmd = intval($cmd);
		return isset($DEFINES[$cmd]) ? $cmd : 0;
	} elseif (defined('XS_CMD_' . $cmd2)) {
		return constant('XS_CMD_' . $cmd2);
	} elseif (defined('XS_CMD_SEARCH_' . $cmd2)) {
		return constant('XS_CMD_SEARCH_' . $cmd2);
	} elseif (defined('XS_CMD_INDEX_' . $cmd2)) {
		return constant('XS_CMD_INDEX_' . $cmd2);
	} elseif (defined('XS_CMD_QUERY_' . $cmd2)) {
		return constant('XS_CMD_QUERY_' . $cmd2);
	}
	return 0;
}

/**
 * 读取并返回一个 send 参数
 * @param string $args 所有参数组成的字符串, 从读取第二参数开始 $args 应为 null
 * @static string $buf
 * @static int $off
 * @return mixed 成功返回参数字符串, 失败或没有更多参数时返回 false
 */
function _get_send_arg($args = NULL)
{
	static $buf = NULL, $off = 0;

	if ($args !== NULL) {
		$off = 0;
		$buf = $args;
	}
	if ($buf === NULL || $off >= strlen($buf)) {
		return false;
	}

	// get start pos
	$quote = false;
	$len = strlen($buf);
	for ($start = $off; $start < $len; $start++) {
		$char = substr($buf, $start, 1);
		if (strpos(" \r\n\t", $char) !== false) {
			continue;
		}
		if ($char == '"') {
			$quote = true;
			$start++;
		}
		break;
	}
	// cut the string
	for ($ret = '', $end = $start; $end < $len; $end++) {
		$char = substr($buf, $end, 1);

		// support: \r,\n,\t,\",\\,\xNN
		if ($char == '\\' && $end < ($len - 1)) {
			$char2 = substr($buf, ++$end, 1);
			if ($char2 == 'r') {
				$char = "\r";
			} elseif ($char2 == 'n') {
				$char = "\n";
			} elseif ($char2 == 't') {
				$char = "\t";
			} elseif ($char2 == 'x' && $end < ($len - 2)) {
				$char = chr(hexdec(substr($buf, $end, 2)));
				$end += 2;
			} else {
				// keep unsupported chars
				if ($char2 != '"' && $char2 != '\\' && $char2 != "'") {
					$ret .= $char;
				}
				$char = $char2;
			}
		} elseif ($quote && $char == '"') {
			$end++;
			break;
		} elseif (!$quote && strpos(" \t\r\n", $char) !== false) {
			break;
		}
		$ret .= $char;
	}

	// pack() support
	if (!empty($ret) && !strncasecmp($ret, 'pack(', 5) && substr($ret, -1, 1) == ')') {
		eval('$ret = ' . $ret . ';');
	}

	$off = $end;
	return ($end == $start ? false : $ret);
}

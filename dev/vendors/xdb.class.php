<?php
/* ----------------------------------------------------------------------- *\
   PHP4代码版XDB - (XTreeDB.class.php)
   -----------------------------------------------------------------------
   作者: 马明练(hightman) (MSN: MingL_Mar@msn.com) (php-QQ群: 17708754)
   网站: http://www.hi-php.com
   时间: 2007/05/01 (update: 2007/05/29)
   版本: 0.1
   目的: 取代 cdb/gdbm 快速存取分词词典, 因大部分用户缺少这些基础配件和知识
		 xdb 改自前身 hdb 为了更好的和C版兼容, 故修改. 目前此版产生了机器字
		 节序依赖, 故打开词典时会自做检查
   功能:		
         这是一个类似于 cdb/gdbm 的 PHP 代码级数据类库, 通过 key, value 的方
		 式存取数据, 使用非常简单.

		 适用于快速根据唯一主键查找数据

   效能:
         1. 效率高(20万记录以上比php内建的cdb还要快), 经过优化后 35万记录时
		    树的最大深度为5, 查找效率高,单个文件
		 2. 文件小(缺省设置下, 基础数据约 100KB, 之后每条记录为 key, value的
		    总长度+13bytes
		 4. PHP 代码级, 修改维护方便
		 5. 提供内建二叉树优化函数, 提供存取结构图绘制接口, 提供遍历接口
		 6. 数据可快速更新, 而 cdb 是只读的或只写的

   缺点:
         1. 对于unique key来说, 一经增加不可清除 (可以将value设为空值)
		 2. 当更新 value 时, 如果新 value 较长则旧的记录直接作废, 长期修改
		    可能会导致文件有一些无用的膨胀, 这类情况可以调用遍历接口完全重
			建整个数据库
		 3. 由于是 php 代码级的引擎, 性能上比 gdbm 没有什么优势
		 4. IO操作, 可以考虑将数据文件放到 memfs 上 (linux/bsd)
		 5. key 最大长度为 240bytes, value 最大长度为 65279 bytes, 整个文件最大为 4G
		 6. 不可排序和随机分页读取

   用法: (主要的方法)

   1. 建立类操作句柄, 构造函数: XTreeDB([int mask [, int base ]])
	  可选参数(仅针对新建数据有效): mask, base 均为整型数, 其中
	    mask 是 hash 求模的基数, 建议选一个质数, 大约为总记录数的 1/10 即可.
		base 是 hash 数据计算的基数, 建议使用默认值. ``h = ((h << 5) + h) ^ c''

      $XDB = new XTreeDB;

   2. 打开数据文件, Bool Open(string fpath [, string mode])
      必要参数 fpath 为数据文件的路径, 可选参数 mode 的值为 r 或 w, 分别表示只
	  读或读写方式打开数据库. 成功返回 true, 失败返回 false.

      缺省情况下是以只读方式打开, 即 mode 的缺省值为 'r'
      $XDB->Open('/path/to/dict.xdb');

	  或以读写方式打开(新建数据时必须), mode 值为 'w', 此时数据库可读可写, 并锁定写
	  $XDB->Open('/path/to/dict.xdb', 'w');

   3. 根据 key 读取数据 mixed Get(string key [, bool verbose])
      成功查找到 key 所对应的数据时返回数据内容, 类型为 string
	  当 key 不存在于数据库中时或产生错误直接返回 false
	  (*注* 当 verbose 被设为 true 时, 则返回一个完整的记录数组, 含 key&value, 仅用于调试目的)

      $value = $XDB->Get($key);
	  或
	  $debug = $XDB->Get($key, true); print_r($debug);

   4. 存入数据 bool Put(string key [, string value])
      成功返回 true, 失败或出错返回 false , 必须以读写方式打开才可调用
	  注意存入的数据目前只支持 string 类型, 有特殊需要可以使用 php 内建的 serialize 将 array 转换
	  成 string 取出时再用 unserialize() 还原

	  $result = $XDB->Put($key, $value);

   5. 关闭数据库, void Close()
      $XDB->Close();

   6. 查询文件版本号, string Version()
      返回类似 XDB/0.1 之类的格式, 是当前文件的版本号

   7. 记录遍历, mixed Next()
      返回一条记录key, value 组成的数组, 并将内部指针往后移一位, 可调用 Reset() 重置指针
	  当没有记录时会返回 false, 典型应用如下

	  $XDB->Reset();
	  while ($tmp = $XDB->Next())
	  {
		  echo "$tmp[key] => $tmp[value]\n";
	  }
	  也可用于导出数据库重建新的数据库, 以清理过多的重写导致的文件空档.

   8. 遍历指针复位, void Reset()
      此函数仅为搭配 Next() 使用
	  $XDB->Reset();

   9. 优化数据库, 将数据库中的 btree 转换成完全二叉树. void Optimize([int index])
      由于数据库针对 key 进行 hash 分散到 mask 颗二叉树中, 故这里的 index 为 0~[mask-1]
	  缺省情况下 index 值为 -1 会优化整个数据库, 必须以读写方式打开的数据库才能用该方法

	  $XDB->Optimize();

  10. 打印分析树, 绘出存贮结构的树状图, void Draw([int index])
      参数 index 同 Optimize() 的参数, 本函数无返回值, 直接将结果 echo 出来, 仅用于调试和观看
	  分析

	  $XDB->Draw(0);
	  $XDB->Draw(1);
	  ...

\* ----------------------------------------------------------------------- */

// Constant Define
define ('XDB_FLOAT_CHECK',	3.14);
define ('XDB_HASH_BASE',	0xf422f);
define ('XDB_HASH_PRIME',	2047);
define ('XDB_VERSION',		34);
define ('XDB_TAGNAME',		'XDB');
define ('XDB_MAXKLEN',		0xf0);

// Class object Declare
class XTreeDB
{
	// Public var
	var $fd = false;
	var $mode = 'r';
	var $hash_base = XDB_HASH_BASE;
	var $hash_prime = XDB_HASH_PRIME;
	var $version = XDB_VERSION;
	var $fsize = 0;

	// Private
	var $trave_stack = array();
	var $trave_index = -1;

	// Debug test
	var $_io_times = 0;

	// Constructor Function
	function XTreeDB($base = 0, $prime = 0)
	{		
		if (0 != $base) $this->hash_base = $base;
		if (0 != $prime) $this->hash_prime = $prime;
	}

	// Open the database: read | write
	function Open($fpath, $mode = 'r')
	{
		// open the file
		$this->Close();

		$newdb = false;
		if ($mode == 'w')
		{
			// write & read only
			if (!($fd = @fopen($fpath, 'rb+')))
			{
				if (!($fd = @fopen($fpath, 'wb+')))
				{
					trigger_error("XDB::Open(" . basename($fpath) . ",w) failed.", E_USER_WARNING);
					return false;
				}
				// create the header
				$this->_write_header($fd);

				// 32 = header, 8 = Pointer
				$this->fsize = 32 + 8 * $this->hash_prime;	
				$newdb = true;
			}
		}
		else
		{
			// read only
			if (!($fd = @fopen($fpath, 'rb')))
			{
				trigger_error("XDB::Open(" . basename($fpath) . ",r) failed.", E_USER_WARNING);
				return false;
			}
		}

		// check the header
		if (!$newdb && !$this->_check_header($fd))
		{
			trigger_error("XDB::Open(" . basename($fpath) . "), invalid xdb format.", E_USER_WARNING);
			fclose($fd);
			return false;
		}

		// set the variable
		$this->fd = $fd;
		$this->mode = $mode;
		$this->Reset();

		// lock the file description until close
		if ($mode == 'w')
			flock($this->fd, LOCK_EX);

		return true;
	}

	// Insert Or Update the value
	function Put($key, $value)
	{
		// check the file description
		if (!$this->fd || $this->mode != 'w')
		{
			trigger_error("XDB::Put(), null db handler or readonly.", E_USER_WARNING);
			return false;
		}

		// check the length
		$klen = strlen($key);
		$vlen = strlen($value);
		if (!$klen || $klen > XDB_MAXKLEN)
			return false;

		// try to find the old data
		$rec = $this->_get_record($key);
		if (isset($rec['vlen']) && ($vlen <= $rec['vlen']))
		{
			// update the old value & length
			if ($vlen > 0)
			{
				fseek($this->fd, $rec['voff'], SEEK_SET);
				fwrite($this->fd, $value, $vlen);
			}

			if ($vlen < $rec['vlen'])
			{
				$newlen = $rec['len'] + $vlen - $rec['vlen'];
				$newbuf = pack('I', $newlen);
				fseek($this->fd, $rec['poff'] + 4, SEEK_SET);
				fwrite($this->fd, $newbuf, 4);
			}
			return true;
		}

		// 构造数据
		$new = array('loff' => 0, 'llen' => 0, 'roff' => 0, 'rlen' => 0);
		if (isset($rec['vlen']))
		{
			$new['loff'] = $rec['loff'];
			$new['llen'] = $rec['llen'];
			$new['roff'] = $rec['roff'];
			$new['rlen'] = $rec['rlen'];
		}
		$buf  = pack('IIIIC', $new['loff'], $new['llen'], $new['roff'], $new['rlen'], $klen);
		$buf .= $key . $value;
		$len  = $klen + $vlen + 17;

		$off  = $this->fsize;
		fseek($this->fd, $off, SEEK_SET);
		fwrite($this->fd, $buf, $len);
		$this->fsize += $len;

		$pbuf = pack('II', $off, $len);
		fseek($this->fd, $rec['poff'], SEEK_SET);
		fwrite($this->fd, $pbuf, 8);
		return true;
	}

	// Read the value by key
	function Get($key, $debug = false)
	{
		// check the file description
		if (!$this->fd)
		{
			trigger_error("XDB::Get(), null db handler.", E_USER_WARNING);
			return false;
		}

		$klen = strlen($key);
		if ($klen == 0 || $klen > XDB_MAXKLEN)
			return false;

		// get the data?
		$rec = $this->_get_record($key);
		if ($debug) 
			return $rec;

		if (!isset($rec['vlen']) || $rec['vlen'] == 0)
			return false;
		
		return $rec['value'];
	}

	// Read the each key & value
	// return array(key => xxx, value => xxx)
	function Next()
	{
		// check the file description
		if (!$this->fd)
		{
			trigger_error("XDB::Next(), null db handler.", E_USER_WARNING);
			return false;
		}

		// Traversal the all tree
		if (!($ptr = array_pop($this->trave_stack)))
		{
			do
			{
				$this->trave_index++;
				if ($this->trave_index >= $this->hash_prime)
					break;

				$poff = $this->trave_index * 8 + 32;
				fseek($this->fd, $poff, SEEK_SET);
				$buf = fread($this->fd, 8);
				if (strlen($buf) != 8) 
				{
					$ptr = false;
					break;
				}

				$ptr = unpack('Ioff/Ilen', $buf);
			}
			while ($ptr['len'] == 0);
		}

		// end the all records?
		if (!$ptr || $ptr['len'] == 0)
			return false;

		// read the record
		$rec = $this->_tree_get_record($ptr['off'], $ptr['len']);

		// push the left & right
		if ($rec['llen'] != 0)
		{
			$left = array('off' => $rec['loff'], 'len' => $rec['llen']);
			array_push($this->trave_stack, $left);
		}
		if ($rec['rlen'] != 0)
		{
			$right = array('off' => $rec['roff'], 'len' => $rec['rlen']);
			array_push($this->trave_stack, $right);
		}

		// return value
		return $rec;
	}

	// Traversal every tree... & debug to test
	function Draw($i = -1)
	{
		if ($i < 0 || $i >= $this->hash_prime)
		{
			$i = 0;
			$j = $this->hash_prime;
		}
		else
		{
			$j = $i + 1;
		}

		echo "Draw the XDB data [$i ~ $j]. (" . trim($this->Version()) . ")\n\n";
		while ($i < $j)
		{
			$poff = $i * 8 + 32;
			fseek($this->fd, $poff, SEEK_SET);
			$buf = fread($this->fd, 8);
			if (strlen($buf) != 8) break;
			$ptr = unpack('Ioff/Ilen', $buf);
			
			$this->_cur_depth = 0;
			$this->_node_num = 0;
			$this->_draw_node($ptr['off'], $ptr['len']);
			echo "-------------------------------------------\n";
			echo "Tree(xdb) [$i] max_depth: {$this->_cur_depth} nodes_num: {$this->_node_num}\n";			
			$i++;
		}
	}

	// Reset the inner pointer
	function Reset()
	{
		$this->trave_stack = array();
		$this->trave_index = -1;
	}

	// Show the version
	function Version()
	{
		$ver = (is_null($this) ? XDB_VERSION : $this->version);
		$str = sprintf("%s/%d.%d", XDB_TAGNAME, ($ver >> 5), ($ver & 0x1f));
		if (!is_null($this)) $str .= " <base={$this->hash_base}, prime={$this->hash_prime}>";
		return $str;
	}

	// Close the DB
	function Close()
	{
		if (!$this->fd)
			return;		
		
		if ($this->mode == 'w')
		{
			$buf = pack('I', $this->fsize);
			fseek($this->fd, 12, SEEK_SET);
			fwrite($this->fd, $buf, 4);
			flock($this->fd, LOCK_UN);
		}
		fclose($this->fd);
		$this->fd = false;		
	}

	// Optimize the tree
	function Optimize($i = -1)
	{
		// check the file description
		if (!$this->fd || $this->mode != 'w')
		{
			trigger_error("XDB::Optimize(), null db handler or readonly.", E_USER_WARNING);
			return false;
		}

		// get the index zone:
		if ($i < 0 || $i >= $this->hash_prime)
		{
			$i = 0;
			$j = $this->hash_prime;
		}
		else
		{
			$j = $i + 1;
		}

		// optimize every index
		while ($i < $j)
		{
			$this->_optimize_index($i);
			$i++;
		}
	}

	// optimize a node
	function _optimize_index($index)
	{
		static $cmp = false;
		$poff = $index * 8 + 32;

		// save all nodes into array()
		$this->_sync_nodes = array();
		$this->_load_tree_nodes($poff);

		$count = count($this->_sync_nodes);
		if ($count < 3) return;

		// sync the nodes, sort by key first
		if ($cmp == false) $cmp = create_function('$a,$b', 'return strcmp($a["key"],$b["key"]);');
		usort($this->_sync_nodes, $cmp);
		$this->_reset_tree_nodes($poff, 0, $count - 1);
		unset($this->_sync_nodes);
	}

	// load tree nodes
	function _load_tree_nodes($poff)
	{
		fseek($this->fd, $poff, SEEK_SET);
		$buf = fread($this->fd, 8);
		if (strlen($buf) != 8) return;

		$tmp = unpack('Ioff/Ilen', $buf);
		if ($tmp['len'] == 0) return;
		fseek($this->fd, $tmp['off'], SEEK_SET);

		$rlen = XDB_MAXKLEN + 17;
		if ($rlen > $tmp['len']) $rlen = $tmp['len'];
		$buf = fread($this->fd, $rlen);

		$rec = unpack('Iloff/Illen/Iroff/Irlen/Cklen', substr($buf, 0, 17));
		$rec['off'] = $tmp['off'];
		$rec['len'] = $tmp['len'];
		$rec['key'] = substr($buf, 17, $rec['klen']);
		$this->_sync_nodes[] = $rec;
		unset($buf);

		// left
		if ($rec['llen'] != 0) $this->_load_tree_nodes($tmp['off']);
		// right
		if ($rec['rlen'] != 0) $this->_load_tree_nodes($tmp['off'] + 8);
	}

	// sync the tree
	function _reset_tree_nodes($poff, $low, $high)
	{
		if ($low <= $high)
		{
			$mid = ($low+$high)>>1;
			$node = $this->_sync_nodes[$mid];
			$buf = pack('II', $node['off'], $node['len']);

			// left
			$this->_reset_tree_nodes($node['off'], $low, $mid - 1);
			// right
			$this->_reset_tree_nodes($node['off'] + 8, $mid + 1, $high);
		}
		else
		{
			$buf = pack('II', 0, 0);
		}

		fseek($this->fd, $poff, SEEK_SET);
		fwrite($this->fd, $buf, 8);
	}

	// Privated Function
	function _get_index($key)
	{
		$l = strlen($key);
		$h = $this->hash_base;
		while ($l--)
		{
			$h += ($h << 5);
			$h ^= ord($key[$l]);
			$h &= 0x7fffffff;
		}
		return ($h % $this->hash_prime);
	}

	// draw the tree nodes by off & len
	function _draw_node($off, $len, $rl = 'T', $icon = '', $depth = 0)
	{
		if ($rl == 'T')	echo '(Ｔ) ';
		else
		{
			echo $icon;
			if ($rl == 'L')
			{
				$icon .= ' ┃';
				echo ' ┟(Ｌ) ';
			}
			else
			{
				$icon .= ' 　';
				echo ' └(Ｒ) ';
			}
		}
		if ($len == 0)
		{
			echo "<NULL>\n";
			return;
		}

		$rec = $this->_tree_get_record($off, $len);
		echo "{$rec[key]} (vlen={$rec[vlen]}, voff={$rec[voff]})\n";
		unset($rec['key'], $rec['value']);

		// debug used
		$this->_node_num++;
		$depth++;
		if ($depth >= $this->_cur_depth)
			$this->_cur_depth = $depth;		

		// Left node & Right Node
		$this->_draw_node($rec['loff'], $rec['llen'], 'L', $icon, $depth);
		$this->_draw_node($rec['roff'], $rec['rlen'], 'R', $icon, $depth);
	}

	// Check XDB Header
	function _check_header($fd)
	{
		fseek($fd, 0, SEEK_SET);
		$buf = fread($fd, 32);
		if (strlen($buf) !== 32) return false;
		$hdr = unpack('a3tag/Cver/Ibase/Iprime/Ifsize/fcheck/a12reversed', $buf);
		if ($hdr['tag'] != XDB_TAGNAME) return false;

		// check the fsize
		$fstat = fstat($fd);
		if ($fstat['size'] != $hdr['fsize'])
			return false;

		// check float?
		
		$this->hash_base = $hdr['base'];
		$this->hash_prime = $hdr['prime'];
		$this->version = $hdr['ver'];
		$this->fsize = $hdr['fsize'];
		return true;
	}

	// Write XDB Header
	function _write_header($fd)
	{
		$buf = pack('a3CiiIfa12', XDB_TAGNAME, $this->version,
			$this->hash_base, $this->hash_prime, 0, XDB_FLOAT_CHECK, '');

		fseek($fd, 0, SEEK_SET);
		fwrite($fd, $buf, 32);
	}

	// get the record by first key
	function _get_record($key)
	{
		$this->_io_times = 1;
		$index = ($this->hash_prime > 1 ? $this->_get_index($key) : 0);
		$poff = $index * 8 + 32;
		fseek($this->fd, $poff, SEEK_SET);
		$buf = fread($this->fd, 8);

		if (strlen($buf) == 8) $tmp = unpack('Ioff/Ilen', $buf);
		else $tmp = array('off' => 0, 'len' => 0);
		return $this->_tree_get_record($tmp['off'], $tmp['len'], $poff, $key);
	}

	// get the record by tree
	function _tree_get_record($off, $len, $poff = 0, $key = '')
	{
		if ($len == 0)
			return (array('poff' => $poff));
		$this->_io_times++;
		
		// get the data & compare the key data
		fseek($this->fd, $off, SEEK_SET);
		$rlen = XDB_MAXKLEN + 17;
		if ($rlen > $len) $rlen = $len;
		$buf = fread($this->fd, $rlen);
		$rec = unpack('Iloff/Illen/Iroff/Irlen/Cklen', substr($buf, 0, 17));		
		$fkey = substr($buf, 17, $rec['klen']);
		$cmp = ($key ? strcmp($key, $fkey) : 0);
		if ($cmp > 0)
		{
			// --> right
			unset($buf);
			return $this->_tree_get_record($rec['roff'], $rec['rlen'], $off + 8, $key);
		}
		else if ($cmp < 0)
		{
			// <-- left
			unset($buf);
			return $this->_tree_get_record($rec['loff'], $rec['llen'], $off, $key);
		}
		else {
			// found!!
			$rec['poff'] = $poff;
			$rec['off'] = $off;
			$rec['len'] = $len;
			$rec['voff'] = $off + 17 + $rec['klen'];
			$rec['vlen'] = $len - 17 - $rec['klen'];
			$rec['key'] = $fkey;
			
			fseek($this->fd, $rec['voff'], SEEK_SET);
			$rec['value'] = fread($this->fd, $rec['vlen']);
			return $rec;
		}
	}
}


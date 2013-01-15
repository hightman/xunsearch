<?php
// Bootstrap file for PHPUNIT
// Force to restart the xs-backend servers
// $Id$
$prefix = trim(file_get_contents(getenv('HOME') . '/.xs_installed'));
//shell_exec($prefix . '/bin/xs-ctl.sh restart');

// global temp ini files
$GLOBALS['fixIniData'] = array(
	// test1
	'/tmp/xs_test1.ini' => '
; comment lines
server.index = 8383

[pid]
type = id

[subject]
type = title

[message]
type = body

[chrono]
type = numeric

[date]
type = date
index = self
tokenizer = full

[other]
index = mixed
			
',
	// test2
	'/tmp/xs_test2.ini' => '
project.name = test2			
project.default_charset = gbk
server.search = localhost:8384

; add extended options
[pid]
type = id

[subject]
type = title
index = self
weight = 3
phrase = no

[message]
type = body
; try to change index type
index = both
cutlen = 100

[chrono]
type = numeric

[date]
type = date
index = mixed
tokenizer = split(/)

[other]
type = string
index = both
weight = 0
phrase = yes

');

// add XS required
require_once dirname(__FILE__) . '/../lib/XS.class.php';


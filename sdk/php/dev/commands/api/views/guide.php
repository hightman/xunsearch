<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta name="language" content="zh-cn" />
<link rel="stylesheet" type="text/css" href="../api/css/style.css" />
<link rel="stylesheet" type="text/css" href="../api/css/guide.css" />
<link rel="stylesheet" type="text/css" href="../api/css/highlight.css" />
<title><?php echo $this->pageTitle; ?></title>
</head>

<body>
<div id="apiPage">

<div id="apiHeader">
<a href="http://www.xunsearch.com" target="_blank">Xunsearch PHP-SDK</a> v<?php echo $this->version; ?> 权威指南
</div><!-- end of header -->

<div id="content" class="markdown">
	<?php echo $this->formatMarkdown($content); ?>
	<div class="clear"></div>
</div><!-- end of content -->
<div id="guideNav">
	<?php if (isset($prev)): ?>
	<div class="prev"><?php echo CHtml::link('&laquo; ' . $prev['label'], $prev['name'] . '.html'); ?></div>
	<?php endif; ?>
	<?php if (isset($next)): ?>
	<div class="next"><?php echo CHtml::link($next['label'] . ' &raquo;', $next['name'] . '.html'); ?></div>
	<?php endif; ?>
	<div class="clear"></div>
</div><!-- end of nav -->

<div id="apiFooter">
Copyright &copy; 2008-2011 by <a href="http://www.xunsearch.com" target="_blank">杭州云圣网络科技有限公司</a><br/>
All Rights Reserved.<br/>
</div><!-- end of footer -->

</div><!-- end of page -->
</body>
</html>
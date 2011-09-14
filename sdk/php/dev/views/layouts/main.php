<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<title><?php echo CHtml::encode($this->pageTitle); ?></title>
<style type="text/css">
#guide-nav { margin-top: 10px; border-top: 1px solid #ccc; padding-top: 5px; overflow: hidden; }
#guide-nav .prev { float: left; }
#guide-nav .next { float: right; }
div.revision { color: #888; font-size: 80%; }
div.clear { clear: both; float: none; height: 0; line-height: 0; }
.toc {
	float: left;
	width: 200px;
	font-size: 12px;
}

.toc ul {
	margin: 10px 0;
	padding: 0 0 0 10px;
}

.markdown {
    font-size: 14px;
	float: left;
	width: 660px;
}
.markdown pre {
    background: none repeat scroll 0 0 #FCFCFC;
    border-bottom: 1px solid #EEEEEE;
    border-top: 1px solid #EEEEEE;
    display: block;
    font-family: Menlo,Consolas,"Courier New",Courier,mono;
    font-size: 10pt;
    margin: 1em 0;
    padding: 1em;
    white-space: pre-wrap;
    word-wrap: break-word;
}
.markdown code, .markdown abbr {
    border-bottom: 1px dotted #CCCCCC;
    font-family: Menlo,Consolas,"Courier New",Courier,mono;
}
.markdown h1 {
    color: #555555;
}
.markdown h2, .markdown h3, .markdown h4 {
    color: #444444;
    font-size: 1.3em;
    margin-bottom: 0.25em;
    margin-top: 1.5em;
}
.markdown h3 {
    font-size: 1.2em;
}
.markdown h4 {
    font-size: 1.15em;
}
.markdown .revision {
    color: #999999;
    font-size: 0.8em;
    margin-top: 5em;
}
.markdown .tip, .markdown .info, .markdown .note {
    background-color: #FFFAE6;
    background-image: url("<?php echo Yii::app()->request->baseUrl; ?>/img/tip.gif");
    background-position: 10px 45%;
    background-repeat: no-repeat;
    border-bottom: 1px solid #E4DFB8;
    border-color: #E4DFB8;
    border-top: 1px solid #E4DFB8;
    margin: 1em 0;
    padding: 1em 1em 0 55px;
}
.markdown .note {
    background-color: #FFE6E6;
    background-image: url("<?php echo Yii::app()->request->baseUrl; ?>/img/note.gif");
    border-color: #D9C3C3;
}
.markdown .info {
    background-color: #EBFFCE;
    background-image: url("<?php echo Yii::app()->request->baseUrl; ?>/img/info.gif");
    border-color: #B4DAA5;
}

.markdown .toc {
    background-color: #F9F9F9;
    border: 1px solid #EEEEEE;
    float: right;
    font-size: 0.95em;
    line-height: 125%;
    margin: 0.75em 0 1em 1em;
    padding: 0.7em 1em;
    width: 210px;
}
.markdown .toc ol {
    color: #666666;
    margin: 0.5em 0;
}
.markdown .toc li {
    padding: 0.3em 0;
}
.markdown .toc code {
    font-size: 0.95em;
}
.markdown .toc .ref {
    margin: 0 0 0.3em;
}
.markdown .toc .ref.level-3 {
    margin-left: 20px;
}
.markdown .anchor {
    visibility: hidden;
}
.markdown h2:hover .anchor, .markdown h3:hover .anchor, .markdown h4:hover .anchor {
    visibility: visible;
}
.markdown .image {
    border: 1px solid #E5E5E5;
    margin: 1em 0;
    text-align: center;
}
.markdown .image > p {
    background: none repeat scroll 0 0 #F5F5F5;
    margin: 0;
    padding: 0.5em;
    text-align: center;
}
.markdown .image img {
    margin: 1em;
}
table.download { width: 100%; }
.download th, .download td {
    border: 1px solid #DDDDDD;
    padding: 0.2em 0.5em;
}
.download th {
    background: none repeat scroll 0 0 #EEEEEE;
    text-align: left;
}
</style>
</head>
<body>
<div id="header">
	XunSearch PHP-SDK 文档
	| <?php echo CHtml::link('首页', array('/')); ?>
	| <?php echo CHtml::link('指南', array('guide/toc')); ?>
	| <?php echo CHtml::link('API', '/xs-php/doc/api/'); ?>
	<hr />
</div><!-- end of header -->

<div id="content">
<?php echo $content; ?>
</div><!-- end of content -->

<div id="footer">
	<hr />
	Copyright &copy; 2008-2011 by <a href="http://www.xunsearch.com">xunsearch.com</a><br/>
	All Rights Reserved.<br/>
</div><!-- end of footer -->

</body>
</html>

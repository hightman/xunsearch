<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<meta name="GENERATOR" content="xunsearch http://www.xunsearch.com">
</head>
<body>
<object type="text/site properties">
<param name="Window Styles" value="0x800025">
<param name="FrameName" value="right">
<param name="ImageType" value="Folder">
<param name="comment" value="title:Online Help">
<param name="comment" value="base:index.html">
</object>
<ul>
	<li><object type="text/sitemap">
		<param name="Name" value="权威指南">
		<param name="Local" value="guide/toc.html">
		</object>
		<ul>
<?php foreach ($guides as $parts): ?>
			<li><object type="text/sitemap">
				<param name="Name" value="<?php echo $parts['label']; ?>">
				</object>
				<ul>
<?php foreach ($parts['items'] as $name => $label): ?>
					<li><object type="text/sitemap">
						<param name="Name" value="<?php echo $label; ?>">
						<param name="Local" value="guide/<?php echo $name; ?>.html">
						</object>
<?php endforeach; ?>
				</ul>
<?php endforeach; ?>
		</ul>
	</li>
	<li><object type="text/sitemap">
		<param name="Name" value="API 指南">
		<param name="Local" value="api/index.html">
		</object>
		<ul>
<?php foreach($this->packages as $package=>$classes): ?>
			<li><object type="text/sitemap">
				<param name="Name" value="<?php echo $package; ?>">
				</object>
				<ul>
<?php foreach($classes as $class): ?>
					<li><object type="text/sitemap">
						<param name="Name" value="<?php echo $class; ?>">
						<param name="Local" value="api/<?php echo $class; ?>.html">
						</object>
<?php endforeach; ?>
				</ul>
<?php endforeach; ?>
		</ul>
	<li><object type="text/sitemap">
		<param name="Name" value="其它文档">
		</object>
		<ul>
<?php foreach ($others as $name => $title): ?>
			<li><object type="text/sitemap">
				<param name="Name" value="<?php echo $title; ?>">
				<param name="Local" value="<?php echo $name; ?>.html">
				</object>
<?php endforeach; ?>
		</ul>
</ul>
</body>
</html>
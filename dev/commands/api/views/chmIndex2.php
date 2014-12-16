<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
<head>
<meta name="GENERATOR" content="xunserch http://www.xunsearch.com">
</head>
<body>
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
	<li><object type="text/sitemap">
		<param name="Name" value="API 指南">
		<param name="Local" value="api/index.html">
		</object>
		<ul>
<?php foreach($this->classes as $name=>$class): ?>
			<li><object type="text/sitemap">
				<param name="Name" value="<?php echo $class->name; ?>">
				<param name="Local" value="api/<?php echo $class->name; ?>.html">
				</object>
				<ul>
<?php foreach($class->properties as $property): ?>
<?php if(!$property->isInherited): ?>
					<li><object type="text/sitemap">
						<param name="Name" value="<?php echo $property->name; ?>">
						<param name="Local" value="api/<?php echo $class->name . '.html#' . $property->name; ?>">
						</object>
<?php endif; ?>
<?php endforeach; ?>
<?php foreach($class->events as $event): ?>
<?php if(!$event->isInherited): ?>
					<li><object type="text/sitemap">
						<param name="Name" value="<?php echo $event->name; ?>">
						<param name="Local" value="api/<?php echo $class->name . '.html#' . $event->name; ?>">
						</object>
<?php endif; ?>
<?php endforeach; ?>
<?php foreach($class->methods as $method): ?>
<?php if(!$method->isInherited): ?>
					<li><object type="text/sitemap">
						<param name="Name" value="<?php echo $method->name; ?>()">
						<param name="Local" value="api/<?php echo $class->name . '.html#' . $method->name; ?>">
						</object>
<?php endif; ?>
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

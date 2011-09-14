<?php if (!isset($name)): ?>
<h1>PHP-SDK 文档列表</h1>
<ul>
<?php foreach ($files as $file): ?>
<li><?php echo CHtml::link($file, array('doc/' . $file)); ?></li>
<?php endforeach; ?>
<li><?php echo CHtml::link('GUIDE-BOOK', array('guide/toc')); ?></li>
</ul>
<?php else: ?>
<div class="markdown">
<?php $this->beginWidget('application.vendors.MarkDown', array('purifyOutput' => false)); ?>
<?php if (!file_exists('../doc/' . $name)): ?>
文档 **<?php echo $name; ?>** 正在紧张编写中 ...
<?php else: ?>
<?php readfile('../doc/' . $name); ?>
<?php endif; ?>
<?php $this->endWidget(); ?>
</div>
<?php endif; ?>
<?php if (isset($nav) && count($nav) > 0): ?>
<div id="guide-nav">
	<?php if (isset($nav['prev'])): ?>
	<div class="prev"><a href="<?php echo $nav['prev'][1]; ?>" title="上一章节">&laquo; <?php echo $nav['prev'][0]; ?></a></div>
	<?php endif; ?>
	<?php if (isset($nav['next'])): ?>
	<div class="next"><a href="<?php echo $nav['next'][1]; ?>" title="下一章节"><?php echo $nav['next'][0]; ?> &raquo;</a></div>
	<?php endif; ?>
</div>
<?php endif; ?>
<div class="clear"></div>

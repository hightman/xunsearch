<h1>PHP-SDK 权威指南</h1>

<div class="toc">
	<?php $this->widget('zii.widgets.CMenu', array('items' => $toc['items'])); ?>
</div>
<div class="markdown">
	<?php $this->beginWidget('application.vendors.MarkDown'); ?>
	<?php if (!file_exists('../doc/guide/' . $name . '.txt')): ?>
	文档 **<?php echo $name; ?>** 正在紧张编写中 ...
	<?php else: ?>
	<?php readfile('../doc/guide/' . $name . '.txt'); ?>
	<?php endif; ?>
	<?php $this->endWidget(); ?>
	<div id="guide-nav">
		<?php if (isset($toc['prev'])): ?>
		<div class="prev"><?php echo CHtml::link('&laquo; ' . $toc['prev']['label'], $toc['prev']['url']); ?></div>
		<?php endif; ?>
		<?php if (isset($toc['next'])): ?>
		<div class="next"><?php echo CHtml::link($toc['next']['label'] . ' &raquo;', $toc['next']['url']); ?></div>
		<?php endif; ?>
	</div>	
</div>
<div class="clear"></div>


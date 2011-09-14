<?php if(!$class->nativePropertyCount) return; ?>
<h2>事件明细</h2>
<?php foreach($class->events as $event): ?>
<?php if($event->isInherited) continue; ?>
<div class="detailHeader" id="<?php echo $event->name.'-detail'; ?>">
<?php echo $event->name; ?>
<span class="detailHeaderTag">
事件
<?php if(!empty($event->since)): ?>
(自版本 v<?php echo $event->since; ?> 起可用)
<?php endif; ?>
</span>
</div>

<div class="signature">
<?php echo $event->trigger->signature; ?>
</div>

<p><?php echo $event->description; ?></p>

<?php $this->renderPartial('seeAlso',array('object'=>$event)); ?>

<?php endforeach; ?>

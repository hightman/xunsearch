<?php if(!$class->nativePropertyCount) return; ?>
<h2>属性明细</h2>
<?php foreach($class->properties as $property): ?>
<?php if($property->isInherited) continue; ?>
<div class="detailHeader" id="<?php echo $property->name.'-detail'; ?>">
<?php echo $property->name; ?>
<span class="detailHeaderTag">
属性
<?php if($property->readOnly) echo ' <em>只读</em> '; ?>
<?php if(!empty($property->since)): ?>
 (自版本 v<?php echo $property->since; ?> 起可用)
<?php endif; ?>
</span>
</div>

<div class="signature">
<?php echo $this->renderPropertySignature($property); ?>
</div>

<p><?php echo $property->description; ?></p>

<?php $this->renderPartial('seeAlso',array('object'=>$property)); ?>

<?php endforeach; ?>

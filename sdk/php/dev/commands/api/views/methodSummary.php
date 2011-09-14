<?php if($protected && !$class->protectedMethodCount || !$protected && !$class->publicMethodCount) return; ?>

<div class="summary docMethod">
<h2><?php echo $protected ? 'Protected 方法' : 'Public 方法'; ?></h2>

<p><a href="#" class="toggle">隐去继承来的方法</a></p>

<table class="summaryTable">
<colgroup>
	<col class="col-method" />
	<col class="col-description" />
	<col class="col-defined" />
</colgroup>
<tr>
  <th>名称</th><th>描述</th><th>定义于</th>
</tr>
<?php foreach($class->methods as $method): ?>
<?php if($protected && $method->isProtected || !$protected && !$method->isProtected): ?>
<?php $methodAnchor=$this->fixMethodAnchor($method->definedBy,$method->name); ?>
<tr<?php echo $method->isInherited?' class="inherited"':''; ?> id="<?php echo $methodAnchor; ?>">
  <td><?php echo $this->renderSubjectUrl($method->definedBy,$methodAnchor,$method->name.'()'); ?></td>
  <td><?php echo $method->introduction; ?></td>
  <td><?php echo $this->renderTypeUrl($method->definedBy); ?></td>
</tr>
<?php endif; ?>
<?php endforeach; ?>
</table>
</div>

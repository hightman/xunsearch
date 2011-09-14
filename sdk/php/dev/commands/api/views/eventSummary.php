<?php if(empty($class->events)) return; ?>

<div class="summary docEvent">
<h2>事件</h2>

<p><a href="#" class="toggle">隐去继承来的事件</a></p>

<table class="summaryTable">
<colgroup>
	<col class="col-event" />
	<col class="col-description" />
	<col class="col-defined" />
</colgroup>
<tr>
  <th>名称</th><th>描述</th><th>定义于</th>
</tr>
<?php foreach($class->events as $event): ?>
<tr<?php echo $event->isInherited?' class="inherited"':''; ?> id="<?php echo $event->name; ?>">
  <td><?php echo $this->renderSubjectUrl($event->definedBy,$event->name); ?></td>
  <td><?php echo $event->introduction; ?></td>
  <td><?php echo $this->renderTypeUrl($event->definedBy); ?></td>
</tr>
<?php endforeach; ?>
</table>
</div>

<h1>API 参考文档</h1>

<table class="summaryTable docIndex">
<colgroup>
	<col class="col-package" />
	<col class="col-class" />
	<col class="col-description" />
</colgroup>
<tr>
  <th>包</th><th>类对象</th><th>描述</th>
</tr>
<?php foreach($this->packages as $package=>$classes): ?>
<?php foreach($classes as $i=>$class): ?>
<tr>
<?php if(!$i): ?>
  <td rowspan="<?php echo count($classes); ?>"><?php echo '<a name="'.$package.'"></a>' . $package; ?></td>
<?php endif; ?>
  <td><?php echo '{{'.$class.'|'.$class.'}}'; ?></td>
  <td><?php echo $this->classes[$class]->introduction; ?></td>
</tr>
<?php endforeach; ?>
<?php endforeach; ?>
</table>

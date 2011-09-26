<div class="sourceCode">
<b>源码:</b> <?php echo $this->renderSourceLink($object->sourcePath,$object->startLine); ?> (<b><a href="#" class="show">显示</a></b>)
<div class="code"><?php echo $this->highlight($this->getSourceCode($object)); ?></div>
</div>

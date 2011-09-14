使用索引缓冲区
============

前面分开讲解了文档的添加、删除、修改操作，它们的调用都是立即传送到服务器的。
如果您进行文档批量处理，如果每一次都进行服务器交互则显得效率低下。

因此，对于大量索引更新操作时，请使用以下方式开启缓冲区进行操作，缓冲区的默认
大小为 `4MB`，如需修改请传入数字作为 [XSIndex::openBuffer] 的参数。

~~~
[php]
$index->openBuffer(); // 开启缓冲区，默认 4MB，如 $index->openBuffer(8) 则表示 8MB

// 在此进行批量的文档添加、修改、删除操作
...
$index->add($doc);
...
$index->del($doc);
...
$index->update($doc);
...

$index->closeBuffer(); // 关闭缓冲区，必须和 openBuffer 成对使用
~~~

<div class="revision">$Id$</div>


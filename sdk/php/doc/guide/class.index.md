XSIndex 索引管理
===============

通过 [XSIndex] 对象实现索引数据库的增、删、改等相关操作。实现使用过程中并不需要
自行创建该对象，而是直接访问 [XS::index] 即可。

~~~
[php]
require '$prefix/sdk/php/lib/XS.php';
$xs = new XS('demo');
$index = $xs->index; // 索引对象来自 XS 的属性
~~~

关于索引对象的详细用法剖析请阅读后面的专题。

<div class="revision">$Id$</div>

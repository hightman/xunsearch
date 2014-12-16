XSSearch 搜索对象
================

通过 [XSSearch] 对象实现各种搜索相关操作。实现使用过程中并不需要自行创建该对象，
而是直接访问 [XS::search] 即可。

~~~
[php]
require '$prefix/sdk/php/lib/XS.php';
$xs = new XS('demo');
$search = $xs->search; // 搜索对象来自 XS 的属性
~~~

默认情况，搜索对象操作时对于用户输入的参数、搜索结果的输出编码视为默认编码，即 [XS::defaultCharset]。
如果您需要指定不同编码，请调用 [XSSearch::setCharset] 进行设置。

~~~
[php]
$search->setCharset('gbk');
~~~

关于搜索对象的详细用法剖析请阅读后面的专题。

<div class="revision">$Id$</div>

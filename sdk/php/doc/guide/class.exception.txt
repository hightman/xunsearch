XSException 异常
================

所有操作中出现的异常、错误均抛出此类型的异常，搜索代码中应该尝试捕捉该异常以确定操作是否成功。

已通过重载魔术方法 [XSException::__toString] 对出错信息作了简要的美化和修饰，您可以直接输出该对象，
如需要查看堆栈追踪，通过继承来的 [XSException::getTraceAsString] 即可。

> note: 在系统内部将所有的错误处理也转换成为抛出 [XSErrorException] 对象，而 `XSErrorException`
> 继承自 [XSException]，所以实际代码中只要统一捕捉 `XSException` 即可。

一段典型的处理代码应类似下面的方式：

~~~
[php]
require '$prefix/sdk/php/lib/XS.php';
try
{
    $xs = new XS('demo');
    $docs = $xs->search->setQuery('hightman')->setLimit(5)->search();
    foreach ($docs as $doc)
    {
       echo $doc->rank() . ". " . $doc->subject . " [" . $doc->percent() . "%]\n";
       echo $doc->message . "\n";
    }
}
catch (XSException $e)
{
    echo $e;               // 直接输出异常描述
    if (defined('DEBUG'))  // 如果是 DEBUG 模式，则输出堆栈情况
        echo "\n" . $e->getTraceAsString() . "\n";
}
~~~


<div class="revision">$Id$</div>

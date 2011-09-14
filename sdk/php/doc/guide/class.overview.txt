基础对象概述
===========

`Xunsearch PHP-SDK` 全面采用面向对象（`OOP`）的开发方式。
本章节将简单介绍开发过程中要接触到的几个重要对象（类）。

* [XS] 搜索项目的总对象，所有操作均基于此对象或其属性。
* [XSException] 所有操作中出现的异常、错误均抛出此类型的异常，搜索代码中应该尝试捕捉该异常以确定操作是否成功。
* [XSDocument] 文档用于描述检索/索引的基础对象，包含一组字段及其值，相当于常规SQL数据表中的一行记录。
* [XSIndex] 提供索引添加/删除/修改功能，通常以 `XS` 对象的属性方式出现，参见 [XS::index]。
* [XSSearch] 提供各种搜索功能，通常以 `XS` 对象的属性方式出现，参见 [XS::search]。
* [XSTokenizer] 自定义字段词法分析器接口。

类对象中的魔术属性
----------------

通过 PHP 对象中的 __get 和 __set 技巧，我们针对所有 [XSComponent] 的子类实现了对象的模拟属性。
这类读取或写入属性值时实际上是隐含调用了相应的 getter/setter 方法，这类属性不区分大小写。

~~~
[php]
$a = $obj->text; // $a 值等于 $obj->getText() 的返回值
$obj->text = $a; // 等同事调用 $obj->setText($a)
~~~

支持这类属性的对象主要包括以下几个（不全，仅挑重要的列出）：

- [XS::index] 项目索引对象：$xs->index
- [XS::search] 项目搜索对象：$xs->search 
- [XS::defaultCharset] 项目默认字符集：$xs->defaultCharset
- [XSSearch::dbTotal] 搜索数据库内的数据总量：$xs->search->dbTotal
- [XSSearch::lastCount] 最近一次搜索的结果匹配总数估算值：$xs->search->lastCount
- [XSSearch::query] 搜索语句：$xs->search->query

<div class="revision">$Id$</div>

获取搜索匹配数量
===============

出于性能考虑，搜索结果的匹配数量均被设计为估算值，并非准确值。


索引库内的数据总数
----------------

这个数量是真实准确的，并非估算，获取总量有以下两种做法，至于喜欢用哪种您自己决定。

* 调用方法 [XSSearch::getDbTotal]
* 读取魔术属性 [XSSearch::dbTotal]，内部也是调用上述方法 

~~~
[php]
$total = $search->dbTotal;
$total = $search->getDbTotal();
~~~


最近一次搜索的匹配数
------------------

通过 [XSSearch::getLastCount] 或 [XSSearch::lastCount] 可以快速获取到最近一次调用
[XSSearch::search] 进行搜索时得到的匹配数量。

除非您对搜索结果完全不关心，否则强烈建议用这种方法获取匹配数目以减少系统开销。

~~~
[php]
// 假设曾经有一次 search 调用
$docs = $search->setQuery('测试')->setLimit(5)->search();

// 必须在 search() 之后调用
$count = $search->lastCount;
$count = $search->getLastCount();
~~~


直接获取搜索匹配数
----------------

通过 [XSSearch::count] 调用可以直接获取搜索的匹配数量，该方法和 [XSSearch::search] 
类似，在调用前可以做一系列的搜索查询语句构建，也可以直接接受查询语句做参数。

返回值是一个整型数字，是估算值。

~~~
[php]

// 先设置 Query 再获取数量
$count = $search->setQuery('神雕侠侣 -电视剧')->count();

// 直接把 Query 语句传入
$count = $search->count('杭州 西湖'); 
~~~

> tip: 对于不带参数的 `count` 调用建议放在 `search` 之后，内部会进行优化，减少一次查询。


<div class="revision">$Id$</div>

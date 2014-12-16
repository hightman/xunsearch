获取相关搜索词
=============

相关搜索通常是作为当前搜索词的搜索建议出现在搜索结果的下方，引导用户发现其它
更具体、更符合要求的关键词。

相关搜索词使用 [XSSearch::getRelatedQuery] 方法，返回值是搜索词组成的数组。
接受 2 个可选参数如下：

  * `$query` 搜索语句，返回跟这个搜索语句相关的搜索词，默认为 NULL 使用最近那次 `setQuery` 的语句
  * `$limit` 整数值，设置要返回的词数量上限，默认为 6，最大值为 20

~~~
[php]
$search->setQuery('西湖');
// 获取前 6 个和默认搜索语句 "西湖" 相关搜索词
$words = $search->getRelatedQuery();

// 获取 10 个和 "杭州" 相关的搜索词
$words = $search->getRelatedQuery('杭州', 10);
~~~

> note: 获取相关搜索内部会重置 [XSSearch::query] ，建议放在搜索的最后调用。

<div class="revision">$Id$</div>

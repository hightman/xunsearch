搜索建议和纠错
=============

这项功能也是建立在搜索日志的基础上。


拼写纠错
--------

对于英文，由于打字速度过快或各种原因都很容易造成一两个字母出错。对于中文来说，古代就动不动
出现同音”通假字“，绝大多数现代人也使用拼音输入法，加上方言口音，乱用同音字现象非常普遍。

所以系统在综合分析索引库内的词汇、用户搜索日志基础上建立了一个庞大的纠错体系，支持英文拼写
纠错、中文同音字纠错、拼音转换等。

使用这些功能都非常简单，直接调用 [XSSearch::getCorrectedQuery] 即可，该方法接受搜索语句
作为参数，如省略参数则直接使用最近一次 `setQuery` 的语句，返回值是修正词汇组成的数组，若
没有更合适的修正方案则返回空数组。

~~~
[php]

// 假设在本意是在 demo 项目中搜索 "测试"，但不小心打成了 "侧试"
$search->setQuery('侧试');
$docs = $search->search();  

// 由于拼写错误，这种情况返回的数据量可能极少甚至没有，因此调用下面方法试图进行修正
$corrected = $search->getCorrectedQuery();
if (count($corrected) !== 0)
{
   // 有纠错建议，列出来看看；此情况就会得到 "测试" 这一建议
   echo "您是不是要找：\n";
   foreach ($corrected as $word)
   {
      echo $word . "\n";
   }
}

/** 
 * 以下拼写示例则简化，并直接传入 Query 语句进行测试 
 * 您也可以例句用 `util/Quest.php demo --correct <word>` 进行测试
 */

$search->getCorrectedQuery('cs');  // 通过声母缩写得到建议: "测试"
$search->getCorrectedQuery('ceshi');  // 通过全拼缩写得到建议: "测试"
$search->getCorrectedQuery('yunsearch'); // 通过拼写纠错得到: xunsearch
$search->getCorrectedQuery('xunseach 侧试'); // 混合纠错得到: xunsearch测试

~~~

> tip: 建议在搜索结果数量过少或没有时再尝试进行拼写纠错，而不是每次搜索都进行。


搜索建议
--------

搜索建议是指类似百度那样，当用户在搜索框输入少量的字、拼音、声母时提示用户一些相关的
热门关键词列表下拉框供用户选择。

这样做非常有利于节省用户的打字时间、提升用户体验。

我们通过 [XSSearch::getExpandedQuery] 来读取展开的搜索词，该方法返回展开的搜索词组成的
数组，如果没有任何可用词则返回空数组。接受 2 个参数：

  * `$query` 要展开的搜索词，返回结果是以这个搜索词为前缀、拼音前缀展开，此为必要参数
  * `$limit` 整数值，设置要返回的词数量上限，默认为 10，最大值为 20

~~~
[php]
/**
 * 以下例子也可以用 `util/Quest.php demo --suggest <word>` 进行测试
 */
$search->getExpandedQuery('x'); // 返回：项目, xunsearch, 行为, 项目测试
$search->getExpandedQuery('xm'); // 返回：项目, 项目测试
$search->getExpandedQuery('项'); // 返回：项目, 项目测试
$search->getExpandedQuery('项目'); // 返回：项目测试
~~~

> tip: 实际使用过程中，搜索建议通常单独设计一个入口脚本，再在主搜索界面的搜索框中通过
> `ajax、AutoComplete` 等 `JavaScript` 技术来根据用户的输入动态载入建议词列表。


<div class="revision">$Id$</div>

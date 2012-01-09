同义词搜索
=========

人类语言文化丰富，同一个意思有无数种不同的表达方式。因此，**同义词**
是全文检索中非常重要和必要的一项基础功能。从 `1.3.0` 版本起，`Xunsearch` 
开始提供同义搜索搜索功能。

什么是同义词搜索
---------------

为了更好的提升用户搜索体验，我们希望用户在搜索“浙江大学”的时候也同时匹配包含缩写“浙大”的结果，
这就是所谓的同义词搜索。通常我们说“浙大”是“浙江大学”的同义词，前者是常用缩写，后者是标准词（原词）。

**同义词搜索** 完全依赖于您事先设置好的同义词库，词库内每条记录由“标准词（原词）”和“同义词”组成，
它们都必须是独立的词汇，也就是最小的索引单位，不可以是多个词组成的短语。

> tip: 独立词汇的意思对于英文来说就是一个单词，对于中文来说必须是 `scws` 词库中的一个词。  
> _Xunsearch_ 的同义词不同于 _Xapian_，会智能进行字段匹配和转换，您只需维护通用词库。


英文同义词的特殊处理
------------------

英文单词的同义词在 xunsearch 中做了一些个特殊处理：

- **统一小写** 统一转换为小写字母进行存储，不支持必须保留大写字母的同义词。

- **同根词支持** 英语同一个单词有多种不同的形式，如：单复数、过去式和现在分词等。
  而它们要传送的其实是同一种涵义。因此，对于纯英文的同义词记录会自动进行词根处理。
  词根记录保存为大写字母 Z 开头的记录。例如：

  设置 `find` 是 _search_ 的同义词，那么检索 _searching_ 时如果 _searching_ 
  本身没有同义词，那么也会匹配包含 `finding` 或 `finds` 等同根词的结果。

- **复合词支持** 支持对多个单词组成的短语设置同义词，词之间用一个半角空格分开，
  但同义词仍然只允许一个单词或中文词哦。如：

  设置 `您好` 为 _hello world_ 的同义词，那么检索 _hello world ok_ 
  时也会匹配同时包含 `您好` 和 `ok` 的记录。


管理同义词库
------------

您可以通过 [XSIndex::addSynonym] 和 [XSIndex::delSynonym] 来添加和删除同义词记录。
多次调用这两个接口函数的时候支持使用[索引缓冲区](index.buffer)来提高效率。

参数 `$raw` 是字符串，表示记录中的原词（标准词），`$synonym` 也是字符串，表示记录中的同义词，
对于删除操作可以省略该参数表示清空原词的所有同义词。

以下为示例代码，其中的 `$index` 变量是索引操作对像，参见[如何开始使用索引?](index.overview#ch1)

~~~
[php]
// 给 "搜索" 增加 "检索" "查找" 两个同义词
$index->addSynonym('搜索', '检索');
$index->addSynonym('搜索', '查找');

// 给 "Hello world" 增加同义词 "你好"
$index->addSynonym('hello world', '你好');

// 删除 search 的全部同义词
$index->delSynonym('search');

// 删除 "搜索" 的同义词 "检索"
$index->delSynonym('搜索', '检索');
~~~

除了调用 `API`　外，您还可以使用 [Indexer 索引管理工具](util.indexer#ch8) 进行同义词管理。


查看已有同义词库
---------------

通过 [XSSearch::getAllSynonyms] 来获取当前索引库内的全部同义词记录，返回一个数组，
以原词（标准词）为键，其同义词列表为值。

以下为示例代码，其中的 `$search` 变量是搜索操作对像，参见[如何开始使用搜索?](search.overview#ch1)

~~~
[php]
// 获取当前库的前 100 个同义词记录
$synonyms = $search->getAllSynonyms();

// 获取当前库第 6~15 条同义词记录
$synonyms = $search->getAllSynonyms(10, 5);

// 查看包含隐藏同义词根在内的前 20 条记录
$synonyms = $search->getAllSynonyms(20, 0, true);
~~~

除了调用 `API`　外，您还可以使用 [Quest 搜索工具](util.quest#ch2) 进行同义词管理。


使用同义词搜索功能
-----------------

同义词搜索默认是不开启的，如果您打算使用同义词搜索，这部分功能隶属于[构建搜索语句](search.query)。

在设置查询语句 [XSSearch::setQuery] 之前调用 [XSSearch::setAutoSynonyms] 来开启同义词功能。

~~~
[php]
// 假设”搜索“有且仅有一个同义词”检索“
// 开启同义词搜索，输出：
// Xapian::Query(((搜索:(pos=1) SYNONYM 检索:(pos=89)) AND 世界:(pos=2)))
$search->setAutoSynonyms()->setQuery('搜索世界')->getQuery();

// 开启同义词搜索，并带有字段效果，假设 subject 是项目的第二个字段，输出：
// Xapian::Query(((B搜索:(pos=1) SYNONYM B检索:(pos=89)) AND B世界:(pos=2)))
$search->setAutoSynonyms()->setQuery('subject:搜索世界')->getQuery();
 
// 关闭同义词搜索，输出：
// Xapian::Query((搜索:(pos=1) AND 世界:(pos=2)))
$search->setAutoSynonyms(false)->setQuery('搜索世界')->getQuery();
~~~

> tip: 您可以分别在开启/关闭同义词功能的条件下，对比 [XSSearch::getQuery] 查询语句分析结果。


<div class="revision">$Id$</div>

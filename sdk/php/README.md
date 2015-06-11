Xunsearch SDK for PHP
=====================
$Id$

这是采用 PHP 语言编写的 xunsearch 开发包，在此基础上开发您自己的全文检索。

在此简要介绍以下几个文件：

    - lib/XS.php             入口文件，所有搜索功能必须包含此文件
    - lib/XS.class.php       未合并带注释的入口文件，会自动加载其它 .class.php 文件
    - util/RequireCheck.php  命令行运行，用于检测您的 PHP 环境是否符合运行条件
    - util/IniWizzaard.php   命令行运行，用于帮助您编写 xunsearch 项目配置文件
    - util/Quest.php         命令行运行，搜索测试工具
    - util/Indexer.php       命令行运行，索引管理工具
    - util/SearchSkel.php    命令行运行，根据配置文件生成搜索骨架代码
    - util/xs                命令行工具统一入口

在开始编写您的代码前强烈建议执行 util/RequireCheck.php 以检查环境。

具体各项文档内容请参阅子目录： doc/ 
强烈推荐在线阅读我们的文档：<http://www.xunsearch.com/doc/>

最简单使用方法就是下载全部源码压缩包，然后引入入口文件即可。
出现异常则抛出 \XSException 异常实例。


Composer 支持
--------------

自 v1.4.9 起，通过 subtree 功能将 xs-sdk-php 分离成为一个只读的子仓库，
以支持通过 [composer](https://getcomposer.org) 安装。我们的包名称为
`hightman/xunsearch`，内含全部 PHP-SDK 的库文件，还包括支持 Yii 的扩展类。


### 安装

和绝大多数 composer 一样，你可以通过以下两种方法中的任意一种安装。

可以直接运行

```
composer require --prefer-dist hightman/xunsearch "*@beta"
```

或者将以下内容添加到您的项目根目录 `composer.json` 中的 `require` 章节，
然后运行 `composer install`

```
"hightman/xunsearch": "*@beta"
```

> tip: 如果您打算用代码仓库中的最新版本，请将星号改为 dev-master。


### 命令行工具

```
vendor/bin/xs help
```

### 普通用法

这和 xunsearch 指南中的用法完全一致，只不过引入开始文件的方法略有不同。
官方用法指南详见 <http://www.xunsearch.com/doc/php>

```php
// 加载 vendor 的 autoload 文件
require_once 'vendor/autoload.php';

// 默认的 xunsearch 应用配置文件目录为 vendor/hightman/xunsearch/app
// 如有必要，请通过常量 XS_APP_ROOT 定义
define ('XS_APP_ROOT', '/path/to/ini')

// 创建 XS 对象，关于项目配置文件请参见官网
$xs = new \XS('demo');

// 后面的代码就和官网上的指南一致了
```

### Yii-1.x 用法

这是对 xunsearch 的一个简单封装，使之更适合 yii-1.x 的用法习惯。首先，
在应用入口文件最开头引入 composer 的 autoload 文件，通常是 index.php。

```php
require_once 'vendor/autoload.php';
// 如有必要请定义常量 XS_APP_ROOT 表示项目文件存放路径
//define ('XS_APP_ROOT', dirname(__FILE__) . '/protected/data');
```

在应用配置文件的 `compnents` 中添加以下代码，通常是 protected/config/main.php

```php
    // application components
    'components' => array(
        // ... other components ... 
        'search' => array(
            'class' => 'EXunSearch',
            'project' => 'demo', // 搜索项目名称或对应的 ini 文件路径
            'charset' => 'utf-8', // 您当前使用的字符集（索引、搜索结果）
        ),  
    ),  
```

然后就可以通过 `Yii::app()->search` 来访问 `EXunSearch` 对象，进行索引管理或检索。

添加、修改索引数据，使用方法参照 [XSIndex][2]。
对于 ActiveRecord 对象来讲，建议在相关的 `afterSave` 和 `afterDelete` 中进行索引同步。

```php
$data = array('pid' => 1234, 'subject' => '标题', 'message' => '内容');
Yii::app()->search->add($data);	// 添加文档
Yii::app()->search->update($data);	// 更新文档
Yii::app()->search->del('1234');	// 删除文档
```

使用检索功能时，可以将 `Yii::app()->search` 当作 [XSSearch][3] 对象一样直接使用它的全部方法。

```php
Yii::app()->search->setQuery('subject:标题');
$docs = Yii::app()->search->setLimit(5, 10)->search();	// 取得搜索结果文档集
```

### Yii-2.x 用法

在 yii2 中，除了提供类似 yii-1.x 的调用方式外，我们还支持 ActiveRecord 方式来操作。首先，
请在应用配置文件的 `components` 中添加以下代码，通常是 `config/web.php`

```php
	// application components
	'components => [
		// ... other components ...
		'xunsearch' => [
			'class' => 'hightman\xunsearch\Connection',	// 此行必须
			'iniDirectory' => '@app/config',	// 搜索 ini 文件目录，默认：@vendor/hightman/xunsearch/app
			'charset' => 'utf-8',	// 指定项目使用的默认编码，默认即时 utf-8，可不指定
		],
	],
```

接下来，你可以通过以下代码获取到 `hightman\xunsearch\Database` 对象，该对像和 yii-1.x 的
`EXunSearch` 用法很相似，通过魔术方法，能够依次检索以下对象的方法列表而直接调用：

```php
$db = \Yii::$app->xunsearch->getDatabase('demo');
$db = (\Yii::$app->xunsearch)('demo');
$xs = $db->xs;
$search = $db->getSearch();
$index = $db->getIndex();
```

- [XS][1] 优先调用该对象方法，如有必要，可直接通过 `hightman\xunsearch\Database::$xs` 属性访问。
- [XSIndex][2] 紧接着检查索引管理方法，如有必要，可直接通过 `hightman\xunsearch\Database::$index` 属性访问。
- [XSSearch][3] 紧接着检查索引管理方法，如有必要，可直接通过 `hightman\xunsearch\Database::$search` 属性访问。

具体用法不再赘述，下面重点讲讲如何通过 ActiveRecord 方法来检索和创建索引，由于遵循 yii2 的思想进行开发设计，
使用起来非常方便和简单。

#### 创建 AR 对象
首先必须创建一个继承自 `hightman\xunsearch\ActiveRecord` 的模型类，默认情况下会以全小写的类名字作为
ini 文件名。如需指定，请自行覆盖编写 `hightman\xunsearch\ActiveRecord::projectName()`。通常代码如下：

```php
class Demo extends \hightman\xunsearch\ActiveRecord
{
    /*public static function projectName() {
        return 'another_name';	// 这将使用 @app/config/another_name.ini 作为项目名
    }*/
}
```

由此可见，如果命名规范模型类几乎不需要任何额外代码，上述代码会自动采用 `demo.ini` 并自动装载字段配置。

#### 添加或更新索引

为避免数据重复，底层统一通过 `XSIndex::update()` 方法进行提交的。

```php
// 添加索引，也可以通过 $model->setAttributes([...]) 批量赋值
$model = new Demo;
$model->pid = 321;
$model->subject = 'hello world';
$model->message = 'just for testing...';
$model->save();

// 更新索引
$model = Demo::findOne(321);
$model->message .= ' + updated';
$model->save();


// 添加或更新索引还支持以方法添加索引词或文本
// 这样做的目的是使得可以通过这些关键词检索到数据，但并非数据的字段值
// 用法与 XSDocument::addTerm() 和 XSDocument::addIndex() 等同
// 通常在 ActiveRecord::beforeSave() 中做这些操作
$model->addTerm('subject', 'hi');
$model->addIndex('subject', '你好，世界');

// 如需删除数据则可直接
$model->delete();

```

如需要做批量删除或更新，请参见以下代码文档：`ActiveRecord::updateAll()` 和 `ActiveRecord::deleteAll()`。

#### 检索对象

重点先介绍一下 `ActiveQuery::where()` 系列搜索条件函数的用法，和 yii2 其它的 ActiveRecord 类似：

```php
$query = Demo::find(); // 返回 ActiveQuery 对象
$condition = 'hello world';	// 字符串原样保持，可包含 subject:xxx 这种形式
$condition = ['WILD', 'key1', 'key2' ... ];	// 通过空格将多个查询条件连接
$condition = ['AND', 'key1', 'key2' ... ]; // 通过 AND 连接，转换为：key1 AND key2
$condition = ['OR', 'key1', 'key2' ... ]; // 通过 OR 连接
$condition = ['XOR', 'key1', 'key2' ... ]; // 通过  XOR 连接
$condition = ['NOT', 'key']; // 排除匹配 key 的结果
$condition = ['pid' => '123', 'subject' => 'hello']; // 转换为：pid:123 subject:hello
$condition = ['pid' => ['123', '456']]; // 相当于 IN，转换为：pid:123 OR pid:456
$condition = ['IN', 'pid', ['123', '456']]; // 转换结果同上
$condition = ['NOT IN', 'pid', ['123', '456']]; // 转换为：NOT (pid:123 OR pid:456)
$condition = ['BETWEEN', 'chrono', 14918161631, 15918161631]; // 相当于 XSSearch::addRange(...)
$condition = ['WEIGHT', 'subject', 'hello', 0.5]; // 相当于额外调用 XSSearch::addWeight('subject', 'hello', 0.5);
$query->where($condition);
```

对于 `hightman\xunsearch\ActiveQuery` 对象，主要支持以下几个方法获取和操作：

- **asArray()**: 以数组形式返回数据
- **one()**: 返回一行数据
- **all()**: 返回全部数据
- **count()**: 统计数据匹配数据，是估算的并不是完全准确
- **exists()**: 判断查询条件是否存在数据
- **where()**: 指定搜索条件
- **orderBy()**: 指定排序方式，默认为相关性排序
- **limit()**, **offfset()**: 指定获取数据量和偏移，用于分页检索
- **with()**, **indexBy** ...
- **buildOther(function(\XSSearch $search){})** 可通过此方法定制检索选项

此外，ActiveQuery 还提供了一个名为 `beforeSearch` 的事件，可在执行搜索前再次对 `ActiveQuery::getSearch()`
所返回的 `XSSearch` 对象进行调整。


如果以 AR 对象获得数据，可通过以下几个方法获取搜索结果元数据，参照 `XSDocument` 相关用法。

```php
$model = Demo::findOne(321);
$model->docid(); //Xapian数据 ID
$model->rank(); //序号
$model->percent(); //匹配百分比
$model->ccount(); //折叠数量，须在 XSSearch::setCollapse() 指定后才有效
$model->matched(); //获得匹配词汇
```

ActiveRecord 对象实现了绝大多数据接口，完全可以像使用普通数据库模型一样使用它。如果需要
访问原始的 xunsearch 对象，请通过以下方式获取 `Database` 对象：

```php
$db = Demo::getDb();
$search = $db->getSearch();
$index = $db->getIndex();
// 如有必要，还可以获得 scws 分词对象
$scws = $db->getScws();
```

#### 使用 xunsearch DebugPanel

为便于调试，还提供了一个 `hightman\xunsearch\DebugPanel` 对象，可以集成到 debug 模块中，
可在调试工具条和面板中显示 `xunsearch` 有关的查询以及耗时情况。

要想启用这个很容易，只要在主配置文件中加入以下代码：

```php
    // ...
    'bootstrap' => ['debug'],
    'modules' => [
        'debug' => [
            'class' => 'yii\\debug\\Module',
            'panels' => [
                'xunsearch' => [
                    'class' => 'hightman\\xunsearch\\DebugPanel',
                ],
            ],
        ],
    ],
    // ...
```


#### 其它用法

TBD. 如关联等，参见其它 AR 用法即可。

> note: 相关的 AR 索引操作均非实时的，如需实时更新索引，请通过 `Database::getIndex()->flushIndex()` 刷新。
> 关于查询日志有关的功能，也建议通过原生的 `XSSearch` 和 `XSIndex` 对象来操作。



[1]: http://www.xunsearch.com/doc/php/api/XS
[2]: http://www.xunsearch.com/doc/php/api/XSIndex
[3]: http://www.xunsearch.com/doc/php/api/XSSearch


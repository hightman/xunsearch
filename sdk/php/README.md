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


Composer 支持
--------------

自 v1.4.9 起，通过 subtree 功能将 xs-sdk-php 分离成为一个只读的子仓库，
以支持通过 [composer](https://getcomposer.org) 安装。我们的包名称为
`hightman/xunsearch`，内含全部 PHP-SDK 的库文件，还包括支持 Yii 的扩展类。


### 安装

和绝大多数 composer 一样，你可以通过以下两种方法中的任意一种安装。

可以直接运行

```
composer require --prefer-dist hightman/xunsearc "*"
```

或者将以下内容添加到您的项目根目录 `composer.json` 中的 `require` 章节，
然后运行 `composer install`

```
"hightman/xunsearch": "*"
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

添加、修改索引数据，使用方法参照 [XSIndex](http://www.xunsearch.com/doc/php/api/XSIndex)。
对于 ActiveRecord 对象来讲，建议在相关的 `afterSave` 和 `afterDelete` 中进行索引同步。

```php
$data = array('pid' => 1234, 'subject' => '标题', 'message' => '内容');
Yii::app()->search->add($data);	// 添加文档
Yii::app()->search->update($data);	// 更新文档
Yii::app()->search->del('1234');	// 删除文档
```

使用检索功能时，可以将 `Yii::app()->search` 当作 [XSSearch](http://www.xunsearch.com/doc/php/api/XSSearch)
对象一样直接使用它的全部方法。

```php
Yii::app()->search->setQuery('subject:标题');
$docs = Yii::app()->search->setLimit(5, 10)->search();	// 取得搜索结果文档集
```

### Yii-2.x 用法

TBD.


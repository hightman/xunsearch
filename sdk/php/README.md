Xunsearch SDK for PHP: 自述文件
================================
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
    - **util/xs**                命令行工具统一入口

在开始编写您的代码前强烈建议执行 util/RequireCheck.php 以检查环境。

具体各项文档内容请参阅子目录： doc/ 
强烈推荐在线阅读我们的文档：<http://www.xunsearch.com/doc/>

最简单使用方法就是下载全部源码压缩包，然后引入入口文件即可。


Composer 支持
--------------

自 v1.4.9 起，通过 subtree 功能将 xs-sdk-php 分离成为一个只读的子仓库，
以支持通过 [composer](https://getcomposer.org) 安装。

我们的包名称 `hightman/xunsearch`，内含全部 PHP-SDK 的库文件，还包括支持
Yii 的扩展类。


### 安装

和绝大多数 composer 一样，你可以通过以下两种方法中的任意一种安装。

直接运行

```
composer require --prefer-dist hightman/xunsearc ">1.4.8"
```

或者将以下内容添加到您的项目根目录 `composer.json` 中的 `require` 章节，
然后运行 `composer install`

```
"hightman/xunsearch": ">1.4.8"
```


### 普通用法

TBD.

### Yii-1.x 用法

TBD.

### Yii-2.x 用法

TBD.



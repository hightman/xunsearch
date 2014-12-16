Yii-Xunsearch 扩展
===================

[Yii](http://www.yiiframework.com) 是一个流行的高性能可扩展 PHP5 框架，
为便于在采用 `Yii` 开发的项目中使用 `Xunsearch` 特编写本扩展。

Requirements
--------------
Yii 1.1.1 or above
必须先安装 xunsearch ，参见 <http://www.xunsearch.com>

Description:
--------------
这个扩展是 xunsearch 全文检索的一个包装，使其在 Yii 中使用起来更加简单和符合习惯。

  - 通过魔术方法可以直接调用 XSSearch/XSIndex 中的绝大多数方法
  - 覆盖了 `XSIndex::add`, `XSIndex::update` 方法，以便可以直接使用数组来修改索引
  - 具体关于 xunsearch 的部分请参见其官方文档：<http://www.xunsearch.com/doc>

Usage:
---------------
这个扩展是一个 Application 组件，请先将下载或解压得到的 `EXunsearch.php`
拷入 Yii 应用的 `protected/extension/` 目录，然后在配置文件里的 `components`
部分添加以下代码：

```php
    // application components
    'components' => array(
        // ... other components ...
        'search' => array(
            'class' => 'ext.xunsarch.EXunSearch',
            'xsRoot' => '/Users/hightman/Projects/xunsearch',  // xunsearch 安装目录
            'project' => 'demo', // 搜索项目名称或对应的 ini 文件路径
            'charset' => 'utf-8', // 您当前使用的字符集（索引、搜索结果）
        ),
    ),
```

添加、修改索引数据，使用方法参照 [XSIndex](http://www.xunsearch.com/doc/php/api/XSIndex)，
对于 ActiveRecord 对象来说，强烈建议在相关的事件里添加代码。

```php
$data = array('pid' => 1234, 'subject' => '标题内容', 'message' => '内容文字');
Yii::app()->search->add($data); // 增加文档
Yii::app()->search->update($data); // 更新相同主键的文档
Yii::app()->search->del('1234'); // 删除主键为 1234 的文档
```

使用搜索时，可以将 Yii::app()->search 当作 [XSSearch](http://www.xunsearch.com/doc/php/api/XSSearch)
对象一样直接使用它的所有方法。

```php
Yii::app()->search->setQuery('subject:标题');
$docs = Yii::app()->search->setLimit(5, 10)->search(); // 取得搜索结果文档
```

ChangeLog:
---------------
Mar 6, 2013

  - 加入到 xunsearch 开发包中

Oct 16, 2012

  - 优化魔术方法访问方式

Sep 23, 2011

  - 第一个简要包装版本

NOTE:
---------------
这里的文档过于简单，建议使用时先通读并了解 [xunsearch](http://www.xunsearch.com) 项目。

Reporting Issue:
-----------------
欢迎提出任何意见、建议和指出我们的问题，请至[扩展主页](http://www.yiiframework.com/extension/xunsearch)留言或[发布到论坛](http://bbs.xunsearch.com/forumdisplay.php?fid=5)


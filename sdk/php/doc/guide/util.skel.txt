生成搜索骨架代码
===============

前面已经把 `Xunsearch PHP-SDK` 相关的基础文档全面讲了一通，您完全可以根据文档和
自动生成的 `API` 文档进行搜索开发了。

但根据我们研发 [Xungle 项目](http://www.xungle.com) 的实践经验，绝大多数搜索项目的前端代码都类似，
不同的均为细节显示、字段使用等。因此为了更快的加速用户开发速度，我们特地推出这个代码生成工具。

这个工具读取并分析项目配置文件(`*.ini`)，生成通用的搜索骨架代码，即使您不做任何修改，也能使用它并看到
基础的搜索效果、界面。强烈建议所有的搜索项目都在生成的骨架代码上进行二次开发，节省大量宝贵时间。

运行脚本工具的 --help 选项可查看内置的帮助和说明，输出文字编码强制为 UTF-8。

~~~
$prefix/sdk/php/util/SearchSkel.php --help
~~~

主要参数和选项
-------------

要使用本工具，必须先指定的项目名称或配置文件，它将根据该项目的情况生成代码，主要选项如下：

  * _-p|--project <name|file>_ 指定项目名称或配置文件路径，参数名可以省略不写，
    如果仅指定项目名称，那么将使用 $prefix/sdk/php/app/<name>.ini 文件。

  * _-o|--output <..dir..>_ 指定生成的代码目录的存放位置，默认放在当前目录中，
    生成的代码本身包含一层以项目名称命名的目录。建议直接将目录指定到 web 可访问目录。

经典用法示例
-----------

~~~
# 在当前目录下生成 demo 项目的搜索代码
util/SearchSkel.php demo

# 在指定的 /path/to/web 目录生成 demo 搜索代码，代码目录为：/path/to/web/demo
util/SearchSkel.php demo /path/to/web

# 使用选项指定方式
util/SearchSkel.php -p demo -o /path/to/web
~~~

生成的代码介绍
-------------

工具运行完毕后，立即生成以下三个文件于目录中，如果文件已经存在，则会把旧有文件保存为 
`.bak` 文件用于备份。

~~~
[demo]
  |- search.php     # 搜索功能入口
  |- search.tpl     # 搜索结果输出模板文件
  \- suggest.php    # 提取搜索输入框下拉建议，通过 autocomplete 组件自动调用
~~~

您可以直接通过浏览器访问：search.php 试用搜索。

> note: 生成的代码中关于搜索建议的部分，采用 jQuery-UI 的 autocomplete 并且从 Google 提供的代码库
> 直接加载，如果您的不能连网将可能无法看到部分效果。您可以将相关的 js/css 下载到本地并替换模板代码。

<div class="revision">$Id$</div>

Xunsearch 完整安装包
===================
$Id$

快速安装、升级
------------

1.  不管您是第一次安装 xunsearch ，还是升级现有的 xunsearch 均直接在 shell 
    环境运行以下指令，其后根据后续的提示操作即可：
    ~~~
    chmod +x ./setup.sh
    ./setup.sh
    ~~~

2.  如果安装顺利完成后，请至安装目录启动或重新启动 xunsearch 的后台服务程序，
    请将 $prefix 替换为您的安装目录再执行以下代码：
    ~~~
    cd $prefix ; bin/xs-ctl.sh restart
    ~~~
 
    强烈建议你将此命令添加到开机启动脚本中，以便每次服务器重启后能自动启动搜索
    服务程序，在 Linux 系统中您可以将脚本指令写进 /etc/rc.local 。

3.  关于数据目录，索引数据全部保存在安装目录的 data/ 子目录下，您可以根据自身
    的硬盘规划情况，将 data 作为软链接指向真实的目录。

4.  安装完毕后，请在我们提供的开发包 (SDK) 基础上开发您自己的搜索引擎，目前只
    提供了 PHP 的 devkit 包，具体文档如下：
    
    > $prefix/sdk/php/README
    > $prefix/sdk/php/doc/
    > 推荐在线阅读我们的文档：<http://www.xunsearch.com/doc/>


安全须知
-------

出于性能和多数需求考虑 xunsearch 服务端和 SDK API 通讯时没有加密和验证处理，
所以默认情况 xs-ctl.sh 启动的服务程序是绑定并监听在 127.0.0.1 上，如果您的
SDK 调用和 xunsearch 服务端不在同一服务器，请使用 -b inet 方式启动脚本，并
注意使用类似 iptables 的防火墙来控制 xunsearch 的 8383/8384 两个端口的访问
权限。
~~~
bin/xs-ctl.sh -b local start
bin/xs-ctl.sh -b inet start
bin/xs-ctl.sh -b a.b.c.d start
bin/xs-ctl.sh -b unix start     [tmp/indexd.sock, tmp/searchd.sock]
~~~


文件结构及说明
------------

完整安装包包含了所有 xunsearch 编译运行所需要的信赖库以及 SDK 代码，方便用户
一次下载、一次安装，免去先装各种依赖的麻烦。压缩包 xunsearch-full-latest.tar.gz 
解压后的目录结构如下：

xunsearch-full/
 |- README.md
 |- setup.sh
 \- packages/
     |- xunsearch-1.x.y.tar.gz
     |- libevent-1.x.y-stable.tar.gz
     |- xapian-core-scws-1.x.y.tar.gz
     |- scws-1.x.y.tar.bz2
     \- scws-dict-chs-utf8.tar.bz2

版权、授权协议
------------

Xunsearch 基于 GPL 许可证免费开源发布，附加要求是希望每一个使用 xunsearch 
构建的搜索引擎应用，在底部或其它位置保留 Powered by xunsearch 的宣告，同时
链接到我们的官网地址：<http://www.xunsearch.com> ，关于 xunsearch 详细授权
说明请参见源码目录下的 COPYING 文件。

软件包中包含的其它依赖或组件则保持其原有的方式授权，著作权也归原始作者所有。

- libevent     BSD 协议
- xapian-core  GPL 协议
- scws-1.2.x   BSD 协议


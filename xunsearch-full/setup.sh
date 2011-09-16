#!/bin/sh
# FULL fast install/upgrade script
# $Id$

# self check
if ! test -d ./packages ; then
  echo "ERROR: you should run the script under its directory"
  echo "错误：您只能在脚本所在目录运行它"
  exit -1
fi

# clean
if test "$1" = "--clean" ; then
  echo -n "Cleaning ... "
  rm -rf setup.log
  rm -rf xapian-core-scws-* scws-* libevent-* xunsearch-*
  echo "done"
  exit
fi

# get default prefix
if test -f $HOME/.xs_installed ; then
  def_prefix=`cat $HOME/.xs_installed`
elif test "$HOME" = "/" || test "$HOME" = "/root" ; then
  def_prefix=/usr/local/xunsearch
else
  def_prefix=$HOME/xunsearch
fi

# welcome msg
echo
echo "+==========================================+"
echo "| Welcome to setup xunsearch(full)         |"
echo "| 欢迎使用 xunsearch (完整版) 安装程序     |"
echo "+------------------------------------------+"
echo "| Follow the on-screen instructions please |"
echo "| 请按照屏幕上的提示操作以完成安装         |"
echo "+==========================================+"
echo

# ask prefix
echo "Please specify the installation directory"
echo "请指定安装目录 (默认为中括号内的值)"
echo -n "[$def_prefix]:"
while test -z ""; do
  read -e set_prefix
  if test $? -ne 0 ; then
    echo -n "[$def_prefix]:"
    read set_prefix
  fi
  if test -z "$set_prefix" ; then
    set_prefix=$def_prefix
  fi
  echo
  echo "Confirm the installation directory"
  echo -n "请确认安装目录：$set_prefix [Y/n]"
  read res_prefix
  if test -z "$res_prefix" || test "$res_prefix" = "y" || test "$res_prefix" = "Y" ; then
    break
  fi
  echo
  echo "Please re-input the installation directory"
  echo "请重新输入安装目录"
  echo -n "[$set_prefix]:"
done
echo

# record it
prefix=$set_prefix
echo $prefix > $HOME/.xs_installed

# compile flags
export CFLAGS=-O2
export CXXFLAGS=-O2
echo -n > setup.log

# error function
setup_abort()
{
  echo "-----"
  tail -4 ../setup.log
  echo "-----"
  echo "ERROR: failed to $1, see 'setup.log' for more detail"
  exit -3
}

# check & install scws
old_version=
echo -n "Checking scws ... "
if test -f $prefix/include/scws/scws_version.h ; then
  old_version=`cat $prefix/include/scws/scws_version.h | grep VERSION | cut -d\" -f2`
  echo $old_version
else
  echo "no"
fi
do_install=
new_file=`ls ./packages/scws-* | grep -v dict`
new_version=`echo $new_file | sed 's#^.*scws-\(.*\)\.tar\.bz2#\1#'`
if test -z "$old_version" ; then
  if test -z "$new_version" ; then
    echo "ERROR: Missing scws package (缺少 scws 安装包)"
    exit -2
  fi
  echo "Installing scws ($new_version) ... "
  do_install=yes  
elif ! test -z "$new_version" && test "$new_version" != "$old_version" ; then
  echo "Upgrading scws ($old_version -> $new_version)"
  do_install=yes
fi

if test "$do_install" = "yes" ; then
  echo "Extracting scws package ..."
  tar -xjf ./packages/scws-${new_version}.tar.bz2
  cd scws-$new_version
  echo "Configuring scws ..."
  ./configure --prefix=$prefix >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "configure scws"
  fi
  echo "Compiling & installing scws ..."
  make clean install >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "compile scws"
  fi
  cd ..
fi

# check & install scws dict
do_install=
new_dict=./packages/scws-dict-chs-utf8.tar.bz2
old_dict=$prefix/etc/dict.utf8.xdb
echo -n "Checking scws dict ... "
if test -f $old_dict ; then
  if test $new_dict -nt $old_dict ; then
    echo "expired"
    echo "Updating new scws dict file ... "
    do_install=yes
  else
    echo "ok"
  fi
else
  echo "no"
  echo "Extracting scws dict file ... "
  do_install=yes
fi
if test "$do_install" = "yes" ; then
  tar -xjf $new_dict -C $prefix/etc
  touch $old_dict
fi

# check & install xapian-scws
old_version=
echo -n "Checking xapian-core-scws ... "
if test -f $prefix/include/xapian/version.h ; then
  old_version=`cat $prefix/include/xapian/version.h | grep XAPIAN_VERSION | cut -d\" -f2`
  echo $old_version
else
  echo "no"
fi
do_install=
new_file=`ls ./packages/xapian-core-scws-*`
new_version=`echo $new_file | sed 's#^.*xapian-core-scws-\(.*\)\.tar\.bz2#\1#'`
if test -z "$old_version" ; then
  if test -z "$new_version" ; then
    echo "ERROR: Missing xapian-core-scws package (缺少 xapian-core-scws 安装包)"
    exit -2
  fi
  echo "Installing xapian-core-scws ($new_version) ... "
  do_install=yes  
elif ! test -z "$new_version" && test "$new_version" != "$old_version" ; then
  echo "Upgrading xapian ($old_version -> $new_version)"
  do_install=yes
fi

if test "$do_install" = "yes" ; then
  echo "Extracting xapian-core-scws package ..."
  tar -xjf $new_file
  cd xapian-core-scws-$new_version
  echo "Configuring xapian-core-scws ..."
  ./configure --prefix=$prefix --with-scws=$prefix >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "configure xapian-core-scws"
  fi
  echo "Compiling & installing xapian-core-scws ..."
  make clean install >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "compile xapian-core-scws"
  fi
  cd ..
fi

# check & install libevent
old_version=
echo -n "Checking libevent ... "
if test -f $prefix/include/event-config.h ; then
  old_version=`cat $prefix/include/event-config.h | grep EVENT_VERSION | cut -d\" -f2`
  echo $old_version
else
  echo "no"
fi
do_install=
new_file=`ls ./packages/libevent-*`
new_version=`echo $new_file | sed 's#^.*libevent-\(.*\)\.tar\.bz2#\1#'`
if test -z "$old_version" ; then
  if test -z "$new_version" ; then
    echo "ERROR: Missing libevent package (缺少 libevent 安装包)"
    exit -2
  fi
  echo "Installing libevent ($new_version) ... "
  do_install=yes  
elif ! test -z "$new_version" && test "$new_version" != "$old_version" ; then
  echo "Upgrading libevent ($old_version -> $new_version)"
  do_install=yes
fi

if test "$do_install" = "yes" ; then
  echo "Extracting libevent package ..."
  tar -xjf $new_file
  cd libevent-$new_version
  echo "Configuring libevent ..."
  ./configure --prefix=$prefix >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "configure libevent"
  fi
  echo "Compiling & installing libevent ..."
  make clean install >> ../setup.log 2>&1
  if test $? -ne 0 ; then
    setup_abort "compile libevent"
  fi
  cd ..
fi

# install/upgrade xunsearch
new_file=`ls ./packages/xunsearch-*`
new_version=`echo $new_file | sed 's#^.*xunsearch-\(.*\)\.tar\.bz2#\1#'`
if test -z "$new_version" ; then
  echo "ERROR: Missing xunsearch package (缺少 xunsearch 安装包)"
  exit -2
fi
echo "Extracting xunsearch package ($new_version) ..."
tar -xjf $new_file
cd xunsearch-$new_version
echo "Configuring xunsearch ..."
./configure --prefix=$prefix --with-scws=$prefix \
--with-libevent=$prefix --with-xapian=$prefix >> ../setup.log 2>&1
if test $? -ne 0 ; then
  setup_abort "configure xunsearch"
fi
echo "Compiling & installing xunsearch ..."
make clean install >> ../setup.log 2>&1
if test $? -ne 0 ; then
  setup_abort "compile xunsearch"
fi
cd ..

# all done
echo
echo "+=================================================+"
echo "| Installation completed successfully, Thanks you |"
echo "| 安装成功，感谢谢选择和使用 xunsearch            |"
echo "+-------------------------------------------------+"
echo "| 说明和注意事项：                                |"
echo "| 1. 开启/重新开启 xunsearch 服务程序，命令如下： |"
echo "|    $prefix/bin/xs-ctl.sh restart"
echo "|    强烈建议将此命令写入服务器开机脚本中         |"
echo "|                                                 |"
echo "| 2. 所有的索引数据将被保存在下面这个目录中：    |"
echo "|    $prefix/data"
echo "|    如需要转移到其它目录，请使用软链接。         |"
echo "|                                                 |"
echo "| 3. 您现在就可以在我们提供的开发包(SDK)基础上    |"
echo "|    开发您自己的搜索了。                         |"
echo "|    目前只支持 PHP 语言，参见下面文档：          |"
echo "|    $prefix/sdk/php/README"
echo "+=================================================+"
echo
exit

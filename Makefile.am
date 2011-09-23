# Automake file on top directory
# $Id: $
#
AUTOMAKE_OPTIONS = 1.5 subdir-objects

SUBDIRS = . src sdk

DIST_SUBDIRS = src sdk

sysconf_DATA = etc/stopwords.txt etc/py.xdb

EXTRA_DIST = README.md README.arch VERSION $(sysconf_DATA)

py_xdb_DEPS =
version_DEPS =

if HAVE_SDK_PHP_DEV
py_xdb_DEPS += sdk/php/dev/data/py.txt
version_DEPS += .git/refs/heads/master
endif HAVE_SDK_PHP_DEV

etc/py.xdb: $(py_xdb_DEPS)
	@echo "Generating pinyin dict by php ..."
	php sdk/php/dev/build.php pinyin make > /dev/null 2>&1

VERSION: $(version_DEPS)
	@echo "Generating version info ..."
	sh version-sh > VERSION

dist-hook:

	
dist-full: dist-bzip2
	sh -c 'cd xunsearch-full ; ./setup.sh --clean'	
	rm -f xunsearch-full/packages/xunsearch-*
	mv $(distdir).tar.bz2 xunsearch-full/packages
	@echo "Running distribute script ..."
	@sh dist-sh

web-doc:
	cd sdk && make php-web-doc
	scp xunsearch-web-doc-latest.tar.bz2 xunsearch:./web/protected/data
	ssh xunsearch tar -xjf ./web/protected/data/xunsearch-web-doc-latest.tar.bz2 -C ./web/protected/data
	rm -f xunsearch-web-doc-latest.tar.bz2
	

# Automake file on top directory
# $Id: $
#
AUTOMAKE_OPTIONS = 1.5 subdir-objects

SUBDIRS = . src sdk

DIST_SUBDIRS = src sdk

EXTRA_DIST = README.arch VERSION etc

sysconf_DATA = etc/stopwords.txt etc/py.xdb

etc/py.xdb:
	@echo "Generating pinyin dict by php ..."
	php sdk/php/dev/build.php pinyin make > /dev/null 2>&1

VERSION:
	@echo "Generating version info ..."
	sh version-sh > VERSION

dist-hook:
	sh version-sh > VERSION
	
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
	

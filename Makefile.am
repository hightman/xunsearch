# Automake file on top directory
# $Id: $
#
AUTOMAKE_OPTIONS = 1.5 subdir-objects

SUBDIRS = . src
if ENABLE_PHP
SUBDIRS += sdk
endif
sysconfDATA_INSTALL = $(INSTALL_DATA)

DIST_SUBDIRS = src sdk

sysconf_DATA = etc/py.xdb

customdir = ${sysconfdir}
custom_DATA = etc/dict_user.txt etc/stopwords.txt

EXTRA_DIST = README.md README.arch VERSION notify-sh $(sysconf_DATA) $(custom_DATA)

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

install-customDATA:
	@$(NORMAL_INSTALL)
	test -z "$(customdir)" || $(MKDIR_P) "$(DESTDIR)$(customdir)"
	@list='$(custom_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  $(am__strip_dir) \
	  if test -f "$(DESTDIR)$(customdir)/$$f"; then \
	    echo " Ignore exists custom data file '$$p'"; else \
	    echo " $(sysconfDATA_INSTALL) '$$d$$p' '$(DESTDIR)$(customdir)/$$f'"; \
	    $(sysconfDATA_INSTALL) "$$d$$p" "$(DESTDIR)$(customdir)/$$f"; \
	  fi; \
	done
	@sh notify-sh $(prefix)

uninstall-customDATA:
	@$(NORMAL_UNINSTALL)

dist-hook:

dist-sdk: distdir
	cd $(distdir) \
		&& cp -pR sdk xunsearch-sdk \
		&& rm -f xunsearch-sdk/Makefile* xunsearch-sdk/*.sh \
		&& sed -e 's#838#demo.xunsearch.com:939#g' \
			sdk/php/app/demo.ini > xunsearch-sdk/php/app/demo.ini \
		&& zip -qr9 ../xunsearch-sdk-$(VERSION).zip xunsearch-sdk \
		&& rm -rf xunsearch-sdk
	
dist-full: dist-sdk dist-bzip2
	sh -c 'cd xunsearch-full ; ./setup.sh --clean'	
	rm -f xunsearch-full/packages/xunsearch-*
	mv $(distdir).tar.bz2 xunsearch-sdk-$(VERSION).zip xunsearch-full/packages
	@echo "Running distribute script ..."
	@sh dist-sh

web-doc:
	cd sdk && make php-web-doc
	scp xunsearch-web-doc-latest.tar.bz2 xunsearch:./web/protected/data
	ssh xunsearch tar -xjf ./web/protected/data/xunsearch-web-doc-latest.tar.bz2 -C ./web/protected/data
	rm -f xunsearch-web-doc-latest.tar.bz2
	

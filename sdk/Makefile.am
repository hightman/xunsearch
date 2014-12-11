# Automake file for sdk
# $Id: $
#

sdkdir = ${prefix}/sdk
phpdir = ${sdkdir}/php
phpappdir = ${phpdir}/app
phplibdir = ${phpdir}/lib

phpapp_DATA = php/app/demo.ini
phplib_DATA = php/lib/XS.php
phplib_FILES = 

if HAVE_SDK_PHP_DEV
phplib_FILES += php/lib/xs_cmd.inc.php php/lib/*.class.php
endif HAVE_SDK_PHP_DEV

EXTRA_DIST = gen-phpinc.sh php/README.md php/doc php/app/demo.ini php/lib/XS.php php/util
EXTRA_DIST += php/wrapper

php/lib/xs_cmd.inc.php: ../config.h ../src/xs_cmd.h
	sh gen-phpinc.sh > $@

php/lib/XS.php: $(phplib_FILES)
	@echo "Regenerating XS library class file ..."
	@php php/dev/build.php lite > /dev/null 2>&1

dist-hook: 
	@echo "Regenerating offline documents ..."
	@rm -rf $(distdir)/php/doc
	@php php/dev/build.php doc offline $(distdir)/php/doc > /dev/null 2>&1
	@echo "Regenerating XS library class file ..."
	@php php/dev/build.php lite > /dev/null 2>&1
	
php-web-doc:
	@echo "Regenerating online documents ..."
	@rm -rf php/doc/api
	@php php/dev/build.php doc online > /dev/null 2>&1
	@echo "Making tar ball ..."
	@tar -cjf ../xunsearch-web-doc-latest.tar.bz2 -C php --exclude doc/html doc/

install: install-am install-sdk
	
uninstall: uninstall-am uninstall-sdk

install-sdk:
	$(MKDIR_P) "$(DESTDIR)$(phpdir)"
	-cp -rf php/README.md php/util php/doc $(DESTDIR)$(phpdir)/
	-ln -sf $(DESTDIR)$(phpdir)/util/xs $(bindir)/xs-php
	chmod 0755 $(DESTDIR)$(phpdir)/util/*.php
	chmod 0755 $(DESTDIR)$(phpdir)/util/xs

uninstall-sdk:
	-rmdir $(phpappdir) $(phplibdir)
	-rm -rf $(phpdir)/README.md $(phpdir)/util $(phpdir)/doc
	-rmdir $(phpdir) $(sdkdir)

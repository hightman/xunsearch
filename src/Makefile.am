# Automake file for core source directory
# $Id: $

INCLUDES =
AM_CFLAGS = -Wall

bin_PROGRAMS = xs-import xs-indexd xs-logging xs-searchd

noinst_HEADERS  = conn.h flock.h global.h log.h mcache.h md5.h
noinst_HEADERS += mm.h pinyin.h pcntl.h task.h tpool.h user.h xs_cmd.h
noinst_HEADERS += import.h indexd.h searchd.h

xs_import_SOURCES = flock.c import.cc log.c pcntl.c
xs_import_LDADD = -lxapian -lscws

xs_indexd_SOURCES = conn.c flock.c log.c pcntl.c user.c
xs_indexd_SOURCES += indexd.c
xs_indexd_LDADD = -levent_core

xs_logging_SOURCES = flock.c log.c logging.cc pinyin.c
xs_logging_LDADD = -lxapian -lscws

xs_searchd_SOURCES = conn.c flock.c log.c mm.c pcntl.c pinyin.c tpool.c user_mm.c
xs_searchd_SOURCES += searchd.cc task.cc
if HAVE_MEMORY_CACHE
xs_searchd_SOURCES += mcache.c md5.c
endif HAVE_MEMORY_CACHE
xs_searchd_LDADD = -levent_core -lxapian -lscws

EXTRA_DIST = xs-ctl.sh.in xs-optimize.sh.in

user_mm.c: user.c
	$(COMPILE) -DHAVE_MM -E -o $@ $?
	
clean-generic:
	-rm -f user_mm.c

dist-hook:
	-rm -f user_mm.c

install: install-am install-shell

install-exec: install-exec-am install-shell

uninstall: uninstall-am uninstall-shell

install-shell:
	$(mkinstalldirs) $(DESTDIR)$(prefix)/data
	$(mkinstalldirs) $(DESTDIR)$(prefix)/tmp
	$(INSTALL) -m 0755 xs-ctl.sh $(DESTDIR)$(bindir)/xs-ctl.sh
	$(INSTALL) -m 0755 xs-optimize.sh $(DESTDIR)$(bindir)/xs-optimize.sh

uninstall-shell:
	-rmdir $(DESTDIR)$(prefix)/data $(DESTDIR)$(prefix)/tmp
	-rm -f $(DESTDIR)$(bindir)/xs-ctl.sh
	-rm -f $(DESTDIR)$(bindir)/xs-optimize.sh


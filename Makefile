#!/usr/bin/make -f
#
APXS=./apxs.sh

mod_statsd: mod_statsd.so
	@echo make done
	@echo type \"make install\" to install mod_statsd

mod_statsd.so: mod_statsd.c
	@$(APXS) -c -n $@ mod_statsd.c

mod_statsd.c:

install: mod_statsd.so
	@$(APXS) -i -S LIBEXECDIR=$(DESTDIR)$$($(APXS) -q LIBEXECDIR)/ -n mod_statsd.so mod_statsd.la

clean:
	@rm -rf *~ *.o *.so *.lo *.la *.slo *.loT .libs/


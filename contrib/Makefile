#!/usr/bin/make -f
#
CWD=$(shell pwd -P)
SPEC=mod_statsd.spec

rpm: srpm
	sudo mock --rebuild $(CWD)/SRPMS/*.src.rpm --resultdir=$(CWD)/RPMS/

srpm: prepare
	sudo mock --buildsrpm --spec=$(CWD)/$(SPEC) --sources=$(CWD)/ --resultdir=$(CWD)/SRPMS

clean:
	@rm -rf RPMS SRPMS src mod_statsd.tar.gz

prepare: clean
	@mkdir -v SRPMS RPMS src
	@cp ../mod_statsd.c src/
	@cp ../statsd.load src/
	@cp ../DOCUMENTATION src/
	@cp ../LICENSE src/
	tar -czf mod_statsd.tar.gz src


# Temporary hack that needs to be removed / replaced
-include ../tmp/buildopts.mk
OPTS += $(OPTSATBUILDTIME)

DIRS=util charm++ converse ampi

all:
	for d in $(DIRS); do \
		(cd $$d && $(MAKE) all OPTS='$(OPTS)' || exit 1) || exit 1; \
	done

test:
	for d in $(DIRS); do \
		(cd $$d && $(MAKE) test OPTS='$(OPTS)' || exit 1) || exit 1; \
	done

bgtest:
	for d in charm++ ampi; do \
		(cd $$d && $(MAKE) bgtest OPTS='$(OPTS)' || exit 1) || exit 1; \
	done

syncfttest causalfttest mpisyncfttest mlogfttest:
	for d in charm++ ampi; do \
		(cd $$d && $(MAKE) $@ OPTS='$(OPTS)' || exit 1) || exit 1; \
	done

test-converse:
	cd converse; $(MAKE) test OPTS='$(OPTS)'

test-charm:
	cd charm++; $(MAKE) test OPTS='$(OPTS)'

test-AMPI:
	cd ampi; $(MAKE) test OPTS='$(OPTS)'

test-fem:
	cd fem; $(MAKE) test OPTS='$(OPTS)'

clean:
	for d in $(DIRS); do (cd $$d && $(MAKE) clean OPTS='$(OPTS)'); done
	rm -f TAGS #*#
	rm -f core *~


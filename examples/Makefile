DIRS=charm++ converse ampi armci bigsim


all:
	for d in $(DIRS); do \
		(cd $$d; $(MAKE) all OPTS='$(OPTS)' || exit 1) || exit 1; \
	done

test:
	for d in $(DIRS); do \
		(cd $$d; $(MAKE) test OPTS='$(OPTS)' TESTOPTS='$(TESTOPTS)' || exit 1) || exit 1; \
	done

test-fem:
	$(MAKE) -C fem test OPTS='$(OPTS)' TESTOPTS='$(TESTOPTS)'

bgtest:
	for d in charm++ ampi armci; do \
		(cd $$d; $(MAKE) bgtest OPTS='$(OPTS)' TESTOPTS='$(TESTOPTS)' || exit 1) || exit 1; \
	done

clean:
	for d in $(DIRS); do (cd $$d; $(MAKE) clean OPTS='$(OPTS)'); done
	rm -f TAGS #*#
	rm -f core *~

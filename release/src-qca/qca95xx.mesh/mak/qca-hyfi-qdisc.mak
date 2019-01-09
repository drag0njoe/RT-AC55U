qca-hyfi-qdisc:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-hyfi-qdisc-stage:
	$(MAKE) -C qca-hyfi-qdisc && $(MAKE) -C qca-hyfi-qdisc stage

qca-hyfi-qdisc-install: qca-hyfi-qdisc
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

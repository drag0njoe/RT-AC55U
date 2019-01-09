qca-hyfi-bridge:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-hyfi-bridge-stage:
	$(MAKE) -C qca-hyfi-bridge && $(MAKE) -C qca-hyfi-bridge stage

qca-hyfi-bridge-install: qca-hyfi-bridge
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

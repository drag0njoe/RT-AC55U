qca-wifi: iproute2-3.x
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-wifi-install: qca-wifi
	$(MAKE) -C $< INSTALLDIR=$(INSTALLDIR)/$< install

qca-wifi-clean:
	$(MAKE) -C qca-wifi clean

qca-wifi-stage:
	$(MAKE) -C qca-wifi stage

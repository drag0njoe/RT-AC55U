qca-hostap: qca-wifi libnl-bf openssl
	$(MAKE) -C $@ && $(MAKE) $@-stage

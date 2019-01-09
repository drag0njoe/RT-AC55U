shortcut-fe: libnl-bf
	$(MAKE) -C $@ && $(MAKE) $@-stage

shortcut-fe-stage:
	$(MAKE) -C shortcut-fe stage

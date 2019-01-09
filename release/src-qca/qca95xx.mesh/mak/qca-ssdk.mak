qca-ssdk:
	$(MAKE) -C $@ && $(MAKE) $@-stage

qca-ssdk-stage:
	$(MAKE) -C qca-ssdk stage

LinuxART2-install: LinuxART2
ifneq ($(ART2_INSTALLDIR),)
	$(MAKE) -C $< install INSTALLDIR=$(ART2_INSTALLDIR)/$< BUILDIN=0
else
	$(MAKE) -C $< install INSTALLDIR=$(INSTALLDIR)/$< BUILDIN=1
endif

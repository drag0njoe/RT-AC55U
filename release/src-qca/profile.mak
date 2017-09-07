EXTRACFLAGS := -DLINUX26 -DCONFIG_QCA -DDEBUG_NOISY -DDEBUG_RCTEST -pipe -funit-at-a-time -Wno-pointer-sign -mtune=mips32 -mips32

ifneq ($(findstring linux-3,$(LINUXDIR)),)
EXTRACFLAGS += -DLINUX30
endif

export EXTRACFLAGS

obj-$(CONFIG_LTQ_DATAPATH) += datapath_misc.o datapath_gswip.o datapath_proc.o datapath_lookup_proc.o

ifneq ($(CONFIG_LTQ_DATAPATH_HAL_GSWIP30_MIB),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_mib.o
endif

ifneq ($(CONFIG_LTQ_DATAPATH_CPUFREQ),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_coc.o
endif




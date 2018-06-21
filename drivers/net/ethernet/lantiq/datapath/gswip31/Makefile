ifneq ($(CONFIG_LTQ_DATAPATH_DDR_SIMULATE_GSWIP31),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_gswip_simulate.o
endif

obj-$(CONFIG_LTQ_DATAPATH) += datapath_misc.o datapath_gswip.o datapath_proc.o
obj-$(CONFIG_LTQ_DATAPATH) += datapath_ppv4.o
obj-$(CONFIG_LTQ_DATAPATH) += datapath_lookup_proc.o datapath_ppv4_api.o

ifneq ($(CONFIG_LTQ_DATAPATH_HAL_GSWIP31_MIB),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_mib.o
endif

ifneq ($(CONFIG_LTQ_DATAPATH_HAL_GSWIP31_COC),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_coc.o
endif

ifneq ($(CONFIG_LTQ_DATAPATH_SWITCHDEV),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_switchdev.o datapath_ext_vlan.o datapath_tc_asym_vlan.o
endif

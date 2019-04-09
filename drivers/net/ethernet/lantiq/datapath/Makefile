obj-$(CONFIG_LTQ_DATAPATH) = datapath_api.o datapath_proc_api.o datapath_proc.o  datapath_misc.o datapath_notifier.o datapath_logical_dev.o datapath_instance.o datapath_platform_dev.o datapath_soc.o datapath_qos.o datapath_proc_qos.o

ifneq ($(CONFIG_INTEL_DATAPATH_HAL_GSWIP31),)
obj-$(CONFIG_LTQ_DATAPATH) += gswip31/
endif
ifneq ($(CONFIG_INTEL_DATAPATH_HAL_GSWIP30),)
obj-$(CONFIG_LTQ_DATAPATH) += gswip30/
endif

ifneq ($(CONFIG_LTQ_DATAPATH_SWITCHDEV),)
obj-$(CONFIG_LTQ_DATAPATH) += datapath_swdev.o
endif

obj-$(CONFIG_LTQ_DATAPATH) += datapath_ioctl.o


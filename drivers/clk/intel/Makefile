#
#Makefile for intel specific clk
#

obj-y += clk_api.o
ifneq ($(CONFIG_FALCONMX_CLK),)
	obj-y += clk-falconmx.o
else ifneq ($(CONFIG_GRX500_CLK),)
	obj-y += clk-grx500.o
else ifneq ($(CONFIG_GRX330_CLK),)
	obj-y += clk-grx330.o
else ifneq ($(CONFIG_GRX200_CLK),)
	obj-y += clk-grx200.o
endif

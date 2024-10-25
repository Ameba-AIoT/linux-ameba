# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the RTK inic drivers.
#

# RTK inic drivers for ameba
inic_core := 	inic_ipc_msg_queue.o \
		wifi_conf_ext_inic_ipc.o \
		wifi_conf_inic_ipc.o \
		wifi_promisc_inic_ipc.o \
		wifi_ind.o \
		inic_cmd.o \
		inic_ext_cmd.o \
		inic_prmc_cmd.o \
		inic_ipc_host_api.o \
		inic_net_device.o \
		inic_ipc_host_trx.o \
		inic_ipc_host.o

obj-$(CONFIG_INIC_RTK_AMEBA) += $(inic_core)
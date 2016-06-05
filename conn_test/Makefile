APPLICATION = conn_test
BOARD ?= iotlab-m3 
RIOTBASE ?= $(CURDIR)/../../RIOT
CFLAGS += -DDEVELHELP
QUIET ?= 1

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += xtimer
USEMODULE += hashes
USEMODULE += random
USEMODULE += ps 
USEMODULE += netstats_l2 
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif

include $(RIOTBASE)/Makefile.include

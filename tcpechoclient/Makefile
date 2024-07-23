APPLICATION = tcpechoclient

BOARD ?= native

RIOTBASE ?= $(CURDIR)/../../RIOT
CFLAGS += -DDEVELHELP
QUIET ?= 1

USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_icmpv6_echo
USEMODULE += sock_tcp
USEMODULE += shell
USEMODULE += shell_cmds_default
USEMODULE += ps

include $(RIOTBASE)/Makefile.include

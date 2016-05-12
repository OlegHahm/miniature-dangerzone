APPLICATION = ccnl_clustering

# If no BOARD is found in the environment, use this default:
BOARD ?= native

BOARD_WHITELIST := fox iotlab-m3 msba2 mulle native pba-d-01-kw2x samr21-xpro


# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

CFLAGS += -DDEVELHELP
CFLAGS += -DUSE_LINKLAYER
CFLAGS += -DCCNL_UAPI_H_
CFLAGS += -DUSE_SUITE_NDNTLV
CFLAGS += -DNEEDS_PREFIX_MATCHING
CFLAGS += -DNEEDS_PACKET_CRAFTING
CFLAGS += -DUSE_HMAC256
CFLAGS += -DUSE_STATS
CFLAGS += -DUSE_RONR
CFLAGS += -DUSE_DUP_CHECK

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += ps
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += gnrc_netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += timex
USEMODULE += xtimer
USEMODULE += random
USEMODULE += hashes
USEMODULE += bloom
ifeq (,$(filter native,$(BOARD)))
	USEMODULE += netdev_retrans
endif
USEMODULE += netstats_l2

USEPKG += tlsf

USEPKG += ccn-lite
USEMODULE += ccn-lite-utils

include $(RIOTBASE)/Makefile.include

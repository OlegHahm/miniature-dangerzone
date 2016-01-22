APPLICATION = noise

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

CFLAGS += -DDEVELHELP

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += xtimer
USEMODULE += gnrc_netif_default
USEMODULE += auto_init_gnrc_netif

include $(RIOTBASE)/Makefile.include

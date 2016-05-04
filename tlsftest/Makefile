APPLICATION = tlsf_test

# If no BOARD is found in the environment, use this default:
BOARD ?= native

BOARD_WHITELIST := fox iotlab-m3 msba2 mulle native pba-d-01-kw2x samr21-xpro


# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

CFLAGS += -DDEVELHELP

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += ps
USEMODULE += shell
USEMODULE += shell_commands

USEPKG += tlsf

include $(RIOTBASE)/Makefile.include

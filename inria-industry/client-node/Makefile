# name of your application
APPLICATION = client-node

# If no BOARD is found in the environment, use this default:
BOARD ?= avsextrem 

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../../RIOT

# Uncomment these lines if you want to use platform support from external
# repositories:
#RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
#RIOTBOARD ?= $(CURDIR)/../../RIOT/thirdparty_boards

# Uncomment this to enable scheduler statistics for ps:
CFLAGS += -DSCHEDSTATISTICS

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Uncomment this to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP
CFLAGS += "-DDBG_IGNORE"

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += posix
USEMODULE += ps
USEMODULE += random
USEMODULE += defaulttransceiver
USEMODULE += vtimer
USEMODULE += ccn_lite
USEMODULE += ccn_lite_client

include $(RIOTBASE)/Makefile.include

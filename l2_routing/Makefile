APPLICATION = l2_routing

FEATURES_REQUIRED = periph_spi periph_gpio

# If no BOARD is found in the environment, use this default:
BOARD ?= iot-lab_M3

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

# Uncomment this to enable scheduler statistics for ps:
#CFLAGS += -DSCHEDSTATISTICS

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Uncomment this to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# Modules to include:
USEMODULE += ng_netbase
USEMODULE += ng_nomac
USEMODULE += ng_pktdump
USEMODULE += ng_at86rf231
USEMODULE += uart0
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += auto_init_ng_netif
USEMODULE += ps

include $(RIOTBASE)/Makefile.include
include $(RIOTBASE)/dist/testbed-support/Makefile.iot-lab

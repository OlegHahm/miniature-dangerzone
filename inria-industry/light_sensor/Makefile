# name of your application
export PROJECT = accel_sensor

# If no BOARD is found in the environment, use this default:
export BOARD ?= iot-lab_M3 

# This has to be the absolute path to the RIOT base directory:
export RIOTBASE ?= $(CURDIR)/../../../RIOT

# Uncomment this to enable scheduler statistics for ps:
CFLAGS += -DSCHEDSTATISTICS

# Uncomment this to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP
CFLAGS += "-DDBG_IGNORE"

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

# get rid of the mandatory RPL warning
CFLAGS += "-Wno-cpp"

# Modules to include:

USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += uart0
USEMODULE += ps
USEMODULE += defaulttransceiver
USEMODULE += vtimer
USEMODULE += rpl
USEMODULE += udp

#include events.h
export INCLUDES += -I$(CURDIR)/.. -I$(CURDIR)

include $(RIOTBASE)/Makefile.include

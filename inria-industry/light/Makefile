# name of your application
export PROJECT = inria_industry_light

# If no BOARD is found in the environment, use this default:
export BOARD ?= msba2

# This has to be the absolute path to the RIOT base directory:
export RIOTBASE ?= $(CURDIR)/../../../riot/RIOT

# Uncomment these lines if you want to use platform support from external
# repositories:
#export RIOTCPU ?= $(CURDIR)/../../RIOT/thirdparty_cpu
#export RIOTBOARD ?= $(CURDIR)/../../RIOT/thirdparty_boards

# Uncomment this to enable scheduler statistics for ps:
#CFLAGS += -DSCHEDSTATISTICS

# If you want to use native with valgrind, you should recompile native
# with the target all-valgrind instead of all:
# make -B clean all-valgrind

# Uncomment this to enable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
#CFLAGS += -DDEVELHELP

# Change this to 0 show compiler invocation lines by default:
export QUIET ?= 1

# get rid of the mandatory RPL warning
CFLAGS += "-Wno-cpp"
CFLAGS += "-DDBG_IGNORE"
CFLAGS += -DINRIA_INDUSTRY_RX_TOGGLE=1

# Modules to include:

USEMODULE += auto_init
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += posix
USEMODULE += ps
USEMODULE += vtimer
USEMODULE += sixlowpan
USEMODULE += rpl
USEMODULE += udp
USEMODULE += transceiver
USEMODULE += rgbled
ifeq ($(strip $(BOARD)),native)
	USEMODULE += nativenet
else ifeq ($(strip $(BOARD)),msba2)
	USEMODULE += cc110x_ng
else ifeq ($(strip $(BOARD)),avsextrem)
	USEMODULE += cc110x_ng
endif

export INCLUDES += -I$(RIOTBASE)/sys/net/include -I$(RIOTBASE)/sys/net/routing/rpl -I$(RIOTBASE)/drivers/cc110x

# include events.h
export INCLUDES += -I$(CURDIR)/.. -I$(CURDIR)

include $(RIOTBASE)/Makefile.include

# name of your application
APPLICATION = gnrc_networking

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

BOARD_INSUFFICIENT_MEMORY := airfy-beacon chronos msb-430 msb-430h nrf51dongle \
                          nrf6310 nucleo-f334 pca10000 pca10005 \
                          stm32f0discovery telosb wsn430-v1_3b wsn430-v1_4 \
                          yunjia-nrf51822 z1

BOARD_BLACKLIST        := arduino-mega2560 spark-core
# arduino-mega2560: unknown error types (e.g. -EBADMSG)

# Include packages that pull up and auto-init the link layer.
# NOTE: 6LoWPAN will be included if IEEE802.15.4 devices are present
USEMODULE += gnrc_netif_default
USEMODULE += auto_init_gnrc_netif
# Specify the mandatory networking modules for IPv6 and UDP
USEMODULE += gnrc_sixlowpan_border_router_default
USEMODULE += gnrc_udp
USEMODULE += gnrc_slip
# This application dumps received packets to STDIO using the pktdump module
USEMODULE += gnrc_pktdump
# Additional networking modules that can be dropped if not needed
USEMODULE += gnrc_icmpv6_echo
USEMODULE += fib
# Add also the shell, some shell commands
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
CFLAGS += -DDEVELHELP

INCLUDES += -I$(CURDIR)

ifneq (,$(SLIP_UART))
  CFLAGS += -DSLIP_UART=$(SLIP_UART)
else
  # set default
  CFLAGS += -DSLIP_UART="(UART_NUMOF-1)"
endif
ifneq (,$(SLIP_BAUDRATE))
  CFLAGS += -DSLIP_BAUDRATE=$(SLIP_BAUDRATE)
else
  # set default
  CFLAGS += -DSLIP_BAUDRATE=115200
endif

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include

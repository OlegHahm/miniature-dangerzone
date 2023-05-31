APPLICATION = dagstuhl-demo

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += sock_dns              # include DNS client
USEMODULE += gnrc_ipv6_nib_dns     # include RDNSS option handling

ifneq (,$(DEFAULT_MQTT_CLIENT_ID))
  CFLAGS += -DDEFAULT_MQTT_CLIENT_ID=\"$(DEFAULT_MQTT_CLIENT_ID)\"
endif
ifneq (,$(DEFAULT_MQTT_USER))
  CFLAGS += -DDEFAULT_MQTT_USER=\"$(DEFAULT_MQTT_USER)\"
endif
ifneq (,$(DEFAULT_MQTT_PWD))
  CFLAGS += -DDEFAULT_MQTT_PWD=\"$(DEFAULT_MQTT_PWD)\"
endif

ifeq (,$(filter native,$(BOARD)))
USEMODULE += ztimer_no_periph_rtt
USEMODULE += rtt_rtc
endif
FEATURES_REQUIRED += periph_gpio_irq

USEMODULE += shell_cmds_default
USEMODULE += ps
USEMODULE += sntp
USEMODULE += saul_default
USEMODULE += netdev_default
USEMODULE += gnrc_netif_bus
USEPKG += paho-mqtt

# paho-mqtt depends on TCP support, choose which stacks you want
GNRC_IPV6 ?= 1
LWIP_IPV4 ?= 0
LWIP_IPV6 ?= 0

include Makefile.lwip
include Makefile.gnrc

USEMODULE += sock_async_event
USEMODULE += sock_ip
USEMODULE += sock_udp
USEMODULE += sock_tcp

USEMODULE += ztimer
USEMODULE += ztimer_msec

####

include $(RIOTBASE)/Makefile.include

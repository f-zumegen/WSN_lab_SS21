CONTIKI_PROJECT = routing

all: $(CONTIKI_PROJECT)

#UIP_CONF_IPV6=1
CONTIKI_WITH_RIME = 1

CONTIKI = $(HOME)/contiki
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
include $(CONTIKI)/Makefile.include

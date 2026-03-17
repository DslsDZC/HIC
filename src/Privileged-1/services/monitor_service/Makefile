# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# Monitor Service Makefile

SERVICE_NAME = monitor_service

SRCS = service.c
OBJS = $(SRCS:.c=.o)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostdlib
CFLAGS += -I../../include -I../../../Core-0
LDFLAGS = -shared -nostdlib

all: $(SERVICE_NAME).hicmod

$(SERVICE_NAME).hicmod: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(SERVICE_NAME).hicmod

.PHONY: all clean

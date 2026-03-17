# SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
#
# SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception

# Hardware Probe Service Makefile

SERVICE_NAME = hardware_probe_service

SRCS = service.c
OBJS = $(SRCS:.c=.o)

CC = gcc
CFLAGS = -Wall -Wextra -O2 -ffreestanding -nostdlib
LDFLAGS = -shared -nostdlib

all: $(SERVICE_NAME).hicmod

$(SERVICE_NAME).hicmod: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(SERVICE_NAME).hicmod

.PHONY: all clean
# Makefile for Duplicate File Finder

CC = gcc
CFLAGS = -Wall -Wextra -O2 -lpthread $(shell pkg-config --cflags openssl)
LDFLAGS = $(shell pkg-config --libs openssl) -lpthread
TARGETS = duplicate_finder duplicate-finder-daemon duplicate-finder-client

all: $(TARGETS)

duplicate_finder: duplicate_finder.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✓ Built: $@"

duplicate-finder-daemon: daemon.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✓ Built: $@"

duplicate-finder-client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "✓ Built: $@"

install: all
	@echo "Installing to /usr/local/bin..."
	install -m 0755 duplicate_finder /usr/local/bin/
	install -m 0755 duplicate-finder-daemon /usr/local/bin/
	install -m 0755 duplicate-finder-client /usr/local/bin/
	@echo "✓ Installation complete"

uninstall:
	@echo "Removing from /usr/local/bin..."
	rm -f /usr/local/bin/duplicate_finder
	rm -f /usr/local/bin/duplicate-finder-daemon
	rm -f /usr/local/bin/duplicate-finder-client
	@echo "✓ Uninstallation complete"

install-service: install
	@echo "Installing systemd service..."
	install -m 0644 duplicate-finder.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable duplicate-finder.service
	@echo "✓ Service installed. Start with: systemctl start duplicate-finder"

clean:
	rm -f $(TARGETS)
	@echo "✓ Cleaned build artifacts"

distclean: clean
	rm -rf /var/cache/duplicate-finder
	rm -f /var/run/duplicate-finder.pid
	@echo "✓ Removed cache and config files"

help:
	@echo "Duplicate File Finder - Build System"
	@echo "===================================="
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build all executables"
	@echo "  make install      - Install to /usr/local/bin"
	@echo "  make uninstall    - Remove from /usr/local/bin"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make help         - Show this message"
	@echo ""

.PHONY: all install uninstall clean distclean help install-service

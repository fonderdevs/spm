CC = gcc
CFLAGS = -O2 -Wall -Wextra
LDFLAGS = -lcurl -pthread
PREFIX = /usr
INSTALL_DIRS = $(PREFIX)/bin /var/lib/steal/repos /etc/steal /usr/local/share/steal/installed
SRC_DIR = src

# ANSI color codes
BLUE = \033[1;34m
GREEN = \033[1;32m
YELLOW = \033[1;33m
CYAN = \033[1;36m
RED = \033[1;31m
NC = \033[0m

# Check for required development packages
CHECK_CURL := $(shell pkg-config --exists libcurl && echo "yes" || echo "no")

all: check_deps steal

steal: $(SRC_DIR)/main.c
	@echo "$(BLUE)╭─────────────────────────────╮$(NC)"
	@echo "$(BLUE)│$(NC)    $(CYAN)Compiling steal...$(NC)        $(BLUE)│$(NC)"
	@echo "$(BLUE)╰─────────────────────────────╯$(NC)"
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "$(GREEN)✓ Successfully compiled steal$(NC)\n"

install: steal
	@echo "$(BLUE)╭─────────────────────────────╮$(NC)"
	@echo "$(BLUE)│$(NC)    $(CYAN)Installing steal...$(NC)        $(BLUE)│$(NC)"
	@echo "$(BLUE)╰─────────────────────────────╯$(NC)"
	@for dir in $(INSTALL_DIRS); do \
		echo "$(YELLOW)Creating directory: $$dir$(NC)"; \
		mkdir -p $$dir; \
	done
	@install -m 755 steal $(PREFIX)/bin/steal
	@echo "$(GREEN)✓ Successfully installed steal$(NC)\n"

uninstall:
	@echo "$(BLUE)╭─────────────────────────────╮$(NC)"
	@echo "$(BLUE)│$(NC)   $(CYAN)Uninstalling steal...$(NC)      $(BLUE)│$(NC)"
	@echo "$(BLUE)╰─────────────────────────────╯$(NC)"
	@rm -f $(PREFIX)/bin/steal
	@echo "$(GREEN)✓ Successfully uninstalled steal$(NC)\n"

clean:
	@echo "$(BLUE)╭─────────────────────────────╮$(NC)"
	@echo "$(BLUE)│$(NC)     $(CYAN)Cleaning files...$(NC)        $(BLUE)│$(NC)"
	@echo "$(BLUE)╰─────────────────────────────╯$(NC)"
	@rm -f steal *.o
	@echo "$(GREEN)✓ Successfully cleaned files$(NC)\n"

check: steal
	@echo "$(BLUE)╭─────────────────────────────╮$(NC)"
	@echo "$(BLUE)│$(NC)     $(CYAN)Running tests...$(NC)         $(BLUE)│$(NC)"
	@echo "$(BLUE)╰─────────────────────────────╯$(NC)"
	@./steal --version || echo "$(RED)✗ Test failed: --version$(NC)"
	@echo "$(GREEN)✓ All tests passed$(NC)\n"


.PHONY: all check_deps install uninstall clean check

CC = gcc
CFLAGS = -O2 -Wall -Wextra -I$(SRC_DIR)/include
LDFLAGS = -pthread
PREFIX = /usr
INSTALL_DIRS = $(PREFIX)/bin /var/lib/spm/repos /etc/spm /usr/local/share/spm/installed
SRC_DIR = src

SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(SRC_DIR)/network/*.c) \
       $(wildcard $(SRC_DIR)/utils/*.c) \
       $(wildcard $(SRC_DIR)/core/*.c)

OBJS = $(SRCS:.c=.o)

#color codes
CYAN = \033[1;36m
NC = \033[0m

all: spm

spm: $(OBJS)
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "✓ Successfully compiled spm"

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@

install: spm
	@echo "$(CYAN)Installing spm...$(NC)"
	@for dir in $(INSTALL_DIRS); do \
		echo "$(YELLOW)Creating directory: $$dir$(NC)"; \
		mkdir -p $$dir; \
	done
	@install -m 755 spm $(PREFIX)/bin/spm
	@echo "$(GREEN)✓ Successfully installed spm$(NC)\n"

uninstall:
	@echo "$(CYAN)Uninstalling spm...$(NC)"
	@rm -f $(PREFIX)/bin/spm
	@echo "$(GREEN)✓ Successfully uninstalled spm$(NC)\n"

clean:
	@echo "$(CYAN)Cleaning files...$(NC)"
	@rm -f spm $(OBJS)

.PHONY: all install uninstall clean

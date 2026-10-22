CC = gcc
CFLAGS = -Wall -Wextra -O2 -Isrc/common
TARGET = bin/vn
SRC = src/virtual.c

ORACLE_IP ?= 127.0.0.1
BASE_UDP_PORT ?= 6000
N ?= 5
OWN_IP ?= 127.0.0.1

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run-oracle:
	@echo Starting oracle
	@cd src && python3 oracle.py

run-nodes: $(TARGET)
	@mkdir -p logs
	@echo Starting $(N) nodes
	@for i in $$(seq 1 $(N)); do \
		PORT=$$(( $(BASE_UDP_PORT) + $$i - 1 )); \
		echo Starting node $$i on port $$PORT; \
		./$(TARGET) $(ORACLE_IP) $$PORT $(OWN_IP) > logs/output$$i.txt 2> logs/error$$i.txt & \
		sleep 0.2; \
	done

stop:
	@echo Stopping all nodes and oracle
	@-pkill -f "$(TARGET)" 2>/dev/null || true
	@-pkill -f "oracle.py" 2>/dev/null || true

clean:
	rm -rf bin/ logs/

.PHONY: all clean run-oracle run-nodes

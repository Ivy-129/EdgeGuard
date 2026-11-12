CC := gcc
DEBUG ?= 0
CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -O2 -fPIC -Iinclude
ifeq ($(DEBUG),1)
CFLAGS += -DEDGE_DEBUG -g
endif
LDFLAGS := -Llib -Wl,-rpath,'$$ORIGIN/../lib' -ledgeguard
LIBSRC := src/protocol.c src/logger.c src/socket_utils.c
.PHONY: all clean test
all: bin/server bin/client
lib/libedgeguard.so: $(LIBSRC) include/protocol.h include/logger.h include/socket_utils.h
	@mkdir -p lib
	$(CC) $(CFLAGS) -shared -o $@ $(LIBSRC)
bin/server: src/server.c src/client_handler.c src/system_info.c src/ipc.c src/job_table.c src/worker.c src/resource_monitor.c include/server.h include/ipc.h include/job.h include/worker.h include/resource_monitor.h lib/libedgeguard.so
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ src/server.c src/client_handler.c src/system_info.c src/ipc.c src/job_table.c src/worker.c src/resource_monitor.c $(LDFLAGS) -pthread -lcrypto
bin/client: src/client.c lib/libedgeguard.so
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ src/client.c $(LDFLAGS)
test: all
	bash tests/smoke.sh
	python3 tests/test_worker_timeout.py
	python3 tests/test_hash_security.py
	python3 tests/test_shutdown.py
	python3 tests/test_worker_shutdown.py
	gcc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -Iinclude -o bin/test_semaphore_timeout tests/test_semaphore_timeout.c src/job_table.c -pthread
	timeout 8s ./bin/test_semaphore_timeout
clean:
	rm -f bin/server bin/client lib/libedgeguard.so

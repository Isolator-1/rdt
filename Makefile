CC = g++
CC_FLAGS = -O2
LIB = -lws2_32

TEST_DIR = test/
BIN_DIR = bin/
SRC_DIR = src/


all: rdtsend_tcp rdtrecv_tcp

test_rdt: $(TEST_DIR)test_rdt.cpp $(SRC_DIR)rdt.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_timer: $(TEST_DIR)test_timer.cpp $(SRC_DIR)timer.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_io: $(TEST_DIR)test_io.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_cq: $(TEST_DIR)test_cq.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_lock: $(TEST_DIR)test_lock.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_precompile: $(TEST_DIR)test_precompile.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

rdtsend_gbn: $(SRC_DIR)rdtsend_gbn.cpp $(SRC_DIR)timer.cpp $(SRC_DIR)rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

rdtrecv_gbn: $(SRC_DIR)rdtrecv_gbn.cpp $(SRC_DIR)timer.cpp $(SRC_DIR)rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

rdtsend_tcp: $(SRC_DIR)rdtsend_tcp.cpp $(SRC_DIR)timer.cpp $(SRC_DIR)rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

rdtrecv_tcp: $(SRC_DIR)rdtrecv_tcp.cpp $(SRC_DIR)timer.cpp $(SRC_DIR)rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

.PHONY: cleantest, clean

cleantest:
	del $(BIN_DIR)test_*.exe

clean:
	del $(BIN_DIR)*.exe
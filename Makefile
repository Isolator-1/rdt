CC = g++
CC_FLAGS = -O2
LIB = -lws2_32

TEST_DIR = test/
BIN_DIR = bin/

test_rdt: $(TEST_DIR)test_rdt.cpp rdt.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_timer: $(TEST_DIR)test_timer.cpp timer.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_io: $(TEST_DIR)test_io.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

test_cq: $(TEST_DIR)test_cq.cpp
	$(CC) $(CC_FLAGS) $^ -o $(BIN_DIR)$@
	.\$(BIN_DIR)$@.exe

rdtsend_gbn: rdtsend_gbn.cpp timer.cpp rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

rdtrecv_gbn: rdtrecv_gbn.cpp rdt.cpp
	$(CC) $(CC_FLAGS) $^ $(LIB) -o $(BIN_DIR)$@

.PHONY: cleantest, clean

cleantest:
	del $(BIN_DIR)test_*.exe

clean:
	del $(BIN_DIR)*.exe
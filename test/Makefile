CXX ?= clang++
CXXFLAGS := -std=gnu++17 -Wall -Wextra -Werror -I. -I../components/ancs
SRC := test_ancs_protocol.cpp ../components/ancs/ancs_protocol.cpp
BIN := build/test_ancs

.PHONY: test clean
test: $(BIN)
	./$(BIN)

$(BIN): $(SRC) doctest.h ../components/ancs/ancs_protocol.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $(BIN) $(SRC)

clean:
	rm -rf build

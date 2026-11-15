CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Wpedantic -O2 -Iinclude
LDLIBS = -lsqlite3 -lcrypto
SOURCES = src/main.cpp src/App.cpp src/Database.cpp
all: movie_booking
movie_booking: $(SOURCES) include/App.h include/Database.h
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@ $(LDLIBS)
run: movie_booking
	mkdir -p data
	./movie_booking
test: movie_booking
	python3 tests/test_app.py
	python3 tests/test_recovery.py
	python3 tests/test_enhancements.py
clean:
	rm -f movie_booking
.PHONY: all run test clean

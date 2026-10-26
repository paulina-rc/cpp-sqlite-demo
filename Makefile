# =============================================================================
#  Makefile — compilación con g++ (MinGW / MSYS2 en Windows, o Linux/macOS)
#
#  Uso:
#     mingw32-make          compila   (en Linux/macOS: make)
#     mingw32-make run      compila y ejecuta el servidor
#     mingw32-make clean    borra los archivos compilados
#
#  SQLite (sqlite3.c, ~9 MB de código C) se compila UNA sola vez a
#  build/sqlite3.o; las siguientes compilaciones solo recompilan lo que cambió.
# =============================================================================

CXX      = g++
CC       = gcc
CXXFLAGS = -std=c++17 -O2 -Wall -Isrc -Ithird_party/httplib -Ithird_party/json -Ithird_party/sqlite
CFLAGS   = -O2 -DSQLITE_THREADSAFE=1

ifeq ($(OS),Windows_NT)
    EXE      = build/servidor.exe
    CXXFLAGS += -D_WIN32_WINNT=0x0A00
    LDLIBS   = -lws2_32 -static
else
    EXE      = build/servidor
    LDLIBS   = -lpthread -ldl
endif

OBJS = build/main.o build/database.o build/sqlite3.o

all: $(EXE)

$(EXE): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDLIBS)

build/main.o: src/main.cpp src/database.h src/estudiante.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/database.o: src/database.cpp src/database.h src/estudiante.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/sqlite3.o: third_party/sqlite/sqlite3.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir build

run: $(EXE)
	./$(EXE)

clean:
	rm -rf build

.PHONY: all run clean

CC=g++
CFLAGS=-Wall -g
TARGET=webserv

all: $(TARGET)

$(TARGET): webserv.cpp
	$(CC) $(CFLAGS) webserv.cpp -o $(TARGET)

clean:
	rm -f $(TARGET)
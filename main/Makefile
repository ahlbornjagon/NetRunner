CC = gcc
CFLAGS = -Wall -Wextra -g
INCLUDES = -I../include -I. -I./protobuf
LIBS = -lm

TARGET = rec
SOURCES = receiver/receiver.c protobuf/uart.pb.c protobuf/pb_common.c protobuf/pb_decode.c protobuf/pb_encode.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
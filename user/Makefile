CC = gcc
CFLAGS = -O2 -Wall
INCLUDE = -I../kernel

TARGET = user_program
SRC = user_program.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDE) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

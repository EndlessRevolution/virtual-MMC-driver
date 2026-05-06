CC = gcc
CFLAGS = -O2 -Wall -Werror --pedantic

TARGET = user_program
SRC = user_program.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

CC=g++
CFLAGS = -g -lpthread -lrt
SRC = $(wildcard *.cpp)
OBJ = $(patsubst %cpp, %o, $(SRC))
BIN = inofity
all:$(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(CFLAGS)
 
%.o:%.cpp
	$(CC) $(STD) $(CFLAGS) -c $< -o $@
clean:
	rm  $(BIN) $(OBJ)

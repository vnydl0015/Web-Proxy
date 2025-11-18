EXE=htproxy

%.o: %.c
	gcc -O3 -Wall -Wextra -Werror -std=c11 -c $<

$(EXE): main.o sockets.o dataStruct.o
	gcc -O3 -Wall -o $(EXE) $^

clean:
	rm -f $(EXE) *.o

format:
	clang-format -style=file -i *.c *.h

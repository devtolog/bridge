TARGET = calc_compiler
OBJS = main.o parse.tab.o lex.yy.o
CC = clang
LLVM_FLAGS = $(shell llvm-config --cflags --ldflags --libs core)

$(TARGET): $(OBJS)
	$(CC) -lgc $(OBJS) $(LLVM_FLAGS) -o $(TARGET)
	
%.o: %.c
	$(CC) -c $< $(shell llvm-config --cflags) -o $@

parse.tab.c parse.tab.h: parse.y
	bison -d parse.y

lex.yy.c: lex.l parse.tab.h
	flex lex.l

clean:
	rm -f $(TARGET) *.o lex.yy.c parse.tab.c parse.tab.h out.ll
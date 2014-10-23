SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
#CC = gcc
INCLUDES = -I ./lib
LIBS = -L ./lib
#CCFLAGS = -g -Wall -O0
CCFLAGS = -Wall -O0
LDFLAGS = -lpthread -lpcap

ismartwlanpcap : $(OBJS)
	$(CC) $^ -o $@ $(INCLUDES) $(LIBS) $(LDFLAGS)

%.o : %.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm -rf *.o
	rm -rf ismartwlanpcap
.PHONY:clean



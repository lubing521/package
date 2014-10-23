SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
INCLUDES = -I ./include
LIBS = -L ./lib
#CCFLAGS = -g -Wall -O0
CCFLAGS = -Wall
#LDFLAGS = -lpthread
SENDOBJS = Sender.o
RECVOBJS = Recver.o
TARGET = Sender Recver


all:$(TARGET)
	

Sender:$(SENDOBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

Recver:$(RECVOBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) -c $< $(CCFLAGS)
clean:
	rm -rf *.o
	rm -rf Sender Recver
.PHONY:clean



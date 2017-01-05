CC = gcc
LDD = glibtool --tag=CC --mode=link gcc
OBJS = suunto_csv.o
LIBS = ../libdivecomputer/src/.libs/libdivecomputer.a

all: suunto_csv

suunto_csv: ${OBJS}
	${LDD} -o suunto_csv ${OBJS} ${LIBS}

suunto_csv.o: suunto_csv.c
	${CC} -c suunto_csv.c

clean:
	rm -f *.o
	rm suunto_csv

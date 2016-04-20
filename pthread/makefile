CWD = $(shell pwd)

CFLAGS = -Wall -Wextra -g -I$(CWD)/../ -D_GNU_SOURCE -fpic -msse2 -O2

OBJS = attr.o barrier.o cancel.o cond.o create.o detach.o exit.o   \
       init.o join.o lowlevellock.o misc.o mutex.o np.o once.o   \
       rwlock.o sem.o signal.o specific.o spinlock.o

LIBS = -lgcc_s

PREFIX = /usr/local

all: $(OBJS)
	gcc -shared -o libhpt.so $(OBJS) $(LIBS)

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

install:
	mkdir -p $(PREFIX)/include/hurd
	cp $(CWD)/../hurd/qval.h $(PREFIX)/include/hurd/
	cp $(CWD)/../hurd/list.h $(PREFIX)/include/hurd/
	cp $(CWD)/pthread.h $(PREFIX)/include/
	cp $(CWD)/semaphore.h $(PREFIX)/include/
	cp libhpt.so $(PREFIX)/lib/

uninstall:
	-rm $(PREFIX)/include/hurd/qval.h
	-rm $(PREFIX)/include/hurd/list.h
	-rm $(PREFIX)/include/pthread.h
	-rm $(PREFIX)/include/semaphore.h
	-rm $(PREFIX)/lib/libhpt.so

clean:
	-rm -rf *.o libhpt.so

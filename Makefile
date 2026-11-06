.POSIX:
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Isrc
LDLIBS = -lX11 -lm
SRC = src/main.c src/finance.c src/storage.c

all: invest

invest: $(SRC) src/finance.h src/storage.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

test: tests/test_finance.c tests/test_storage.c src/finance.c src/storage.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-finance tests/test_finance.c src/finance.c -lm
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-storage tests/test_storage.c src/finance.c src/storage.c -lm
	./test-finance
	./test-storage

clean:
	rm -f invest test-finance test-storage

PREFIX = /usr/local

install: invest
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 invest $(DESTDIR)$(PREFIX)/bin/invest
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 invest.desktop $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/invest
	rm -f $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

.POSIX:
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Isrc
LDLIBS = -lX11 -lm
SRC = src/main.c src/finance.c src/storage.c src/budget.c

all: invest

invest: $(SRC) src/finance.h src/storage.h src/budget.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

test: tests/test_finance.c tests/test_storage.c tests/test_budget.c src/finance.c src/storage.c src/budget.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-finance tests/test_finance.c src/finance.c -lm
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-storage tests/test_storage.c src/finance.c src/storage.c -lm
	./test-finance
	./test-storage
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-budget tests/test_budget.c src/budget.c -lm
	./test-budget

clean:
	rm -f invest test-finance test-storage test-budget

PREFIX = /usr/local

install: invest
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 invest $(DESTDIR)$(PREFIX)/bin/invest
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 invest.desktop $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/invest
	rm -f $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

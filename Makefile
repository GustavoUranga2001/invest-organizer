.POSIX:
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Isrc
# cairo is used only for antialiased vector painting; Xlib owns the window.
CAIRO_CFLAGS = $(shell pkg-config --cflags cairo)
CAIRO_LIBS = $(shell pkg-config --libs cairo)
LDLIBS = -lX11 $(CAIRO_LIBS) -lm
SRC = src/main.c src/finance.c src/storage.c src/budget.c src/utf8.c src/compose.c src/monthly.c

all: invest

invest: $(SRC) src/finance.h src/storage.h src/budget.h src/utf8.h src/compose.h src/monthly.h
	$(CC) $(CPPFLAGS) $(CAIRO_CFLAGS) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

test: tests/test_monthly.c src/monthly.c src/monthly.h tests/test_finance.c tests/test_storage.c tests/test_budget.c tests/test_utf8.c tests/test_compose.c src/finance.c src/storage.c src/budget.c src/utf8.c src/compose.c src/monthly.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-finance tests/test_finance.c src/finance.c -lm
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-storage tests/test_storage.c src/finance.c src/storage.c src/utf8.c -lm
	./test-finance
	./test-storage
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-budget tests/test_budget.c src/budget.c src/utf8.c -lm
	./test-budget
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-utf8 tests/test_utf8.c src/utf8.c
	./test-utf8
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-compose tests/test_compose.c src/compose.c src/utf8.c
	./test-compose
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-monthly tests/test_monthly.c src/monthly.c src/budget.c src/utf8.c -lm
	./test-monthly

clean:
	rm -f invest test-finance test-storage test-budget test-utf8 test-compose test-monthly

PREFIX = /usr/local

install: invest
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 invest $(DESTDIR)$(PREFIX)/bin/invest
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 invest.desktop $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/invest
	rm -f $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

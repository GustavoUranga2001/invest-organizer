.POSIX:
CC = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Isrc
# Cairo desenha apenas pixels/fontes; Xlib continua gerenciando a janela.
CAIRO_CFLAGS = $(shell pkg-config --cflags cairo)
CAIRO_LIBS = $(shell pkg-config --libs cairo)
LDLIBS = -lX11 $(CAIRO_LIBS) -lm
SRC = src/main.c src/finance.c src/storage.c src/budget.c src/utf8.c src/compose.c src/monthly.c src/safeio.c src/lock.c src/market.c
OBJ = $(SRC:.c=.o)

all: invest invest-doctor invest-market-check

DOCTOR_SRC = src/doctor.c src/finance.c src/storage.c src/budget.c src/monthly.c src/utf8.c src/safeio.c src/lock.c

invest-doctor: $(DOCTOR_SRC) src/safeio.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(DOCTOR_SRC) -lm

invest-market-check: src/market_check.c src/market.c src/storage.c src/finance.c src/utf8.c src/safeio.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ -lm

# Compilacao incremental: so os modulos alterados sao recompilados.
invest: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/main.o: src/main.c src/lock.h src/finance.h src/storage.h src/budget.h src/monthly.h src/utf8.h src/compose.h src/market.h
	@pkg-config --exists cairo || { echo 'Falta cairo-devel (Void Linux: sudo xbps-install -S cairo-devel pkg-config)' >&2; exit 1; }
	$(CC) $(CPPFLAGS) $(CAIRO_CFLAGS) $(CFLAGS) -c -o $@ $<

src/finance.o: src/finance.c src/finance.h
src/storage.o: src/storage.c src/storage.h src/finance.h src/utf8.h src/safeio.h
src/budget.o: src/budget.c src/budget.h src/utf8.h src/safeio.h
src/utf8.o: src/utf8.c src/utf8.h
src/compose.o: src/compose.c src/compose.h src/utf8.h
src/monthly.o: src/monthly.c src/monthly.h src/budget.h src/utf8.h src/safeio.h
src/safeio.o: src/safeio.c src/safeio.h
src/lock.o: src/lock.c src/lock.h src/storage.h
src/market.o: src/market.c src/market.h src/storage.h src/safeio.h

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

test: invest-market-check
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-market tests/test_market.c src/market.c src/storage.c src/finance.c src/utf8.c src/safeio.c -lm
	./test-market
	./tests/test_market_network.sh
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-finance tests/test_finance.c src/finance.c -lm
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-storage tests/test_storage.c src/finance.c src/storage.c src/utf8.c src/safeio.c -lm
	./test-finance
	./test-storage
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-budget tests/test_budget.c src/budget.c src/utf8.c src/safeio.c -lm
	./test-budget
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-utf8 tests/test_utf8.c src/utf8.c
	./test-utf8
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-compose tests/test_compose.c src/compose.c src/utf8.c
	./test-compose
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-monthly tests/test_monthly.c src/monthly.c src/budget.c src/utf8.c src/safeio.c -lm
	./test-monthly
	$(CC) $(CPPFLAGS) $(CFLAGS) -o test-recovery tests/test_recovery.c src/storage.c src/finance.c src/budget.c src/monthly.c src/utf8.c src/safeio.c -lm
	./test-recovery

clean:
	rm -f invest invest-doctor invest-market-check $(OBJ) test-finance test-storage test-budget test-utf8 test-compose test-monthly test-recovery test-market

PREFIX = /usr/local
install: invest invest-doctor invest-market-check invest-market-check
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 invest $(DESTDIR)$(PREFIX)/bin/invest
	install -m 755 invest-doctor $(DESTDIR)$(PREFIX)/bin/invest-doctor
	install -m 755 invest-market-check $(DESTDIR)$(PREFIX)/bin/invest-market-check
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 invest.desktop $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/invest $(DESTDIR)$(PREFIX)/bin/invest-doctor $(DESTDIR)$(PREFIX)/bin/invest-market-check
	rm -f $(DESTDIR)$(PREFIX)/share/applications/invest.desktop

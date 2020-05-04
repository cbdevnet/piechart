export PREFIX?=$(DESTDIR)/usr/bin
export DOCDIR?=$(DESTDIR)/usr/share/man/man1
.PHONY: clean

CFLAGS=-Wall -g
LDLIBS=-lm

all: svg_header.h piechart.1.gz piechart

piechart: piechart.c

piechart.1.gz:
	gzip -c < piechart.1 > $@

svg_header.h:
	xxd -i svg_header > $@ 

install:
	install -m 0755 piechart "$(PREFIX)"
	install -g 0 -o 0 -m 0644 piechart.1.gz "$(DOCDIR)"

clean:
	rm piechart svg_header.h piechart.1.gz


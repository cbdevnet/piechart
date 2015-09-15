CFLAGS=-Wall -g -lm
.PHONY: clean

piechart: svg_header.h piechart.c

svg_header.h:
	xxd -i svg_header > svg_header.h

clean:
	rm piechart svg_header.h


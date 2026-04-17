all: libsie-reference.html

libsie-reference.html: libsie-reference.mdwn
	(echo '<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN"'	  \
	'"http://www.w3.org/TR/html4/strict.dtd">';			  \
	echo '<html><head><title>libsie reference manual</title></head>'; \
	echo '<body>';							  \
	markdown < $<;							  \
	echo '</body></html>') > $@

libsie-reference.mdwn: ../../include/sie.h h2mdwn.pl
	grep -v 'SIE_H_INCLUDED' $< | grep -v '^#include' | perl h2mdwn.pl > $@

../../include/sie.h: ../../include/sie.h.in
	cd ../.. && $(MAKE) include/sie.h

clean::
	rm -f *.mdwn *.html

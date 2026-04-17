all:: sie-format.pdf core-schema.pdf somat-schema.pdf

%.pdf: %.tex
	pdflatex $<
	pdflatex $<

sie-format.pdf: sie-format.tex bibliography.bib
	pdflatex $<
	bibtex sie-format
	pdflatex $<
	pdflatex $<

sa:: sie-format.pdf
	acroread $<

scs:: core-schema.pdf
	acroread $<

sss:: somat-schema.pdf
	acroread $<

clean:
	rm -f *.aux *.out *.toc *.dvi *.pdf *.log *.nav *.snm *.bbl *.blg

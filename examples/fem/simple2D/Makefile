CHARMC=../../../bin/charmc $(OPTS) 

all: pgm

pgm: pgm.o cst_nl.o
	$(CHARMC) -o pgm pgm.o cst_nl.o -language fem -module netfem 

pgm.o: pgm.C
	$(CHARMC) -c pgm.C

cst_nl.o: cst_nl.C
	$(CHARMC) -c cst_nl.C


test: pgm
	./charmrun ./pgm +vp4 +p2 $(TESTOPTS)

bgtest: pgm
	./charmrun ./pgm +vp4 +p2 +x2 +y2 +z1 $(TESTOPTS)

clean:
	rm -f pgm fpgm *.o conv-host charmrun charmrun.exe pgm.exe pgm.pdb pgm.ilk

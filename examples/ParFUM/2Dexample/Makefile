CHARMC=../../../bin/charmc $(OPTS) 

all: pgm

pgm: pgm.o	
	$(CHARMC) -o 2dex pgm.o -language ParFUM -module netfem #-memory paranoid

pgm.o: pgm.C
	$(CHARMC) -I../../../src/libs/ck-libs/ParFUM -c pgm.C

test: pgm
	./charmrun ./2dex +vp4 +p2

bgtest: pgm
	./charmrun ./2dex +vp4 +p2 +x2 +y2 +z1

clean:
	rm -rf pgm 2dex fpgm *.o conv-host charmrun NetFEM Paraview* ParaView* *~ pvClient.out README

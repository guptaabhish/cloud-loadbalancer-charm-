CXX = ../../../bin/charmc
CXXFLAGS = -g -O3 -Wall

SRC = $(wildcard *.cc)
SRCCI = $(wildcard *.ci)
OBJ = $(SRC:.cc=.o)
EXEC_NAME = comp.x
DECLS := $(SRCCI:.ci=.decl.h)
DEFS := $(SRCCI:.ci=.def.h)

all: $(EXEC_NAME)

.PHONY: clean

clean:
	-rm $(OBJ) $(EXEC_NAME) $(DECLS) $(DEFS) charmrun

$(SRC): $(DECLS)

$(EXEC_NAME): $(OBJ)
	$(CXX) -o $@ $^ -module completion

%.decl.h: %.ci
	$(CXX) -c $*.ci

comp.o: comp.cc $(DECLS)
	$(CXX) $(CXXFLAGS) -c $*.cc -o $*.o
#ifndef _MPI_INTEROPERATE_
#define _MPI_INTEROPERATE_

#include "converse.h"
#include "ck.h"
#include "trace.h"
#include <mpi.h>

void CharmLibInit(MPI_Comm userComm, int argc, char **argv);
void CharmLibExit();

extern "C" void LibCkExit(void);

extern int _ringexit;		    // for charm exit
extern int _ringtoken;
extern MPI_Comm charmComm;
extern void _initCharm(int unused_argc, char **argv);
extern void CkExit(void);

#define CkExit LibCkExit

#endif //_MPI_INTEROPERATE_

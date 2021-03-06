/****************************************************************
           Benchmark tests the performance of PINGPONG with 
           oneway and two way traffic. It reports both completion 
           time and COU overhead. The CPU overhead is computed from 
           the idle time, and hence can be inaccurate on some machine layers.

   - Sameer Kumar (03/08/05)
*******************************************************************/


#include <stdlib.h>
#include <converse.h>

//Iterations for each message size
enum {nCycles = 2000};

//Max message size
enum { maxMsgSize = 1 << 18 };

CpvDeclare(int,msgSize);
CpvDeclare(int,cycleNum);
CpvDeclare(int,sizeNum);
CpvDeclare(int,exitHandler);
CpvDeclare(int,node0Handler);
CpvDeclare(int,node1Handler);
CpvStaticDeclare(double,startTime);
CpvStaticDeclare(double,endTime);

CpvDeclare(double, IdleStartTime);
CpvDeclare(double, IdleTime);

//Registering idle handlers
void ApplIdleStart(void *, double start)
{
    CpvAccess(IdleStartTime)= start; //CmiWallTimer();
    return;
}

void ApplIdleEnd(void *, double cur)
{
  if(CpvAccess(IdleStartTime) < 0)
      return;
  
  CpvAccess(IdleTime) += cur /*CmiWallTimer()*/-CpvAccess(IdleStartTime);
  CpvAccess(IdleStartTime)=-1;
  return;
}

//Start ping pong

void startPingpong()
{
    //CmiBarrier();
    CpvAccess(cycleNum) = 0;
    CpvAccess(msgSize) = (CpvAccess(msgSize)-CmiMsgHeaderSizeBytes)*2 + 
        CmiMsgHeaderSizeBytes;

    char *msg = (char *)CmiAlloc(CpvAccess(msgSize));
    *((int *)(msg+CmiMsgHeaderSizeBytes)) = CpvAccess(msgSize);
  
    CmiSetHandler(msg,CpvAccess(node0Handler));
    CmiSyncSendAndFree(CmiMyPe(), CpvAccess(msgSize), msg);
    
    CpvAccess(startTime) = CmiWallTimer();
    CpvAccess(IdleTime) = 0.0;
}

void pingpongFinished(char *msg)
{
    CmiFree(msg);
    double cycle_time = 
        (1e6*(CpvAccess(endTime)-CpvAccess(startTime)))/(2.*nCycles);
    double compute_time = cycle_time - 
        (1e6*(CpvAccess(IdleTime)))/(2.*nCycles);
    
    CmiPrintf("[%d] %d \t %5.3lfus \t %5.3lfus\n", CmiMyPe(),
              CpvAccess(msgSize) - CmiMsgHeaderSizeBytes, 
              cycle_time, compute_time);
    
    CpvAccess(sizeNum)++;
    
    if (CpvAccess(msgSize) < maxMsgSize)
        startPingpong();
    else {
        void *sendmsg = CmiAlloc(CmiMsgHeaderSizeBytes);
        CmiSetHandler(sendmsg,CpvAccess(exitHandler));
        CmiSyncBroadcastAllAndFree(CmiMsgHeaderSizeBytes,sendmsg);
    }
}

CmiHandler exitHandlerFunc(char *msg)
{
    CmiFree(msg);
    CsdExitScheduler();
    return 0;
}

CmiHandler node0HandlerFunc(char *msg)
{
    CpvAccess(cycleNum)++;
    
    if (CpvAccess(cycleNum) == nCycles) {
        CpvAccess(endTime) = CmiWallTimer();
        pingpongFinished(msg);
    }
    else {
        CmiSetHandler(msg,CpvAccess(node1Handler));
        *((int *)(msg+CmiMsgHeaderSizeBytes)) = CpvAccess(msgSize);
        
        int dest = CmiNumPes() - CmiMyPe() - 1;
        CmiSyncSendAndFree(dest,CpvAccess(msgSize),msg);
    }
    
    return 0;
}

CmiHandler node1HandlerFunc(char *msg)
{
    CpvAccess(msgSize) = *((int *)(msg+CmiMsgHeaderSizeBytes));
    CmiSetHandler(msg,CpvAccess(node0Handler));
    
    int dest = CmiNumPes() - CmiMyPe() - 1;
    CmiSyncSendAndFree(dest,CpvAccess(msgSize),msg);
    return 0;
}

CmiStartFn mymain(int argc, char** argv)
{
    CpvInitialize(int,msgSize);
    CpvInitialize(int,cycleNum);
    CpvInitialize(int,sizeNum);
    CpvAccess(sizeNum) = 1;
    CpvAccess(msgSize)= CmiMsgHeaderSizeBytes + 8;
    
    CpvInitialize(int,exitHandler);
    CpvAccess(exitHandler) = CmiRegisterHandler((CmiHandler) exitHandlerFunc);
    CpvInitialize(int,node0Handler);
    CpvAccess(node0Handler) = CmiRegisterHandler((CmiHandler) node0HandlerFunc);
    CpvInitialize(int,node1Handler);
    CpvAccess(node1Handler) = CmiRegisterHandler((CmiHandler) node1HandlerFunc);
    
    CpvInitialize(double,startTime);
    CpvInitialize(double,endTime);
    
    int otherPe = CmiMyPe() ^ 1;
    
    CcdCallOnConditionKeep(CcdPROCESSOR_BEGIN_IDLE, ApplIdleStart, NULL);
    CcdCallOnConditionKeep(CcdPROCESSOR_END_IDLE, ApplIdleEnd, NULL);
    
    int twoway = 0;
    
    if(argc > 1)
        twoway = atoi(argv[1]);

    if(CmiMyPe() == 0) {
        if(!twoway)
            CmiPrintf("Starting Pingpong with oneway traffic \n");
        else
            CmiPrintf("Starting Pingpong with twoway traffic\n");
        
        if ((CmiMyPe() < CmiNumPes()/2) || twoway)
            startPingpong();
    }

    return 0;
}

int main(int argc,char *argv[])
{
    ConverseInit(argc,argv,(CmiStartFn)mymain,0,0);
    return 0;
}

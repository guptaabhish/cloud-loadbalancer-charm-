#ifndef _CONV_MACH_H
#define _CONV_MACH_H

// specify the version of the UNIX APIs that we want to use (for ucontext headers)
#define _XOPEN_SOURCE                                       

#define CMK_AMD64                                          1
#define CMK_64BIT    1
#define CMK_CONVERSE_MPI                                   1

#define CMK_DEFAULT_MAIN_USES_COMMON_CODE                  1
#define CMK_NOT_USE_TLS_THREAD                             1

#define CMK_ASYNC_NOT_NEEDED                               0
#define CMK_ASYNC_USE_FIOASYNC_AND_FIOSETOWN               0
#define CMK_ASYNC_USE_FIOASYNC_AND_SIOCSPGRP               0
#define CMK_ASYNC_USE_FIOSSAIOSTAT_AND_FIOSSAIOOWN         0
#define CMK_ASYNC_USE_F_SETFL_AND_F_SETOWN                 1

#define CMK_GETPAGESIZE_AVAILABLE                          0

#define CMK_MALLOC_USE_GNU_MALLOC                          0
#define CMK_MALLOC_USE_OS_BUILTIN                          1
#define CMK_MALLOC_USE_GNUOLD_MALLOC                       0

#define CMK_MEMORY_BUILD_GNU_HOOKS                         0
#define CMK_MEMORY_PAGESIZE                                4096
#define CMK_MEMORY_PROTECTABLE                             0

#define CMK_NODE_QUEUE_AVAILABLE                           0

#undef CMK_RSH_IS_A_COMMAND                              
#define CMK_RSH_IS_A_COMMAND                               1
#undef CMK_RSH_NOT_NEEDED                               
#define CMK_RSH_NOT_NEEDED                                 0
#undef CMK_RSH_USE_REMSH                               
#define CMK_RSH_USE_REMSH                                  0

#define CMK_SHARED_VARS_UNAVAILABLE                        1
#define CMK_SHARED_VARS_UNIPROCESSOR                       0
#define CMK_SHARED_VARS_POSIX_THREADS_SMP                  0

#define CMK_THREADS_USE_PTHREADS                           0
#define CMK_THREADS_ARE_WIN32_FIBERS                       0
#define CMK_THREADS_USE_CONTEXT                            0
#define CMK_THREADS_USE_JCONTEXT                           1

#define CMK_SIGNAL_NOT_NEEDED                              0
#define CMK_SIGNAL_USE_SIGACTION                           0
#define CMK_SIGNAL_USE_SIGACTION_WITH_RESTART              1

#define CMK_THREADS_REQUIRE_NO_CPV                         0
#define CMK_THREADS_COPY_STACK                             0

#define CMK_TIMER_USE_GETRUSAGE                            1
#define CMK_TIMER_USE_SPECIAL                              0
#define CMK_TIMER_USE_TIMES                                0

#define CMK_TYPEDEF_INT2 short
#define CMK_TYPEDEF_INT4 int
#define CMK_TYPEDEF_INT8 long long
#define CMK_TYPEDEF_UINT2 unsigned short
#define CMK_TYPEDEF_UINT4 unsigned int
#define CMK_TYPEDEF_UINT8 unsigned long long
#define CMK_TYPEDEF_FLOAT4 float
#define CMK_TYPEDEF_FLOAT8 double

#define CMK_DEBUG_MODE					   0 
#define CMK_WEB_MODE                                       1

#define CMK_LBDB_ON					   1

#undef CMK_STACKSIZE_DEFAULT				 
#define CMK_STACKSIZE_DEFAULT				   262144

//#define CMK_NO_ISO_MALLOC				   1

#define CMK_WHEN_PROCESSOR_IDLE_BUSYWAIT                   1
#define CMK_WHEN_PROCESSOR_IDLE_USLEEP                     0

/* Mac-specific optimizations */
#undef CMK_USE_POLL
#define CMK_USE_POLL                                       0
#define CMK_USE_KQUEUE                                     1
#define PXSHM_OSSPINLOCK                                   1
#define PXSHM_LOCK                                         0
#define PXSHM_FENCE                                        0


#endif


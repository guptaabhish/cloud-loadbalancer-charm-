#define CMK_SMP						   1

#undef CMK_MALLOC_USE_GNU_MALLOC
#undef CMK_MALLOC_USE_OS_BUILTIN
#define CMK_MALLOC_USE_GNU_MALLOC                          0
#define CMK_MALLOC_USE_OS_BUILTIN                          1

#undef CMK_MEMORY_PROTECTABLE
#define CMK_MEMORY_PROTECTABLE                             0

#undef CMK_NODE_QUEUE_AVAILABLE
#define CMK_NODE_QUEUE_AVAILABLE                           1

#undef CMK_SHARED_VARS_UNAVAILABLE
#undef CMK_SHARED_VARS_POSIX_THREADS_SMP
#define CMK_SHARED_VARS_UNAVAILABLE                        0
#define CMK_SHARED_VARS_POSIX_THREADS_SMP                  1

#undef CMK_CCS_AVAILABLE
#define CMK_CCS_AVAILABLE                                  0

#undef CMK_TIMER_USE_SPECIAL
#undef CMK_TIMER_USE_GETRUSAGE
#define CMK_TIMER_USE_SPECIAL				   0
#define CMK_TIMER_USE_GETRUSAGE				   1

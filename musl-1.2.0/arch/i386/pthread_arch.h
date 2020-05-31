#include "syscall.h"

extern struct pthread mainThread;

static inline struct pthread *__pthread_self()
{
	struct pthread *self;
	//__asm__ ("movl %%gs:0,%0" : "=r" (self) );
	self = &mainThread;
	//TODO: proper Thread Local Storage
	return self;
}

#define TP_ADJ(p) (p)

#define MC_PC gregs[REG_EIP]

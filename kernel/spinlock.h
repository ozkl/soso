#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "common.h"

typedef int32_t Spinlock;

void Spinlock_Init(Spinlock* spinlock);
void Spinlock_Lock(Spinlock* spinlock);
BOOL Spinlock_TryLock(Spinlock* spinlock);
void Spinlock_Unlock(Spinlock* spinlock);

#endif // SPINLOCK_H

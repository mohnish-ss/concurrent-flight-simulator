#include "flight_sim.h"

#include <stdlib.h>

bool sim_lock_init(SimLock *lock)
{
#ifdef __APPLE__
    return pthread_mutex_init(&lock->native, NULL) == 0;
#else
    return sem_init(&lock->native, 0, 1) == 0;
#endif
}

void sim_lock_acquire(SimLock *lock)
{
#ifdef __APPLE__
    if (pthread_mutex_lock(&lock->native) != 0)
#else
    if (sem_wait(&lock->native) != 0)
#endif
    {
        abort();
    }
}

void sim_lock_release(SimLock *lock)
{
#ifdef __APPLE__
    if (pthread_mutex_unlock(&lock->native) != 0)
#else
    if (sem_post(&lock->native) != 0)
#endif
    {
        abort();
    }
}

void sim_lock_destroy(SimLock *lock)
{
#ifdef __APPLE__
    (void)pthread_mutex_destroy(&lock->native);
#else
    (void)sem_destroy(&lock->native);
#endif
}

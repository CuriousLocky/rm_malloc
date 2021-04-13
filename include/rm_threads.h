#ifndef RM_THREADS_H
#define RM_THREADS_H

#include <stdbool.h>

#ifdef __GNUC__
#define tls __thread

/* rm_lock_t is a spinlock implementation without any malloc usage*/
#define rm_lock_t volatile bool 
#define RM_LOCKED true
#define RM_UNLOCKED false
#define RM_LOCK_INITIALIZER RM_UNLOCKED
inline void rm_lock_init(rm_lock_t *lock){
    *lock = RM_UNLOCKED;
}
inline void rm_lock(rm_lock_t *lock){
    do{
        while(*lock == RM_LOCKED){
            ;
        }
    }while(__sync_lock_test_and_set(lock, RM_LOCKED)==RM_LOCKED);
}

inline void rm_unlock(rm_lock_t *lock){
    *lock = RM_UNLOCKED;
}

/*returns RM_LOCKED when succeed, RM_UNLCOKED otherwise*/
inline bool rm_trylock(rm_lock_t *lock){
    return !(__sync_lock_test_and_set(lock, RM_LOCKED));
}

#define rm_once_t volatile bool
#define RM_ONCE_INITIALIZER false
#define RM_HAS_RUN true
inline void rm_callonce(rm_once_t *once_flag, void (*func)(void)){
    if(*once_flag == RM_HAS_RUN){
        return;
    }
    if(__sync_lock_test_and_set(once_flag, RM_HAS_RUN)==RM_ONCE_INITIALIZER){
        func();
    }
}

#endif

#endif
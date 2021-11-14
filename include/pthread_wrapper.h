#include <pthread.h>

// a modified pthead_create to insure init and destructor are executed
int rm_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
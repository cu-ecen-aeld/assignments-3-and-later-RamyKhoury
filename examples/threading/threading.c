#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg, ...)
// #define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg, ...) printf("threading ERROR: " msg "\n", ##__VA_ARGS__)

void sleep_ms(long milliseconds)
{
    struct timespec ts;

    // Calculate whole seconds
    ts.tv_sec = milliseconds / 1000;

    // Convert remaining milliseconds into nanoseconds (1 ms = 1,000,000 ns)
    ts.tv_nsec = (milliseconds % 1000) * 1000000;

    // Execute sleep
    nanosleep(&ts, NULL);
}

void *threadfunc(void *thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    // struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    sleep_ms(((struct thread_data *)thread_param)->time_to_obtain);
    if (pthread_mutex_lock(((struct thread_data *)thread_param)->mutex))
    {
        ((struct thread_data *)thread_param)->thread_complete_success = false;
        return thread_param;
    }
    sleep_ms(((struct thread_data *)thread_param)->time_to_release);
    pthread_mutex_unlock(((struct thread_data *)thread_param)->mutex);
    ((struct thread_data *)thread_param)->thread_complete_success = true;
    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *thread_d = malloc(sizeof(struct thread_data));
    thread_d->mutex = mutex;
    thread_d->time_to_obtain = wait_to_obtain_ms;
    thread_d->time_to_release = wait_to_release_ms;
    thread_d->thread_complete_success = false;
    int result = pthread_create(thread,
                                NULL,
                                threadfunc,
                                (void *)thread_d);
    if (result)
    {
        ERROR_LOG("Error while creating thread.");
        return false;
    }
    return true;
}

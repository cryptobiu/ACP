
#ifdef __APPLE__

#include <unistd.h>
#include <pthread.h>

int pthread_mutex_timedlock(pthread_mutex_t * lock, struct timespec abs_to)
{
	int result;
	struct timespec now;
	do
	{
		if(0 == (result = pthread_mutex_trylock(lock)))
			break;
		usleep(1);
		clock_gettime(CLOCK_REALTIME, &now);
	} while(now.tv_sec < abs_to.tv_sec || (now.tv_sec == abs_to.tv_sec && now.tv_nsec < abs_to.tv_nsec));
	return result;
}


#endif //__APPLE__


#ifdef __APPLE__

int pthread_mutex_timedlock(pthread_mutex_t * lock, struct timespec * abs_to);

ssize_t splice(int fd_in, off_t * off_in, int fd_out, off_t * off_out, size_t len, unsigned int flags);

int pthread_timedjoin_np(pthread_t thread, void **retval, const struct timespec *abstime);

#endif //__APPLE__

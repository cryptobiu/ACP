
#ifdef __APPLE__

int pthread_mutex_timedlock(pthread_mutex_t * lock, struct timespec * abs_to);

ssize_t splice(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags);

#endif //__APPLE__


#pragma once

template <class T>
class lfq
{
	size_t m_qsize, m_nread, m_nwrite;
	sem_t m_sread, m_swrite;
	T * m_q;
public:
	lfq(const size_t qsize);
	~lfq();

	int push(const T & t, const size_t to_ms = 0);
	int pop(T &, const size_t to_ms = 0);
};

template <class T>
lfq<T>::lfq(const size_t qsize)
{
	m_q = new T[m_qsize = qsize];
	m_nread = m_nwrite = 0;
	sem_init(&m_sread, 0, 0);
	sem_init(&m_swrite, 0, qsize);
}

template <class T>
lfq<T>::~lfq()
{
	sem_destroy(&m_sread);
	sem_destroy(&m_swrite);
	delete m_q;
}

template <class T>
int lfq<T>::push(const T & t, const size_t to_ms)
{
	int result = -1;

	bool write_ready = false;
	if(0 == to_ms)
	{
		 write_ready = (0 == sem_wait(&m_swrite))? true: false;
	}
	else
	{
		struct timespec abs_timeout;
		clock_gettime(CLOCK_REALTIME, &abs_timeout);
		abs_timeout.tv_nsec += (1000000 * to_ms);
		abs_timeout.tv_sec += (abs_timeout.tv_nsec / 1000000000);
		abs_timeout.tv_nsec %= 1000000000;
		write_ready = (0 == sem_timedwait(&m_swrite, &abs_timeout))? true: false;
	}

	if(write_ready)
	{
		m_q[m_nwrite] = t;
		m_nwrite = (m_nwrite + 1)%m_qsize;
		sem_post(&m_sread);
		result = 0;
	}

	return result;
}

template <class T>
int lfq<T>::pop(T & t, const size_t to_ms)
{
	int result = -1;

	bool read_ready = false;
	if(0 == to_ms)
	{
		read_ready = (0 == sem_wait(&m_sread))? true: false;
	}
	else
	{
		struct timespec abs_timeout;
		clock_gettime(CLOCK_REALTIME, &abs_timeout);
		abs_timeout.tv_nsec += (1000000 * to_ms);
		abs_timeout.tv_sec += (abs_timeout.tv_nsec / 1000000000);
		abs_timeout.tv_nsec %= 1000000000;
		read_ready = (0 == sem_timedwait(&m_sread, &abs_timeout))? true: false;
	}

	if(read_ready)
	{
		t = m_q[m_nread];
		m_nread = (m_nread + 1)%m_qsize;
		sem_post(&m_swrite);
		result = 0;
	}

	return result;

}

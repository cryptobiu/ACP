
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <memory.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/uio.h>

#ifdef __ANDROID__

#include <pthread.h>

#include <android/log.h>

#define lc_fatal(...) __android_log_print(ANDROID_LOG_FATAL,m_logcat.c_str(),__VA_ARGS__)
#define lc_error(...) __android_log_print(ANDROID_LOG_ERROR,m_logcat.c_str(),__VA_ARGS__)
#define lc_warn(...) __android_log_print(ANDROID_LOG_WARN,m_logcat.c_str(),__VA_ARGS__)
#define lc_notice(...) __android_log_print(ANDROID_LOG_INFO,m_logcat.c_str(),__VA_ARGS__)
#define lc_info(...) __android_log_print(ANDROID_LOG_INFO,m_logcat.c_str(),__VA_ARGS__)
#define lc_debug(...) __android_log_print(ANDROID_LOG_DEBUG,m_logcat.c_str(),__VA_ARGS__)

#else

#include <log4cpp/Category.hh>

#define lc_fatal(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_error(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_warn(...) log4cpp::Category::getInstance(m_logcat).warn(__VA_ARGS__)
#define lc_notice(...) log4cpp::Category::getInstance(m_logcat).notice(__VA_ARGS__)
#define lc_info(...) log4cpp::Category::getInstance(m_logcat).info(__VA_ARGS__)
#define lc_debug(...) log4cpp::Category::getInstance(m_logcat).debug(__VA_ARGS__)

#endif

#include "comm_client.h"
#include "comm_client_cb_api.h"

void * comm_client_proc(void * arg)
{
	comm_client * client = (comm_client *)arg;
	client->run();
	return NULL;
}

comm_client::comm_client(cc_args_t * cc_args)
: m_logcat(cc_args->logcat), m_id((unsigned int)-1), m_peer_count(0), m_sink(NULL), m_runner(0)
{
	sem_init(&m_run_flag, 0, 0);
}

comm_client::~comm_client()
{
	sem_destroy(&m_run_flag);
}

int comm_client::start(const unsigned int id, const unsigned int peer_count, const char * comm_conf_file, comm_client_cb_api * sink)
{
	if(id >= peer_count)
	{
		lc_error("%s: invalid id/parties values %u/%u", __FUNCTION__, id, peer_count);
		return -1;
	}

	if(get_run_flag())
	{
		lc_error("%s: this comm client is already started", __FUNCTION__);
		return -1;
	}
	set_run_flag(true);

	m_id = id;
	m_peer_count = peer_count;
	m_comm_conf_file = comm_conf_file;
	m_sink = sink;

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	lc_notice("%s: started %lu.%03lu", __FUNCTION__, ts.tv_sec, ts.tv_nsec/1000000);

	return launch();
}

int comm_client::launch()
{
	int result = pthread_create(&m_runner, NULL, comm_client_proc, this);
	if(0 != result)
	{
		char errmsg[512];
		lc_error("%s: pthread_create() failed with error %d : %s", __FUNCTION__, result, strerror_r(result, errmsg, 512));
		set_run_flag(false);
		return -1;
	}
	return 0;
}

void comm_client::stop()
{
	if(!get_run_flag())
	{
		lc_error("%s: this comm client is not running.", __FUNCTION__);
		return;
	}
	set_run_flag(false);

    void * return_code = NULL;
#ifdef __ANDROID__
    int result = pthread_join(m_runner, &return_code);
	if(0 != result)
	{
		char errmsg[512];
		lc_error("%s: pthread_join() failed with error %d : %s", __FUNCTION__, result, strerror_r(result, errmsg, 512));
        pthread_kill(m_runner, SIGABRT);
	}
#else
	struct timespec timeout;
	clock_gettime(CLOCK_REALTIME, &timeout);
	timeout.tv_sec += 5;

	int result = pthread_timedjoin_np(m_runner, &return_code, &timeout);
	if(0 != result)
	{
		char errmsg[512];
		lc_error("%s: pthread_timedjoin_np() failed with error %d : %s", __FUNCTION__, result, strerror_r(result, errmsg, 512));

		result = pthread_cancel(m_runner);
		if(0 != result)
		{
			char errmsg[512];
			lc_error("%s: pthread_cancel() failed with error %d : %s", __FUNCTION__, result, strerror_r(result, errmsg, 512));
		}
	}
#endif

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	lc_notice("%s: stopped %lu.%03lu", __FUNCTION__, ts.tv_sec, ts.tv_nsec/1000000);

	m_id = (unsigned int)-1;
	m_comm_conf_file.clear();
	m_sink = NULL;
}

bool comm_client::get_run_flag()
{
	int val = 0;
	if(0 != sem_getvalue(&m_run_flag, &val))
	{
        int errcode = errno;
        char errmsg[256];
        lc_fatal("%s: sem_getvalue() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(-__LINE__);
	}
	return (0 == val)? false: true;
}

void comm_client::set_run_flag(bool raise)
{
	bool up = get_run_flag();
	if(up && !raise)
	{
		if(0 != sem_wait(&m_run_flag))
		{
	        int errcode = errno;
	        char errmsg[256];
	        lc_fatal("%s: sem_wait() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	        exit(-__LINE__);
		}
	}
	else if(!up && raise)
	{
		if(0 != sem_post(&m_run_flag))
		{
	        int errcode = errno;
	        char errmsg[256];
	        lc_fatal("%s: sem_post() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
	        exit(-__LINE__);
		}
	}
}

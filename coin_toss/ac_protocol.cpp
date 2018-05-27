
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <string>
#include <vector>
#include <list>

#include "comm_client_cb_api.h"
#include "ac_protocol.h"
#include "comm_client.h"
#include "comm_client_tcp_mesh.h"

ac_protocol::ac_protocol(const char * log_category)
: m_logcat(log_category), m_cc(new comm_client_tcp_mesh(((m_logcat + '.') + "ctm").c_str())), m_id(-1), m_parties(0), m_rounds(0), m_run_flag(false)
{
	int errcode = 0;
	if(0 != (errcode = pthread_mutex_init(&m_q_lock, NULL)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_mutex_init() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
	if(0 != (errcode = pthread_mutex_init(&m_e_lock, NULL)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_mutex_init() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
	if(0 != (errcode = pthread_cond_init(&m_comm_e, NULL)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_cond_init() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
}

ac_protocol::~ac_protocol()
{
	int errcode = 0;
	if(0 != (errcode = pthread_cond_destroy(&m_comm_e)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_cond_destroy() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
	if(0 != (errcode = pthread_mutex_destroy(&m_e_lock)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_mutex_destroy() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
	if(0 != (errcode = pthread_mutex_destroy(&m_q_lock)))
	{
        char errmsg[256];
        syslog(LOG_ERR, "%s: pthread_mutex_destroy() failed with error %d : [%s].",
        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
        exit(__LINE__);
	}
	delete m_cc;
}

void ac_protocol::push_comm_event(comm_evt * evt)
{
	int errcode;
	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);
	to.tv_sec += 2;
	if(0 == (errcode = pthread_mutex_timedlock(&m_q_lock, &to)))
	{
		m_comm_q.push_back(evt);

		if(0 != (errcode = pthread_mutex_unlock(&m_q_lock)))
		{
			char errmsg[256];
			syslog(LOG_ERR, "%s: pthread_mutex_unlock() failed with error %d : [%s].",
					__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
			exit(__LINE__);
		}

		clock_gettime(CLOCK_REALTIME, &to);
		to.tv_sec += 2;
		if(0 == (errcode = pthread_mutex_timedlock(&m_e_lock, &to)))
		{
			if(0 != (errcode = pthread_cond_signal(&m_comm_e)))
			{
				char errmsg[256];
				syslog(LOG_ERR, "%s: pthread_cond_signal() failed with error %d : [%s].",
						__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				exit(__LINE__);
			}

			if(0 != (errcode = pthread_mutex_unlock(&m_e_lock)))
			{
				char errmsg[256];
				syslog(LOG_ERR, "%s: pthread_mutex_unlock() failed with error %d : [%s].",
						__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				exit(__LINE__);
			}
		}
		else
		{
			char errmsg[256];
			syslog(LOG_ERR, "%s: pthread_mutex_timedlock() failed with error %d : [%s].",
					__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
			exit(__LINE__);
		}
	}
	else
	{
		char errmsg[256];
		syslog(LOG_ERR, "%s: pthread_mutex_timedlock() failed with error %d : [%s].",
				__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
		exit(__LINE__);
	}
}

void ac_protocol::report_party_comm(const size_t party_id, const bool comm)
{
	comm_conn_evt * pevt = new comm_conn_evt;
	pevt->type = comm_evt_conn;
	pevt->party_id = party_id;
	pevt->connected = comm;
	push_comm_event(pevt);
}

void ac_protocol::on_comm_up_with_party(const unsigned int party_id)
{
	report_party_comm(party_id, true);
}

void ac_protocol::on_comm_down_with_party(const unsigned int party_id)
{
	report_party_comm(party_id, false);
}

void ac_protocol::on_comm_message(const unsigned int src_id, const unsigned char * msg, const size_t size)
{
	comm_msg_evt * pevt = new comm_msg_evt;
	pevt->type = comm_evt_msg;
	pevt->party_id = src_id;
	pevt->msg.assign(msg, msg + size);
	push_comm_event(pevt);
}

int ac_protocol::run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds)
{
	m_id = id;
	m_parties = parties;
	m_conf_file = conf_file;
	m_rounds = rounds;
	m_comm_q.clear();

	if(0 != pre_run())
	{
		syslog(LOG_ERR, "%s: run preliminary failure.", __FUNCTION__);
		return -1;
	}

	if(0 != m_cc->start(id, parties, conf_file, this))
	{
		syslog(LOG_ERR, "%s: comm client start failed; toss failure.", __FUNCTION__);
		return -1;
	}

	int errcode;
	struct timespec to, idle_since, now;
	m_run_flag = true;
	clock_gettime(CLOCK_REALTIME, &idle_since);
	while(m_run_flag)
	{
		clock_gettime(CLOCK_REALTIME, &to);
		to.tv_sec += 1;
		if(0 == (errcode = pthread_mutex_timedlock(&m_e_lock, &to)))
		{
			clock_gettime(CLOCK_REALTIME, &to);
			to.tv_sec += 1;
			if(0 != (errcode = pthread_cond_timedwait(&m_comm_e, &m_e_lock, &to)) && ETIMEDOUT != errcode)
			{
				char errmsg[256];
				syslog(LOG_ERR, "%s: pthread_cond_timedwait() failed with error %d : [%s].",
						__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				exit(__LINE__);
			}

			if(0 != (errcode = pthread_mutex_unlock(&m_e_lock)))
			{
				char errmsg[256];
				syslog(LOG_ERR, "%s: pthread_mutex_unlock() failed with error %d : [%s].",
						__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				exit(__LINE__);
			}

			if(handle_comm_events())
			{
				clock_gettime(CLOCK_REALTIME, &idle_since);
				while(m_run_flag && run_around() && round_up());
			}
			else
			{
				clock_gettime(CLOCK_REALTIME, &now);
				if(idle_timeout_seconds < (now.tv_sec - idle_since.tv_sec))
				{
					syslog(LOG_ERR, "%s: idle timeout %lu reached; toss failed.", __FUNCTION__, idle_timeout_seconds);
					m_run_flag = false;
				}
			}
		}
		else
		{
	        char errmsg[256];
	        syslog(LOG_ERR, "%s: pthread_mutex_timedlock() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
		}
	}
	m_cc->stop();
	if(0 != post_run())
	{
		syslog(LOG_ERR, "%s: post-run processing failure.", __FUNCTION__);
		return -1;
	}
	return 0;
}

bool ac_protocol::handle_comm_events()
{
	std::list< comm_evt * > all_comm_evts;

	int errcode;
	struct timespec to;
	clock_gettime(CLOCK_REALTIME, &to);
	to.tv_sec += 2;
	if(0 == (errcode = pthread_mutex_timedlock(&m_q_lock, &to)))
	{
		all_comm_evts.swap(m_comm_q);

		if(0 != (errcode = pthread_mutex_unlock(&m_q_lock)))
		{
			char errmsg[256];
			syslog(LOG_ERR, "%s: pthread_mutex_unlock() failed with error %d : [%s].",
					__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
			exit(__LINE__);
		}
	}
	else
	{
		char errmsg[256];
		syslog(LOG_ERR, "%s: pthread_mutex_timedlock() failed with error %d : [%s].",
				__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
		exit(__LINE__);
	}

	for(std::list< comm_evt * >::iterator i = all_comm_evts.begin(); i != all_comm_evts.end() && m_run_flag; ++i)
		handle_comm_event(*i);

	for(std::list< comm_evt * >::iterator i = all_comm_evts.begin(); i != all_comm_evts.end(); ++i)
		delete (*i);

	return (!all_comm_evts.empty());
}

void ac_protocol::handle_comm_event(comm_evt * evt)
{
	if(evt->party_id >= m_parties || evt->party_id == m_id)
	{
		syslog(LOG_ERR, "%s: invalid party id %u.", __FUNCTION__, evt->party_id);
		return;
	}

	switch(evt->type)
	{
	case comm_evt_conn:
		handle_conn_event(evt);
		break;
	case comm_evt_msg:
		handle_msg_event(evt);
		break;
	default:
		syslog(LOG_ERR, "%s: invalid comm event type %u", __FUNCTION__, evt->type);
		break;
	}
}

void ac_protocol::handle_conn_event(comm_evt * evt)
{
	comm_conn_evt * conn_evt = dynamic_cast<comm_conn_evt *>(evt);
	if(NULL != conn_evt)
		handle_party_conn(conn_evt->party_id, conn_evt->connected);
	else
		syslog(LOG_ERR, "%s: invalid event type cast.", __FUNCTION__);
}

void ac_protocol::handle_msg_event(comm_evt * evt)
{
	comm_msg_evt * msg_evt = dynamic_cast<comm_msg_evt *>(evt);
	if(NULL != msg_evt)
		handle_party_msg(msg_evt->party_id, msg_evt->msg);
	else
		syslog(LOG_ERR, "%s: invalid event type cast.", __FUNCTION__);
}


#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

#include <string>
#include <vector>
#include <list>

#include <log4cpp/Category.hh>

#include "lfq.h"
#include "comm_client_cb_api.h"
#include "comm_client_factory.h"
#include "ac_protocol.h"
#include "comm_client.h"

#define LC log4cpp::Category::getInstance(m_logcat)

ac_protocol::ac_protocol(comm_client_factory::client_type_t cc_type, const char * log_category)
: m_logcat(log_category), m_id(-1), m_parties(0), m_run_flag(false)
, m_cc( comm_client_factory::create_comm_client(cc_type, ((m_logcat + '.') + "cc").c_str()))
, m_evt_q(NULL), m_msg_evt_q(NULL), m_con_evt_q(NULL)
{
}

ac_protocol::~ac_protocol()
{
	delete m_cc;
}

void ac_protocol::push_comm_event(comm_evt * evt)
{
	m_evt_q->push(evt);
}

void ac_protocol::report_party_comm(const size_t party_id, const bool comm)
{
	comm_conn_evt * pevt = NULL;
	m_con_evt_q->pop(pevt);
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
	comm_msg_evt * pevt = NULL;
	m_msg_evt_q->pop(pevt);
	pevt->type = comm_evt_msg;
	pevt->party_id = src_id;
	pevt->msg.assign(msg, msg + size);
	push_comm_event(pevt);
}

int ac_protocol::run_ac_protocol(const size_t id, const size_t parties, const char * conf_file, const size_t idle_timeout_seconds)
{
	m_id = id;
	m_parties = parties;
	m_conf_file = conf_file;

	if(0 != pre_run())
	{
		LC.error("%s: run preliminary failure.", __FUNCTION__);
		return -1;
	}

	m_evt_q = new lfq< ac_protocol::comm_evt * >(2 * parties);

	m_msg_evt_q = new lfq< ac_protocol::comm_msg_evt * >(parties);
	for(size_t i = 0; i < parties; ++i) m_msg_evt_q->push(new ac_protocol::comm_msg_evt);

	m_con_evt_q = new lfq< ac_protocol::comm_conn_evt * >(parties);
	for(size_t i = 0; i < parties; ++i) m_con_evt_q->push(new ac_protocol::comm_conn_evt);

	if(0 != m_cc->start(id, parties, conf_file, this))
	{
		LC.error("%s: comm client start failed; run failure.", __FUNCTION__);
		return -1;
	}

	comm_evt * pevt;
	int errcode;
	struct timespec to, idle_since, now;
	m_run_flag = true;
	clock_gettime(CLOCK_REALTIME, &idle_since);
	while(m_run_flag)
	{
		if(0 == m_evt_q->pop(pevt, 1000))
		{
			handle_comm_event(pevt);
		}
		else
		{
			clock_gettime(CLOCK_REALTIME, &now);
			if(idle_timeout_seconds < (now.tv_sec - idle_since.tv_sec))
			{
				LC.error("%s: idle timeout %lu reached; run failed.", __FUNCTION__, idle_timeout_seconds);
				m_run_flag = false;
			}
		}
	}
	m_cc->stop();

	{
		ac_protocol::comm_evt * p;
		for(size_t i = 0; i < parties; ++i)
		{
			if(0 == m_evt_q->pop(p, 10))
				delete p;
			else
				break;
		}
		delete m_msg_evt_q;
	}


	{
		ac_protocol::comm_msg_evt * p;
		for(size_t i = 0; i < parties; ++i)
		{
			if(0 == m_msg_evt_q->pop(p, 10))
				delete p;
			else
				break;
		}
		delete m_msg_evt_q;
	}

	{
		ac_protocol::comm_conn_evt * p;
		for(size_t i = 0; i < parties; ++i)
		{
			if(0 == m_con_evt_q->pop(p, 10))
				delete p;
			else
				break;
		}
		delete m_con_evt_q;
	}

	if(0 != post_run())
	{
		LC.error("%s: post-run processing failure.", __FUNCTION__);
		return -1;
	}
	return 0;
}

void ac_protocol::handle_comm_event(comm_evt * evt)
{
	switch(evt->type)
	{
	case comm_evt_conn:
		handle_conn_event(evt);
		break;
	case comm_evt_msg:
		handle_msg_event(evt);
		break;
	default:
		LC.error("%s: invalid comm event type %u", __FUNCTION__, evt->type);
		break;
	}

	while(m_run_flag && run_around() && round_up());
}

void ac_protocol::handle_conn_event(comm_evt * evt)
{
	comm_conn_evt * conn_evt = dynamic_cast<comm_conn_evt *>(evt);
	if(NULL != conn_evt)
	{
		if(evt->party_id < m_parties && evt->party_id != m_id)
			handle_party_conn(conn_evt->party_id, conn_evt->connected);
		else
			LC.error("%s: invalid party id %u.", __FUNCTION__, evt->party_id);
		m_con_evt_q->push(conn_evt);
	}
	else
		LC.error("%s: invalid event type cast.", __FUNCTION__);
}

void ac_protocol::handle_msg_event(comm_evt * evt)
{
	comm_msg_evt * msg_evt = dynamic_cast<comm_msg_evt *>(evt);
	if(NULL != msg_evt)
	{
		if(evt->party_id < m_parties && evt->party_id != m_id)
			handle_party_msg(msg_evt->party_id, msg_evt->msg);
		else
			LC.error("%s: invalid party id %u.", __FUNCTION__, evt->party_id);
		m_msg_evt_q->push(msg_evt);
	}
	else
		LC.error("%s: invalid event type cast.", __FUNCTION__);
}

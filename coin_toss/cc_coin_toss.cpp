
#include <stdlib.h>
#include <semaphore.h>
#include <syslog.h>
#include <memory.h>
#include <stdio.h>
#include <errno.h>

#include <string>
#include <vector>
#include <list>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <openssl/rand.h>
#include <openssl/evp.h>
#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "comm_client.h"
#include "cc_coin_toss.h"
#include "comm_client_tcp_mesh.h"

#define SHA256_BYTE_SIZE		32
#define SEED_BYTE_SIZE			16

cc_coin_toss::cc_coin_toss()
: m_cc(new comm_client_tcp_mesh), m_id(-1), m_parties(0), m_rounds(0), m_run_flag(false)
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

cc_coin_toss::~cc_coin_toss()
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
}

int cc_coin_toss::run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds)
{
	m_id = id;
	m_parties = parties;
	m_conf_file = conf_file;
	m_rounds = rounds;
	m_toss_outcomes.clear();
	m_party_states.clear();
	m_party_states.resize(m_parties);
	m_comm_q.clear();

	if(0 != generate_data(m_id, m_party_states[m_id].seed, m_party_states[m_id].commit))
	{
		syslog(LOG_ERR, "%s: self data generation failed; toss failure.", __FUNCTION__);
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
	m_party_states.clear();
	for(std::list< comm_evt * >::iterator i = m_comm_q.begin(); i != m_comm_q.end(); ++i)
		delete (*i);
	m_comm_q.clear();

	if(m_toss_outcomes.size() != m_rounds)
	{
		syslog(LOG_ERR, "%s: invalid number of toss results %lu out of %lu; toss failure.", __FUNCTION__, m_toss_outcomes.size(), m_rounds);
		return -1;
	}
	size_t round = 0;
	for(std::list< std::vector< u_int8_t > >::const_iterator toss = m_toss_outcomes.begin(); toss != m_toss_outcomes.end(); ++toss, ++round)
	{
		syslog(LOG_INFO, "%s: toss result %lu = <%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X>",
				__FUNCTION__, round,
				(*toss)[0], (*toss)[1], (*toss)[2], (*toss)[3], (*toss)[4], (*toss)[5], (*toss)[6], (*toss)[7],
				(*toss)[8], (*toss)[9], (*toss)[10], (*toss)[11], (*toss)[12], (*toss)[13], (*toss)[14], (*toss)[15]);
	}
	return 0;
}

int cc_coin_toss::generate_data(const size_t id, std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit)
{
	int result = -1;
	seed.resize(SEED_BYTE_SIZE);
	commit.resize(SHA256_BYTE_SIZE);
	if(RAND_bytes(seed.data(), 16))
	{
		if(0 == (result = commit_seed(id, seed, commit)))
			syslog(LOG_DEBUG, "%s: %lu data generated.", __FUNCTION__, id);
		else
			syslog(LOG_ERR, "%s: commit_seed() failed.", __FUNCTION__);
	}
	else
		syslog(LOG_ERR, "%s: RAND_bytes() failed.", __FUNCTION__);
	return result;
}

int cc_coin_toss::commit_seed(const size_t id, const std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit)
{
	int result = -1;
	EVP_MD_CTX ctx;
	EVP_MD_CTX_init(&ctx);
	const EVP_MD * md = EVP_sha256();
	if(EVP_DigestInit_ex(&ctx, md, NULL))
	{
		if(EVP_DigestUpdate(&ctx, &id, sizeof(id)))
		{
			if(EVP_DigestUpdate(&ctx, seed.data(), seed.size()))
			{
				unsigned int digest_size = 0;
				commit.resize(SHA256_BYTE_SIZE);
				if(EVP_DigestFinal_ex(&ctx, commit.data(), &digest_size))
				{
					if(SHA256_BYTE_SIZE == digest_size)
					{
						result = 0;
						syslog(LOG_INFO, "%s: seed committed by SHA256 hash.", __FUNCTION__);
					}
					else
						syslog(LOG_ERR, "%s: digest size %u mismatch SHA256 size %u.", __FUNCTION__, digest_size, SHA256_BYTE_SIZE);
				}
				else
					syslog(LOG_ERR, "%s: EVP_DigestFinal_ex() failed.", __FUNCTION__);
			}
			else
				syslog(LOG_ERR, "%s: EVP_DigestUpdate(seed) failed.", __FUNCTION__);
		}
		else
			syslog(LOG_ERR, "%s: EVP_DigestUpdate(id) failed.", __FUNCTION__);
	}
	else
		syslog(LOG_ERR, "%s: EVP_DigestInit_ex() failed.", __FUNCTION__);
	EVP_MD_CTX_cleanup(&ctx);
	return result;
}

int cc_coin_toss::valid_seed(const size_t id, const std::vector<u_int8_t> & seed, const std::vector<u_int8_t> & commit)
{
	std::vector<u_int8_t> control_commit;
	commit_seed(id, seed, control_commit);
	if(0 == memcmp(control_commit.data(), commit.data(), SHA256_BYTE_SIZE))
		return 0;
	return -1;
}

void cc_coin_toss::push_comm_event(comm_evt * evt)
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

void cc_coin_toss::report_party_comm(const size_t party_id, const bool comm)
{
	comm_conn_evt * pevt = new comm_conn_evt;
	pevt->type = comm_evt_conn;
	pevt->party_id = party_id;
	pevt->connected = comm;
	push_comm_event(pevt);
}

void cc_coin_toss::on_comm_up_with_party(const unsigned int party_id)
{
	report_party_comm(party_id, true);
}

void cc_coin_toss::on_comm_down_with_party(const unsigned int party_id)
{
	report_party_comm(party_id, false);
}

void cc_coin_toss::on_comm_message(const unsigned int src_id, const unsigned char * msg, const size_t size)
{
	comm_msg_evt * pevt = new comm_msg_evt;
	pevt->type = comm_evt_msg;
	pevt->party_id = src_id;
	pevt->msg.assign(msg, msg + size);
	push_comm_event(pevt);
}

bool cc_coin_toss::handle_comm_events()
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

void cc_coin_toss::handle_comm_event(comm_evt * evt)
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

void cc_coin_toss::handle_conn_event(comm_evt * evt)
{
	comm_conn_evt * conn_evt = dynamic_cast<comm_conn_evt *>(evt);
	if(NULL != conn_evt)
		handle_party_conn(conn_evt->party_id, conn_evt->connected);
	else
		syslog(LOG_ERR, "%s: invalid event type cast.", __FUNCTION__);
}

void cc_coin_toss::handle_msg_event(comm_evt * evt)
{
	comm_msg_evt * msg_evt = dynamic_cast<comm_msg_evt *>(evt);
	if(NULL != msg_evt)
		handle_party_msg(msg_evt->party_id, msg_evt->msg);
	else
		syslog(LOG_ERR, "%s: invalid event type cast.", __FUNCTION__);
}

void cc_coin_toss::handle_party_conn(const size_t party_id, const bool connected)
{
	party_t & peer(m_party_states[party_id]);

	if(connected)
	{
		if(ps_nil == peer.state)
		{
			syslog(LOG_DEBUG, "%s: party %lu is now connected.", __FUNCTION__, party_id);
			peer.state = ps_connected;
		}
		else
			syslog(LOG_WARNING, "%s: party %lu unexpectedly again connected.", __FUNCTION__, party_id);
	}
	else
	{
		bool OK =
		(
				(m_toss_outcomes.size() == m_rounds)
				||
				(m_toss_outcomes.size() == (m_rounds - 1) && peer.state > ps_seed_sent)
				||
				(m_toss_outcomes.size() == (m_rounds - 1) && peer.state == ps_seed_sent && SEED_BYTE_SIZE <= (peer.seed.size() + peer.data.size()))
		);

		if(!OK)
		{
			syslog(LOG_ERR, "%s: party id %lu premature disconnection; toss failed.", __FUNCTION__, party_id);
			m_run_flag = false;
		}
		else
		{
			syslog(LOG_DEBUG, "%s: party %lu is now disconnected.", __FUNCTION__, party_id);
		}
	}
}

void cc_coin_toss::handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg)
{
	party_t & peer(m_party_states[party_id]);
	peer.data.insert(peer.data.end(), msg.data(), msg.data() + msg.size());
}

bool cc_coin_toss::run_around()
{
	bool round_ready = true;
	for(size_t pid = 0; pid < m_parties; ++pid)
	{
		if(pid == m_id) continue;
		round_ready = round_ready && party_run_around(pid);//(ps_round_up == m_party_states[pid].state);
	}
	return round_ready;
}

bool cc_coin_toss::party_run_around(const size_t party_id)
{
	party_t & peer(m_party_states[party_id]);
	party_t & self(m_party_states[m_id]);
	switch(peer.state)
	{
	case ps_nil:
		return false;
	case ps_connected:
		if(0 != m_cc->send(party_id, self.commit.data(), self.commit.size()))
		{
			syslog(LOG_ERR, "%s: party id %lu commit send failure; toss failed.", __FUNCTION__, party_id);
			return (m_run_flag = false);
		}
		else
			peer.state = ps_commit_sent;
			/* no break */
	case ps_commit_sent:
		if(peer.commit.size() < SHA256_BYTE_SIZE)
		{
			if(!peer.data.empty())
			{
				size_t chunk_size = SHA256_BYTE_SIZE - peer.commit.size();
				if(peer.data.size() < chunk_size) chunk_size = peer.data.size();
				peer.commit.insert(peer.commit.end(), peer.data.data(), peer.data.data() + chunk_size);
				peer.data.erase(peer.data.begin(), peer.data.begin() + chunk_size);
			}

			if(peer.commit.size() < SHA256_BYTE_SIZE)
				return false;//wait for more data
		}
		peer.state = ps_commit_rcvd;
		/* no break */
	case ps_commit_rcvd:
		if(0 != m_cc->send(party_id, self.seed.data(), self.seed.size()))
		{
			syslog(LOG_ERR, "%s: party id %lu seed send failure; toss failed.", __FUNCTION__, party_id);
			return (m_run_flag = false);
		}
		peer.state = ps_seed_sent;
		/* no break */
	case ps_seed_sent:
		if(peer.seed.size() < SEED_BYTE_SIZE)
		{
			if(!peer.data.empty())
			{
				size_t chunk_size = SEED_BYTE_SIZE - peer.seed.size();
				if(peer.data.size() < chunk_size) chunk_size = peer.data.size();
				peer.seed.insert(peer.seed.end(), peer.data.data(), peer.data.data() + chunk_size);
				peer.data.erase(peer.data.begin(), peer.data.begin() + chunk_size);
			}

			if(peer.seed.size() < SEED_BYTE_SIZE)
				return false;//wait for more data
		}
		peer.state = ps_seed_rcvd;
		/* no break */
	case ps_seed_rcvd:
		if(0 != valid_seed(party_id, peer.seed, peer.commit))
		{
			syslog(LOG_ERR, "%s: party id %lu seed invalid; toss failed.", __FUNCTION__, party_id);
			return (m_run_flag = false);
		}
		peer.state = ps_round_up;
		/* no break */
	case ps_round_up:
		return true;
	default:
		syslog(LOG_ERR, "%s: invalid party state value %u.", __FUNCTION__, peer.state);
		exit(__LINE__);
	}
}

bool cc_coin_toss::round_up()
{
	for(size_t pid = 0; pid < m_parties; ++pid)
	{
		if(pid == m_id) continue;
		if(ps_round_up != m_party_states[pid].state)
			return false;
	}

	std::vector<u_int8_t> toss(SEED_BYTE_SIZE, 0);
	for(size_t pid = 0; pid < m_parties; ++pid)
	{
		for(size_t j = 0; j < SEED_BYTE_SIZE; ++j)
			toss[j] ^= m_party_states[pid].seed[j];
		m_party_states[pid].commit.clear();
		m_party_states[pid].seed.clear();
		m_party_states[pid].state = ps_connected;
	}

	m_toss_outcomes.push_back(toss);
	if(m_toss_outcomes.size() == m_rounds)
	{
		syslog(LOG_NOTICE, "%s: done tossing; all results are in.", __FUNCTION__);
		return (m_run_flag = false);
	}

	if(0 != generate_data(m_id, m_party_states[m_id].seed, m_party_states[m_id].commit))
	{
		syslog(LOG_ERR, "%s: self data generation failed; toss failure.", __FUNCTION__);
		exit(__LINE__);
	}

	return true;
}

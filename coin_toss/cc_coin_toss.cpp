
#include <stdlib.h>
#include <semaphore.h>
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
#include <log4cpp/Category.hh>
#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "ac_protocol.h"
#include "comm_client.h"
#include "cc_coin_toss.h"
#include "comm_client_tcp_mesh.h"

#define SHA256_BYTE_SIZE		32
#define SEED_BYTE_SIZE			16

#define LC log4cpp::Category::getInstance(m_logcat)

cc_coin_toss::cc_coin_toss(const char * log_category)
: ac_protocol(log_category)
{
}

cc_coin_toss::~cc_coin_toss()
{
}

int cc_coin_toss::run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds)
{
	m_id = id;
	m_parties = parties;
	m_conf_file = conf_file;
	m_rounds = rounds;
	m_comm_q.clear();

	pre_run();

	if(0 != m_cc->start(id, parties, conf_file, this))
	{
		LC.error("%s: comm client start failed; toss failure.", __FUNCTION__);
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
				LC.error("%s: pthread_cond_timedwait() failed with error %d : [%s].",
						__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
				exit(__LINE__);
			}

			if(0 != (errcode = pthread_mutex_unlock(&m_e_lock)))
			{
				char errmsg[256];
				LC.error("%s: pthread_mutex_unlock() failed with error %d : [%s].",
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
					LC.error("%s: idle timeout %lu reached; toss failed.", __FUNCTION__, idle_timeout_seconds);
					m_run_flag = false;
				}
			}
		}
		else
		{
	        char errmsg[256];
	        LC.error("%s: pthread_mutex_timedlock() failed with error %d : [%s].",
	        		__FUNCTION__, errcode, strerror_r(errcode, errmsg, 256));
		}
	}

	m_cc->stop();
	post_run();
	return 0;
}

int cc_coin_toss::pre_run()
{
	m_toss_outcomes.clear();
	m_party_states.clear();
	m_party_states.resize(m_parties);

	if(0 != generate_data(m_id, m_party_states[m_id].seed, m_party_states[m_id].commit))
	{
		LC.error("%s: self data generation failed; toss failure.", __FUNCTION__);
		return -1;
	}
	return 0;
}

int cc_coin_toss::post_run()
{
	m_party_states.clear();
	for(std::list< comm_evt * >::iterator i = m_comm_q.begin(); i != m_comm_q.end(); ++i)
		delete (*i);
	m_comm_q.clear();

	if(m_toss_outcomes.size() != m_rounds)
	{
		LC.error("%s: invalid number of toss results %lu out of %lu; toss failure.", __FUNCTION__, m_toss_outcomes.size(), m_rounds);
		return -1;
	}
	size_t round = 0;
	for(std::list< std::vector< u_int8_t > >::const_iterator toss = m_toss_outcomes.begin(); toss != m_toss_outcomes.end(); ++toss, ++round)
	{
		LC.info("%s: toss result %lu = <%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X>",
				__FUNCTION__, round,
				(*toss)[0], (*toss)[1], (*toss)[2], (*toss)[3], (*toss)[4], (*toss)[5], (*toss)[6], (*toss)[7],
				(*toss)[8], (*toss)[9], (*toss)[10], (*toss)[11], (*toss)[12], (*toss)[13], (*toss)[14], (*toss)[15]);
	}
	return 0;
}

int cc_coin_toss::generate_data(const size_t id, std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit) const
{
	int result = -1;
	seed.resize(SEED_BYTE_SIZE);
	commit.resize(SHA256_BYTE_SIZE);
	if(RAND_bytes(seed.data(), 16))
	{
		if(0 == (result = commit_seed(id, seed, commit)))
			LC.debug("%s: %lu data generated.", __FUNCTION__, id);
		else
			LC.error("%s: commit_seed() failed.", __FUNCTION__);
	}
	else
		LC.error("%s: RAND_bytes() failed.", __FUNCTION__);
	return result;
}

int cc_coin_toss::commit_seed(const size_t id, const std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit) const
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
						LC.info("%s: seed committed by SHA256 hash.", __FUNCTION__);
					}
					else
						LC.error("%s: digest size %u mismatch SHA256 size %u.", __FUNCTION__, digest_size, SHA256_BYTE_SIZE);
				}
				else
					LC.error("%s: EVP_DigestFinal_ex() failed.", __FUNCTION__);
			}
			else
				LC.error("%s: EVP_DigestUpdate(seed) failed.", __FUNCTION__);
		}
		else
			LC.error("%s: EVP_DigestUpdate(id) failed.", __FUNCTION__);
	}
	else
		LC.error("%s: EVP_DigestInit_ex() failed.", __FUNCTION__);
	EVP_MD_CTX_cleanup(&ctx);
	return result;
}

int cc_coin_toss::valid_seed(const size_t id, const std::vector<u_int8_t> & seed, const std::vector<u_int8_t> & commit) const
{
	std::vector<u_int8_t> control_commit;
	commit_seed(id, seed, control_commit);
	if(0 == memcmp(control_commit.data(), commit.data(), SHA256_BYTE_SIZE))
		return 0;
	return -1;
}

void cc_coin_toss::handle_party_conn(const size_t party_id, const bool connected)
{
	party_t & peer(m_party_states[party_id]);

	if(connected)
	{
		if(ps_nil == peer.state)
		{
			LC.debug("%s: party %lu is now connected.", __FUNCTION__, party_id);
			peer.state = ps_connected;
		}
		else
			LC.warn("%s: party %lu unexpectedly again connected.", __FUNCTION__, party_id);
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
			LC.error("%s: party id %lu premature disconnection; toss failed.", __FUNCTION__, party_id);
			m_run_flag = false;
		}
		else
		{
			LC.debug("%s: party %lu is now disconnected.", __FUNCTION__, party_id);
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
			LC.error("%s: party id %lu commit send failure; toss failed.", __FUNCTION__, party_id);
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
			LC.error("%s: party id %lu seed send failure; toss failed.", __FUNCTION__, party_id);
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
			LC.error("%s: party id %lu seed invalid; toss failed.", __FUNCTION__, party_id);
			return (m_run_flag = false);
		}
		peer.state = ps_round_up;
		/* no break */
	case ps_round_up:
		return true;
	default:
		LC.error("%s: invalid party state value %u.", __FUNCTION__, peer.state);
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
		LC.notice("%s: done tossing; all results are in.", __FUNCTION__);
		return (m_run_flag = false);
	}

	if(0 != generate_data(m_id, m_party_states[m_id].seed, m_party_states[m_id].commit))
	{
		LC.error("%s: self data generation failed; toss failure.", __FUNCTION__);
		exit(__LINE__);
	}

	return true;
}

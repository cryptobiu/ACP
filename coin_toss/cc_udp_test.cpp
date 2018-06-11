
#include <unistd.h>
#include <semaphore.h>
#include <memory.h>

#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <openssl/rand.h>
#include <openssl/evp.h>

#include <log4cpp/Category.hh>

#define lc_fatal(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_error(...) log4cpp::Category::getInstance(m_logcat).error(__VA_ARGS__)
#define lc_warn(...) log4cpp::Category::getInstance(m_logcat).warn(__VA_ARGS__)
#define lc_notice(...) log4cpp::Category::getInstance(m_logcat).notice(__VA_ARGS__)
#define lc_info(...) log4cpp::Category::getInstance(m_logcat).info(__VA_ARGS__)
#define lc_debug(...) log4cpp::Category::getInstance(m_logcat).debug(__VA_ARGS__)

#include <event2/event.h>

#include "comm_client_cb_api.h"
#include "comm_client_factory.h"
#include "comm_client_udp.h"
#include "ac_protocol.h"

class cc_udp_tst : public ac_protocol
{
	size_t m_rounds;

	typedef struct __party_t
	{
		std::list<u_int32_t> m_msgs;
		u_int32_t m_round_rcvd, m_round_sent;
		u_int32_t m_round_done;
		u_int32_t m_resends;
		struct timespec beat;
		__party_t():m_round_done(0),m_round_rcvd(0), m_round_sent(0), m_resends(0){}
	}party_t;
	static const u_int32_t sm_round_mask;
	std::vector< cc_udp_tst::party_t > m_party_states;

	void handle_party_conn(const size_t party_id, const bool connected);
	void handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg);

	int pre_run();
	bool run_around();
	bool party_run_around(const size_t party_id);
	bool round_up();
	int post_run();

public:
	cc_udp_tst(comm_client::cc_args_t * cc_args);
	virtual ~cc_udp_tst();

	int run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds);
};

void test_udp_cc(const unsigned int id, const unsigned int count, const char * party_file, const size_t rounds, const char * logcat)
{
	comm_client::cc_args_t cc_args;
	cc_args.logcat = std::string(logcat) + ".ccu";
	cc_udp_tst ccu(&cc_args);
	if(0 != ccu.run(id, count, party_file, rounds, 60))
		log4cpp::Category::getInstance(logcat).error("%s: %lu round tcp mesh coin toss test failed.", __FUNCTION__, rounds);
	else
		log4cpp::Category::getInstance(logcat).notice("%s: %lu round tcp mesh coin toss test succeeded.", __FUNCTION__, rounds);
}

const u_int32_t cc_udp_tst::sm_round_mask = 0x3FFFFFFF;

cc_udp_tst::cc_udp_tst(comm_client::cc_args_t * cc_args)
: ac_protocol(comm_client_factory::cc_udp, cc_args)
{
}

cc_udp_tst::~cc_udp_tst()
{
}

int cc_udp_tst::run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds)
{
	lc_notice("%s: running protocol.", __FUNCTION__);
	m_rounds = rounds;
	return ac_protocol::run_ac_protocol(id, parties, conf_file, idle_timeout_seconds);
}

void cc_udp_tst::handle_party_conn(const size_t party_id, const bool connected)
{
	lc_error("%s: party %lu %sconnected? should not be reported using UDP.", __FUNCTION__, party_id, ((connected)? "": "dis"));
}

void cc_udp_tst::handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg)
{
	if(party_id > m_parties || party_id == m_id)
	{
		lc_error("%s: invalid source party id %lu.", __FUNCTION__, party_id);
		return;
	}

	if(msg.size() != sizeof(u_int32_t))
	{
		lc_error("%s: invalid msg size %lu from party id %lu.", __FUNCTION__, msg.size(), party_id);
		return;
	}

	m_party_states[party_id].m_msgs.push_back(ntohl(*((u_int32_t *)msg.data())));
}

int cc_udp_tst::pre_run()
{
	m_party_states.resize(m_parties);
	for(size_t i = 0; i < m_parties; ++i)
		clock_gettime(CLOCK_REALTIME, &m_party_states[i].beat);
	return 0;
}

bool cc_udp_tst::run_around()
{
	bool round_ready = true;
	for(size_t pid = 0; pid < m_parties; ++pid)
	{
		if(pid == m_id) continue;
		round_ready = round_ready && party_run_around(pid);
	}
	return round_ready;
}

bool cc_udp_tst::party_run_around(const size_t party_id)
{
	return (m_run_flag = false);
	/*
	party_t & peer(m_party_states[party_id]);

	u_int32_t round_sent_value = (peer.m_round_sent & sm_round_mask);
	u_int32_t round_sent_flag = (peer.m_round_sent & ~sm_round_mask)>>30;

	if(2 > round_sent_flag)
	{
		if(0 == round_sent_flag)//0 - have not sent yet
		{
			peer.m_round_sent = (1 << 30) | (peer.m_round_done + 1);
			u_int32_t val_2_send = ntohl(peer.m_round_sent);

			if(0 != m_cc->send(party_id, &val_2_send, sizeof(u_int32_t)))
			{
				lc_error("%s: party id %lu round %08X send failed.", __FUNCTION__, party_id, peer.m_round_sent);
				return (m_run_flag = false);
			}
			clock_gettime(CLOCK_REALTIME, &peer.beat);
		}
		else//1 - sent but not acked; check to
		{
			struct timespec now;
			clock_gettime(CLOCK_REALTIME, &now);
			if(now.tv_sec > (peer.beat.tv_sec + 10))
			{
				if(++peer.m_resends > 3)
				{
					lc_error("%s: party id %lu round %u timeout; resend %u; quitting.", __FUNCTION__, party_id, round_sent_value, peer.m_resends);
					return (m_run_flag = false);
				}
				else
				{
					lc_notice("%s: party id %lu round %u timeout; resend %u.", __FUNCTION__, party_id, round_sent_value, peer.m_resends);
				}
			}
		}
	}
	*/
}

bool cc_udp_tst::round_up()
{
	return (m_run_flag = false);
}

int cc_udp_tst::post_run()
{
	return -1;
}

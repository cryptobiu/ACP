
#pragma once

class cc_coin_toss : public ac_protocol
{
	size_t m_rounds;
	std::list< std::vector< u_int8_t > > m_toss_outcomes;

	typedef enum
	{
		ps_nil = 0,
		ps_connected,
		ps_commit_sent,
		ps_commit_rcvd,
		ps_seed_sent,
		ps_seed_rcvd,
		ps_round_up
	}party_state_t;

	typedef struct __party_t
	{
		party_state_t state;
		std::vector< u_int8_t > data, commit, seed;
		__party_t():state(ps_nil){}
	}party_t;

	std::vector< cc_coin_toss::party_t > m_party_states;

	void handle_party_conn(const size_t party_id, const bool connected);
	void handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg);

	int pre_run();
	bool run_around();
	bool party_run_around(const size_t party_id);
	bool round_up();
	int post_run();

	int generate_data(const size_t id, std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit) const;
	int commit_seed(const size_t id, const std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit) const;
	int valid_seed(const size_t id, const std::vector<u_int8_t> & seed, const std::vector<u_int8_t> & commit) const;

public:
	cc_coin_toss(const char * log_category);
	virtual ~cc_coin_toss();

	int run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds, const size_t idle_timeout_seconds);
};

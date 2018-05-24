
#pragma once

class cc_coin_toss : public comm_client_cb_api
{
	comm_client * m_cc;
	size_t m_id, m_parties, m_rounds;
	std::string m_conf_file;

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

	typedef enum { comm_evt_nil = 0, comm_evt_conn, comm_evt_msg } comm_evt_type_t;
	class comm_evt
	{
	public:
		comm_evt() : type(comm_evt_nil), party_id(-1) {}
		virtual ~comm_evt(){}
		comm_evt_type_t type;
		unsigned int party_id;
	};
	class comm_conn_evt : public comm_evt
	{
	public:
		bool connected;
	};
	class comm_msg_evt : public comm_evt
	{
	public:
		std::vector< u_int8_t > msg;
	};

	std::list< comm_evt * > m_comm_q;
	pthread_cond_t m_comm_e;
	pthread_mutex_t m_q_lock, m_e_lock;

	bool m_run_flag;
	void handle_comm_events();
	void handle_comm_event(comm_evt * evt);
	void handle_conn_event(comm_evt * evt);
	void handle_msg_event(comm_evt * evt);
	void handle_party_conn(const size_t party_id, const bool connected);
	void handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg);

	bool run_around();
	bool party_run_around(const size_t party_id);
	bool round_up();

	void push_comm_event(comm_evt * evt);
	void report_party_comm(const size_t party_id, const bool comm);

	static int generate_data(const size_t id, std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit);
	static int commit_seed(const size_t id, const std::vector<u_int8_t> & seed, std::vector<u_int8_t> & commit);
	static int valid_seed(const size_t id, const std::vector<u_int8_t> & seed, const std::vector<u_int8_t> & commit);

public:
	cc_coin_toss();
	virtual ~cc_coin_toss();

	int run(const size_t id, const size_t parties, const char * conf_file, const size_t rounds);

	virtual void on_comm_up_with_party(const unsigned int party_id);
	virtual void on_comm_down_with_party(const unsigned int party_id);
	virtual void on_comm_message(const unsigned int src_id, const unsigned char * msg, const size_t size);
};

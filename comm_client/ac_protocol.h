
#pragma once

class comm_client;

class ac_protocol : public comm_client_cb_api
{
protected:
	std::string m_logcat;
	comm_client * m_cc;
	size_t m_id, m_parties;
	std::string m_conf_file;
	bool m_run_flag;

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

	lfq< ac_protocol::comm_evt * > * m_evt_q;
	lfq< ac_protocol::comm_msg_evt * > * m_msg_evt_q;
	lfq< ac_protocol::comm_conn_evt * > * m_con_evt_q;

	void push_comm_event(comm_evt * evt);
	void report_party_comm(const size_t party_id, const bool comm);

	void handle_comm_event(comm_evt * evt);
	void handle_conn_event(comm_evt * evt);
	void handle_msg_event(comm_evt * evt);
	virtual void handle_party_conn(const size_t party_id, const bool connected) = 0;
	virtual void handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg) = 0;

	virtual int pre_run() = 0;
	virtual bool run_around() = 0;
	virtual bool round_up() = 0;
	virtual int post_run() = 0;

public:
	ac_protocol(comm_client_factory::client_type_t cc_type, comm_client::cc_args_t * cc_args);
	virtual ~ac_protocol();

	virtual int run_ac_protocol(const size_t id, const size_t parties, const char * conf_file, const size_t idle_timeout_seconds);

	virtual void on_comm_up_with_party(const unsigned int party_id);
	virtual void on_comm_down_with_party(const unsigned int party_id);
	virtual void on_comm_message(const unsigned int src_id, const unsigned char * msg, const size_t size);
};

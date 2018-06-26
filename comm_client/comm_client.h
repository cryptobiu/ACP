
#pragma once

#include <semaphore.h>
#include <string>

class comm_client_cb_api;

class comm_client
{
protected:
	std::string m_logcat;
	unsigned int m_id, m_peer_count;
	std::string m_comm_conf_file;
	comm_client_cb_api * m_sink;
	pthread_t m_runner;
	sem_t m_run_flag;

	virtual void run() = 0;
	int launch();

	bool get_run_flag();
	void set_run_flag(bool);

	static int parse_address(const char * address, std::string & ip, u_int16_t & port, struct sockaddr_in & sockaddr);

public:

	typedef struct __cc_args
	{
		std::string logcat;
		std::string proxy_addr;
		u_int16_t proxy_port;

		__cc_args()
		: proxy_port(0)
		{}
	}cc_args_t;

	comm_client(cc_args_t * cc_args);
	virtual ~comm_client();

	virtual int start(const unsigned int id, const unsigned int peer_count, const char * comm_conf_file, comm_client_cb_api * sink);
	virtual void stop();

	virtual int send(const unsigned int dst_id, const unsigned char * msg, const size_t size) = 0;
	virtual int broadcast(const unsigned char * msg, const size_t size);

	friend void * comm_client_proc(void * arg);
};

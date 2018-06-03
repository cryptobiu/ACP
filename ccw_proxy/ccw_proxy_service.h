
#pragma once

class ccw_proxy_service : public comm_client_cb_api
{
public:
	ccw_proxy_service(const char * log_category);
	virtual ~ccw_proxy_service();

	typedef struct __client
	{
		unsigned int id;
		unsigned int count;
		std::string conf_file;

		__client()
		: id((unsigned int)-1), count((unsigned int)-1)
		{}
	}client_t;

	typedef struct __service
	{
		std::string ip;
		u_int16_t port;

		__service()
		: port((u_int16_t)-1)
		{}
	}service_t;

	//comm_client_cb_api
	virtual void on_comm_up_with_party(const unsigned int party_id);
	virtual void on_comm_down_with_party(const unsigned int party_id);
	virtual void on_comm_message(const unsigned int src_id, const unsigned char * msg, const size_t size);

	int serve(const service_t & a_svc, const client_t & a_clnt);
};

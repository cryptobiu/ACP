
#pragma once

class comm_client_udp : public comm_client
{
protected:
	void run();
	int load_peers();
	int start_socket();

	typedef struct
	{
		size_t id;
		std::string ip;
		u_int16_t port;
		struct sockaddr_in inet_addr;
	}peer_t;
	std::map< size_t , peer_t > m_peers;
	std::map< u_int64_t , size_t > m_reeps;

	int m_sock_fd;

	typedef struct
	{
		size_t id;
		std::vector< u_int8_t > data;
	}msg_t;
	int m_send_pipe[2];
	pthread_mutex_t m_send_lock;
	std::deque< msg_t * > m_msg_q;

	struct mmsghdr * m_send_msg_vec;
	struct mmsghdr * m_recv_msg_vec;
	static void init_msg_vec(struct mmsghdr * & msg_vec, size_t count);
	static void clear_msg_vec(struct mmsghdr * & msg_vec, size_t count);
	static void delete_msg_vec(struct mmsghdr * & msg_vec, size_t count);

	static u_int64_t tokenize_inetaddr(const struct sockaddr_in &);

	struct event_base * the_base;
	struct event * the_timer;
	struct event * the_recvr;
	struct event * the_sendr;

	void on_timer(evutil_socket_t fd, short what);
	void on_recvr(evutil_socket_t fd, short what);
	void on_sendr(evutil_socket_t fd, short what);

	void process_recvd_msg(const size_t msg_len, const struct msghdr & msg_hdr);
	void perform_msgs_send();
	void pop_queued_msgs(std::deque< msg_t * > & qd_msgs);

public:
	comm_client_udp(comm_client::cc_args_t * cc_args);
	virtual ~comm_client_udp();

	int start(const unsigned int id, const unsigned int peer_count, const char * comm_conf_file, comm_client_cb_api * sink);
	void stop();

	int send(const unsigned int dst_id, const unsigned char * msg, const size_t size);

	static void time_cb(evutil_socket_t fd, short what, void * arg);
	static void recv_cb(evutil_socket_t fd, short what, void * arg);
	static void send_cb(evutil_socket_t fd, short what, void * arg);
};


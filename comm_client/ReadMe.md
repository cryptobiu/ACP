# COMM CLIENT LIBRARY
The comm_client library features:
- asynchronous protocol infrastructure.
- asynchronous communication infrastructure.
- communication layer abstraction.
- various communication implementations (TCP, UDP, TCP-by-proxy).

## Classes
The library includes the following classes:
- comm_client - a virtual communication client base class.
- comm_client_cb_api - a pure virtual call back interface for the communication client.
- comm_client_tcp_mesh - a communication client implementation using a mesh network of TCP connections.
- comm_client_udp - a communication client implementation using a UDP socket.
- cct_proxy_client - a communication client implementation using a TCP proxy to a TCP mesh network.
- ac_protocol - a virtual base for a protocol implementation that utilizes asynchronous communication.

## Asynchronous Communication Protocol
The mechanism for a protocol that uses asynchronous communication is provided by the ac_protocol class. In order to implement a new asynchronous protocol one must derive the new protocol class from the ac_protocol and override the task handling methods.

### Asynchronous Communication Protocol Concepts
The basic principle of an asynchronous protocol is that there are segments of the protocol that can be handled between oneself and other parties independently and asynchronously, and yet there are points in between (called synch points) where the state of processing must be synchronized between oneself and all peer parties in order to compute a desired outcome and advance.

In the ac_protocol implementation the asynchronous segments are called rounds while the synchronous points are referred to as advancing to the next round.

During a round data is freely (and asynchronously) exchanged between oneself and each peer party until the next synch point is reached, namely, the end of the round. Once the synch point is reached with all peer parties, the protocol can be advanced to the next round by computing the synch point result.

A recommended example of such an implementation can be found at [ACP cointoss].

### Asynchronous Communication Protocol Implementation
Suppose we want to implement a protocol that uses asynchronous communication. The first thing we'll do is to derive our protocol class from ac_protocol.

```c
class my_ac_protocol: public ac_protocol {
public:
	my_ac_protocol(comm_client_factory::client_type_t cc_type,
			comm_client::cc_args_t * cc_args);
	virtual ~my_ac_protocol();
}
```

The constructor parameters are required to be passed on to the ac_protocol constructor in order to create the comm_client object.
Once the class is created, a protocol specific information for each and every peer party and perhaps generic information must be added to the class to preserve the information between rounds and accumulate the computation result.

```c
class my_ac_protocol: public ac_protocol {
	typedef struct {
		size_t party_id;
		//protocol specific information...
	} party_state_information_t;

	std::map<size_t, party_state_information_t> m_party_info_map;
	//generic protocol specific information...

public:
	my_ac_protocol(comm_client_factory::client_type_t cc_type,
			comm_client::cc_args_t * cc_args);
	virtual ~my_ac_protocol();
}
```

The next thing we need to do is to implement the pure virtual methods of ac_protocol:
- __*handle_party_conn(const size_t party_id, const bool connected)*__ - called in order to process a report from the communication client regarding a connection/desconnection of a peer party.
- __*handle_party_msg(const size_t party_id, std::vector< u_int8_t > & msg)*__ - called in order to process an incoming network message from a peer party.
- __*pre_run()*__ - called before starting any of the protocol rounds; returns 0 for success.
- __*run_around()*__ - called during a protocol round; returns false unless we're ready to advance to the next round.
- __*round_up()*__ - called to advance to the next round; returns true for success.
- __*post_run()*__ - called after the last round to allow for resource cleanup.

```c
class my_ac_protocol: public ac_protocol {
	typedef struct {
		size_t party_id;
		//protocol specific information...
	} party_state_information_t;

	std::map<size_t, party_state_information_t> m_party_info_map;
	//generic protocol specific information...

	//ac_protocol overrides    
	void handle_party_conn(const size_t party_id, const bool connected);
	void handle_party_msg(const size_t party_id, std::vector<u_int8_t> & msg);
	int pre_run();
	bool run_around();
	bool round_up();
	int post_run();
public:
	my_ac_protocol(comm_client_factory::client_type_t cc_type,
			comm_client::cc_args_t * cc_args);
	virtual ~my_ac_protocol();
}
```
The *pre_run()* and *post_run()* can be used to initialize and terminate (respectively) the party/generic protocol specific information data structures. The other ac_protocol overrides are supposed to either add information to the party/generic protocol specific information or trigger protocol specific result computation.

To run the protocol the ac_protocol::run_ac_protocol(...) must be called with the following arguments:
- const size_t __*id*__ - the party identifier of self.
- const size_t __*parties*__ - the number of paticipating parties.
- const char * __*conf_file*__ - an address file for all parties, in numerical order, in the form *ip-address:port*.
- const size_t __*idle_timeout_seconds*__ - the maximum number of seconds with no network traffic, after which the protocol is aborted.


[//]: # 
   [ACP cointoss]: <https://github.com/cryptobiu/ACP/blob/master/coin_toss/cc_coin_toss.h>

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

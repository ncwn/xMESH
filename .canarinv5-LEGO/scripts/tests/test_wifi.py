#!/usr/bin/env python3
import socket
import sys

host_ip = "0.0.0.0"
host_port = 4321
buf_size = 1024

if __name__ == '__main__':
    print ('Running {}'.format(sys.argv[0]))
    sock = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    sock.bind((host_ip, host_port))
    iter = 1

    while True:
        msg, client_addr = sock.recvfrom(buf_size)
        client_msg = 'Message from Client:{}'.format(msg)
        client_ip = 'Client IP Address:{}'.format(client_addr)
        print(client_msg)
        print(client_ip)
        reply = '{} netif test recv'.format(iter)
        sock.sendto(reply.encode(), client_addr)
        iter += 1

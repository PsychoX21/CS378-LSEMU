# I found this for handling changes in config.txt and using the same idea
# https://devindeep.com/how-to-detect-file-changes-with-python/

import os
import time
import socket
from common.protocol import unpack_connect, pack_link_state

graph_dict = {}
N = 0

def parse(file_path):
    global graph_dict, N
    graph_dict = {}
    N = 0
    with open(file_path, 'r') as f:
        for line in f:
            if line.strip() and not line.startswith('#'):
                current_node = chr(N + 65)
                if(graph_dict.get(current_node) is None):
                    graph_dict[current_node] = []
                neighbor = N + 1
                for l in line.strip().split():
                    l = int(l)
                    if l == -1:
                        neighbor += 1
                        continue
                    graph_dict[current_node].append((chr(neighbor + 65), l))
                    if graph_dict.get(chr(neighbor + 65)) is None:
                        graph_dict[chr(neighbor + 65)] = []
                    graph_dict[chr(neighbor + 65)].append((current_node, l))
                    neighbor += 1
                N += 1
    N += 1
    print(graph_dict)

def send_link_state(connections, node_to_ip_port):
    for node in node_to_ip_port:
        records = [(node, node_to_ip_port[node][0], node_to_ip_port[node][1], 0)]
        neighbors = graph_dict[node]
        for neighbor in neighbors:
            neighbor_node = neighbor[0]
            neighbor_ip = node_to_ip_port[neighbor_node][0]
            neighbor_port = node_to_ip_port[neighbor_node][1]
            neighbor_cost = neighbor[1]
            records.append((neighbor_node, neighbor_ip, neighbor_port, neighbor_cost))
        connections[node].sendall(pack_link_state(records))

def start():
    global modifiedOn,N
    try:
        node_to_ip_port = {}
        connections = {}
        for i in range(N):
            node = chr(i+ 65)
            conn, addr = sock.accept()
            print("Connected with address:", addr)
            connections[node] = conn
            received = conn.recv(1024)
            ip_str, port = unpack_connect(received)
            node_to_ip_port[node] = (ip_str, port)

        send_link_state(connections, node_to_ip_port)

        while (True):
            time.sleep(0.5)
            modified = os.path.getmtime(file_path)
            if modified != modifiedOn:
                modifiedOn = modified
                # print(f"{file_path} has been modified")
                parse(file_path)
                send_link_state(connections, node_to_ip_port)

    except Exception as e:
        print(e)

address = "0.0.0.0"
port = 5000

file_path = "config.txt"
modifiedOn = os.path.getmtime(file_path)
parse(file_path)
sock = socket.create_server((address, port))
start()





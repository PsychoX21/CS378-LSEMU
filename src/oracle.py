# I found this for handling changes in config.txt and using the same idea
# https://devindeep.com/how-to-detect-file-changes-with-python/

import os
import time
import socket
from common.protocol import unpack_connect, pack_link_state, CONNECT_SIZE

graph_dict = {}
N = 0

def recv_full(conn, n):
    buf = b""
    while len(buf) < n:
        result = conn.recv(n - len(buf))
        if not result:
            raise ConnectionError("Didn't receive the full message.")
        buf += result
    return buf

def changed(L1, L2):
    if len(L1) != len(L2):
        return True
    for i in range(len(L1)):
        if L1[i]!=L2[i]:
            return True
    return False

def parse(file_path):
    global graph_dict, N
    sendNodes = set()
    new_graph_dict = {}
    new_N = 0
    with open(file_path, 'r') as f:
        for line in f:
            if line.strip() and not line.strip().startswith('#'):
                current_node = chr(new_N + 65)
                if(new_graph_dict.get(current_node) is None):
                    new_graph_dict[current_node] = []
                neighbor = new_N + 1
                for l in line.strip().split():
                    l = int(l)
                    if l == -1:
                        neighbor += 1
                        continue
                    new_graph_dict[current_node].append((chr(neighbor + 65), l))
                    if new_graph_dict.get(chr(neighbor + 65)) is None:
                        new_graph_dict[chr(neighbor + 65)] = []
                    new_graph_dict[chr(neighbor + 65)].append((current_node, l))
                    neighbor += 1
                new_graph_dict[current_node].sort()
                if graph_dict.get(current_node) is None:
                    sendNodes.add(current_node)
                elif changed(new_graph_dict[current_node], graph_dict[current_node]):
                    sendNodes.add(current_node)
                new_N += 1

    current_node = chr(new_N + 65)
    if(new_graph_dict.get(current_node) is None):
        new_graph_dict[current_node] = []
    new_graph_dict[current_node].sort()
    if graph_dict.get(current_node) is None:
        sendNodes.add(current_node)
    elif changed(new_graph_dict[current_node], graph_dict[current_node]):
        sendNodes.add(current_node) 
    N = new_N + 1
    graph_dict = new_graph_dict
    # print(sendNodes)
    # print(graph_dict)
    return sendNodes

def send_link_state(connections, node_to_ip_port, sendNodes):
    for node in sendNodes:
        records = [(node, node_to_ip_port[node][0], node_to_ip_port[node][1], 0)]
        neighbors = graph_dict[node]
        for neighbor in neighbors:
            neighbor_node = neighbor[0]
            neighbor_ip = node_to_ip_port[neighbor_node][0]
            neighbor_port = node_to_ip_port[neighbor_node][1]
            neighbor_cost = neighbor[1]
            records.append((neighbor_node, neighbor_ip, neighbor_port, neighbor_cost))
        connections[node].sendall(pack_link_state(records))

    print("Sent Link State to : ", end = " ")
    for i in sorted(list(sendNodes)):
        print(i, end = " ")
    print()

def start():
    global modifiedOn,N, sendNodes
    try:
        node_to_ip_port = {}
        connections = {}
        for i in range(N):
            node = chr(i+ 65)
            # print("This prove that", end = " ", flush = True)
            conn, addr = sock.accept()
            # print("accept is Blocking")
            print("Connected with address:", addr)
            connections[node] = conn
            # print("This proves that", end = " ", flush = True)
            received = recv_full(conn, CONNECT_SIZE)
            # print("recv is Blocking")
            ip_str, port = unpack_connect(received)
            node_to_ip_port[node] = (ip_str, port)

        send_link_state(connections, node_to_ip_port, sendNodes)

        while (True):
            time.sleep(0.5)
            modified = os.path.getmtime(file_path)
            if modified != modifiedOn:
                modifiedOn = modified
                time.sleep(1)
                print(f"{file_path} has been modified")
                try:
                    sendNodes = parse(file_path)
                    send_link_state(connections, node_to_ip_port, sendNodes)
                except:
                    pass

    except Exception as e:
        print(e)

address = "0.0.0.0"
port = 5000

file_path = "../config.txt"
modifiedOn = os.path.getmtime(file_path)
sendNodes = parse(file_path)
sock = socket.create_server((address, port))
start()





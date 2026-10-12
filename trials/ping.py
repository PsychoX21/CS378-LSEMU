import socket

address = "0.0.0.0"
port = 6767
myIP = socket.gethostbyname(socket.gethostname())

DATABASE = {
    "PsychoX21" : "........",
    "4N5Hadow" : "hehehehehe",
    "username3" : "password3"
}

with socket.create_server((address, port)) as sock:
    # print(myIP)
    conn, addr = sock.accept()
    print("Connected with " + addr[0] + ":" + str(addr[1]))
    conn.sendall(b"Username?\n")
    received = conn.recv(1024).decode()

    if received in DATABASE:
        conn.sendall(b"Password?\n")
        receivedPassword = conn.recv(1024).decode()

        if receivedPassword == DATABASE[received]:
            conn.sendall(b"ping\n")
            print(f"User {received} login")
        else:
            conn.sendall(b"wrong password\n")
    else:
        print(f"User {received} login failed")
        conn.sendall(b"wrong username\n")

import socket
import struct

ORACLE_PORT = 5000

CONNECT_FMT = "!IH"
LS_HEADER_FMT = "!H"
LS_RECORD_FMT = "!cIHi"
LSP_HEADER_FMT = "!cIH"
LSP_NEIGHBOR_FMT = "!ci"

CONNECT_SIZE = struct.calcsize(CONNECT_FMT)
LS_HEADER_SIZE = struct.calcsize(LS_HEADER_FMT)
LS_RECORD_SIZE = struct.calcsize(LS_RECORD_FMT)
LSP_HEADER_SIZE = struct.calcsize(LSP_HEADER_FMT)
LSP_NEIGHBOR_SIZE = struct.calcsize(LSP_NEIGHBOR_FMT)


def pack_connect(ip, port):
    ip_int = struct.unpack("!I", socket.inet_aton(ip))[0]
    return struct.pack(CONNECT_FMT, ip_int, port)


def unpack_connect(data):
    ip_int, port = struct.unpack_from(CONNECT_FMT, data)
    ip_str = socket.inet_ntoa(struct.pack("!I", ip_int))
    return ip_str, port


def pack_link_state(records):
    buf = bytearray(struct.pack(LS_HEADER_FMT, len(records)))
    for node, ip, port, cost in records:
        ip_int = struct.unpack("!I", socket.inet_aton(ip))[0]
        buf.extend(struct.pack(LS_RECORD_FMT, node.encode(), ip_int, port, cost))
    return bytes(buf)


def unpack_link_state(data):
    num_records, = struct.unpack_from(LS_HEADER_FMT, data)
    offset = LS_HEADER_SIZE
    records = []
    for _ in range(num_records):
        node_b, ip_int, port, cost = struct.unpack_from(LS_RECORD_FMT, data, offset)
        ip_str = socket.inet_ntoa(struct.pack("!I", ip_int))
        records.append((node_b.decode(), ip_str, port, cost))
        offset += LS_RECORD_SIZE
    return records


def pack_lsp(origin, seq, neighbors):
    buf = bytearray(struct.pack(LSP_HEADER_FMT, origin.encode(), seq, len(neighbors)))
    for neighbor, cost in neighbors:
        buf.extend(struct.pack(LSP_NEIGHBOR_FMT, neighbor.encode(), cost))
    return bytes(buf)


def unpack_lsp(data):
    origin_b, seq, num_neighbors = struct.unpack_from(LSP_HEADER_FMT, data)
    offset = LSP_HEADER_SIZE
    neighbors = []
    for _ in range(num_neighbors):
        neighbor_b, cost = struct.unpack_from(LSP_NEIGHBOR_FMT, data, offset)
        neighbors.append((neighbor_b.decode(), cost))
        offset += LSP_NEIGHBOR_SIZE
    return origin_b.decode(), seq, neighbors

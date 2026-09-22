#!/usr/bin/env python3
"""Throwaway #210 sniffer: AF_PACKET capture of RTPS on all interfaces, prints each SEDP
publication (writer 0x000003c2) / subscription (0x000004c2) DATA with topic, type,
PID_TYPE_MAX_SIZE_SERIALIZED (0x0060), PID_DATA_REPRESENTATION (0x0073), and SPDP
(0x000100c2) PID_VENDORID/PID_PRODUCT_VERSION(0x8000)/PID_PROTOCOL_VERSION.
Only works for traffic that goes over UDP (set FASTDDS_BUILTIN_TRANSPORTS=UDPv4).
usage: sniff_sedp.py <seconds>"""
import socket, struct, sys, time

SEC = float(sys.argv[1]) if len(sys.argv) > 1 else 10
s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0003))
s.settimeout(0.5)
seen = set()
end = time.time() + SEC


def params(buf, off, le):
    f = '<' if le else '>'
    out = []
    while off + 4 <= len(buf):
        pid, ln = struct.unpack_from(f + 'HH', buf, off)
        off += 4
        if pid == 1:
            break
        out.append((pid & 0x3fff, buf[off:off + ln]))
        off += ln
    return out


def cdr_string(v, le):
    n = struct.unpack_from('<I' if le else '>I', v, 0)[0]
    return v[4:4 + n - 1].decode(errors='replace')


def handle(rtps):
    if rtps[:4] != b'RTPS':
        return
    vendor = rtps[6:8].hex()
    prefix = rtps[8:20].hex()
    off = 20
    while off + 4 <= len(rtps):
        sid, flags = rtps[off], rtps[off + 1]
        le = bool(flags & 1)
        ln = struct.unpack_from('<H' if le else '>H', rtps, off + 2)[0]
        body = off + 4
        nxt = body + ln if ln else len(rtps)
        if sid == 0x15:  # DATA
            f = '<' if le else '>'
            oq = struct.unpack_from(f + 'H', rtps, body + 2)[0]
            wid = rtps[body + 8:body + 12].hex()
            p = body + 4 + oq
            if flags & 0x02:  # inline qos
                q = params(rtps, p, le)
                # skip inline qos
                while True:
                    pid, l2 = struct.unpack_from(f + 'HH', rtps, p)
                    p += 4 + l2
                    if pid == 1:
                        break
            if wid in ('000003c2', '000004c2', '000100c2') and flags & 0x04:
                enc = rtps[p:p + 2]
                ple = enc == b'\x00\x03'
                pl = params(rtps, p + 4, ple)
                d = {}
                for pid, v in pl:
                    if pid == 0x0005:
                        d['topic'] = cdr_string(v, ple)
                    elif pid == 0x0007:
                        d['type'] = cdr_string(v, ple)
                    elif pid == 0x0060:
                        d['max_size_serialized'] = struct.unpack_from('<I' if ple else '>I', v)[0]
                    elif pid == 0x0073:
                        n = struct.unpack_from('<I' if ple else '>I', v)[0]
                        d['representation'] = [struct.unpack_from('<h' if ple else '>h', v, 4 + 2 * i)[0] for i in range(n)]
                    elif pid == 0x0016:
                        d['vendor'] = v[:2].hex()
                    elif pid == 0x0015:
                        d['protocol'] = '%d.%d' % (v[0], v[1])
                    elif pid == 0x8000 or pid == 0x0000 and False:
                        d['pid_0x8000'] = v.hex()
                    elif pid == 0x005a:
                        d['endpoint_guid'] = v.hex()
                    elif pid == 0x0062:
                        d['entity_name'] = cdr_string(v, ple)
                    elif pid == 0x0075:
                        d['type_information_len'] = len(v)
                d['pids'] = ','.join('%04x' % pid for pid, _ in pl)
                kind = {'000003c2': 'PUB', '000004c2': 'SUB', '000100c2': 'SPDP'}[wid]
                if kind != 'SPDP':
                    d.pop('pids')
                line = '%s src_prefix=%s hdr_vendor=%s %s' % (kind, prefix, vendor, ' '.join('%s=%s' % kv for kv in sorted(d.items())))
                if line not in seen:
                    seen.add(line)
                    print(line, flush=True)
        off = nxt
        if ln == 0:
            break


while time.time() < end:
    try:
        pkt, addr = s.recvfrom(65535)
    except socket.timeout:
        continue
    eth = 14
    if pkt[12:14] != b'\x08\x00':
        continue
    ihl = (pkt[eth] & 0x0f) * 4
    if pkt[eth + 9] != 17:
        continue
    udp = eth + ihl
    if addr[2] == socket.PACKET_OUTGOING:
        continue  # loopback frames appear twice
    try:
        handle(pkt[udp + 8:])
    except Exception as e:  # noqa
        pass

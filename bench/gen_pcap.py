#!/usr/bin/env python3
"""
Generate a pcap of Ethernet/IPv4/UDP frames for benchmarking.

Frame sizes follow RFC 2544 benchmark: SIZE includes the 4-byte FCS,
which is not stored in the pcap so `--size 64` writes 60-byte frames, the
smallest legal Ethernet frame, aka the worst case for a sniffer.

UDP source ports vary across packets so the flows are not all identical.
No dependencies beyond the standard library.

usage: gen_pcap.py --size 64 --count 10000 -o /tmp/udp64.pcap
"""
import argparse
import struct

def ip_checksum(hdr: bytes) -> int:
    s = sum(struct.unpack("!%dH" % (len(hdr) // 2), hdr))
    s = (s >> 16) + (s & 0xFFFF)
    s += s >> 16
    return ~s & 0xFFFF


def frame(size: int, sport: int) -> bytes:
    l2_len = size - 4                     # strip FCS
    payload_len = l2_len - 14 - 20 - 8    # eth + ipv4 + udp
    if payload_len < 0:
        raise ValueError("size must be >= 64")

    eth = bytes.fromhex("ffffffffffff") + bytes.fromhex("020000000001") + b"\x08\x00"
    ip_total = 20 + 8 + payload_len
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, ip_total, 0, 0x4000, 64, 17, 0,
                     bytes([10, 99, 0, 1]), bytes([10, 99, 0, 2]))
    ip = ip[:10] + struct.pack("!H", ip_checksum(ip)) + ip[12:]
    udp = struct.pack("!HHHH", sport, 9, 8 + payload_len, 0)   # port 9 = discard
    return eth + ip + udp + bytes(payload_len)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--size", type=int, required=True, help="frame size incl. FCS (64..1518)")
    ap.add_argument("--count", type=int, default=10000, help="packets in the file")
    ap.add_argument("-o", "--output", required=True)
    args = ap.parse_args()

    with open(args.output, "wb") as f:
        # classic pcap, microsecond timestamps, LINKTYPE_ETHERNET
        f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 1))
        for i in range(args.count):
            pkt = frame(args.size, 10000 + (i % 50000))
            f.write(struct.pack("<IIII", i // 1000000, i % 1000000, len(pkt), len(pkt)))
            f.write(pkt)


if __name__ == "__main__":
    main()

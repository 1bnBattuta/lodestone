# Lodestone

Lodestone is a single-threaded Linux packet sniffer written in C. It captures through `AF_PACKET` with a `TPACKET_V3 memory-mapped ring`: the kernel writes packets straight into memory shared with the sniffer, so there is no system call nor copy per packet. Captures are saved as nanosecond-resolution **pcap** files that Wireshark and tcpdump can open.
It has no dependencies beyond libc and the kernel headers.

## Performance at a glance

Measured on a laptop (Intel Core i7-5500U, 2 cores / 4 threads, Linux 7.2) over a virtual Ethernet link. Details and method are in [Benchmark](#benchmark).

- **0 packets lost at 1 Gbps line rate** with full-size (1518 B) frames, sustained for 30 s (2.44 M packets), using **~6 % of one CPU core**
- **0 packets lost over 29.4 M minimum-size (64 B) packets** in a 3-minute run
- **0 loss at 309 k packets/s** with 64 B frames, the highest rate the traffic generator reached on this machine, using **3.4 % of one core**

## How it works

```
 NIC / veth ─► kernel RX path ─► AF_PACKET ─► TPACKET_V3 ring: 64 blocks × 4 MiB, shared (mmap)
                                                 │ block full, or 60 ms timeout
                                                 │ status = TP_STATUS_USER
                                                 ▼
                              lodestone: poll() ─► read every packet in the block ─► pcap / stdout
                                                 │ status = TP_STATUS_KERNEL
                                                 └─► block handed back to the kernel
```

- **Block-based ring (TPACKET_V3).** The kernel packs variable-length packets into 4 MiB blocks and hands over a whole block at once. One `poll()` wake-up covers thousands of packets, and small frames don't waste a fixed-size slot as they would with TPACKET_V2.
- **Lock-free hand-off.** Ownership of each block is a single status word. Lodestone reads it with an acquire load and returns the block with a release store, so packet data is never read before the kernel has finished writing it or after the block is given back.
- **Retire timeout.** `tp_retire_blk_tov = 60 ms` makes the kernel return half-filled blocks, so packets on a slow link still arrive eventually.
- **Clean start.** The socket is opened with protocol 0 and only starts receiving at `bind()` on the chosen interface, so no traffic from other interfaces leaks in during setup (painful debug session there).
- **Accurate statistics.** On exit, Lodestone reads `PACKET_STATISTICS` and reports packets seen by the kernel, packets captured and packets dropped. Seen = captured + dropped.

Design notes and trade-offs: [`docs/design.md`](docs/design.md).

## Build

Requires Linux, gcc and make.

```sh
make            # debug build with ASan + UBSan  -> build/debug/lodestone-capture
make release    # optimized build (-O2)          -> build/release/lodestone-capture
make test       # build and run the tests
```

Use `make release` for real captures and benchmarks. The sanitizers in the debug build will skew the CPU cost per packet.

## Usage

Capturing needs `CAP_NET_RAW` which usually means root.

```sh
# save to a pcap file
sudo ./build/release/lodestone-capture -i eth0 -o capture.pcap

# without -o: print each packet to the terminal (timestamp, lengths, MAC addresses, EtherType)
sudo ./build/release/lodestone-capture -i eth0

# -P: promiscuous mode, also capture traffic not addressed to this machine
sudo ./build/release/lodestone-capture -i eth0 -P -o capture.pcap
```

Stop with Ctrl+C. Statistics are printed to stderr:

```
2440000 packets seen by kernel
2440000 packets captured (3694160000 bytes)
0 packets dropped by kernel (0.00%)
```

```
Options:
  -i, --interface <name>  Network interface to capture on
  -h, --help              Show this help menu and exit
  -P, --promiscuous       Enable NIC Promiscuous mode
      --af_xdp            Run in AF_XDP mode
  -o, --output <name>     Output file
```

Supported link types: Ethernet, loopback, and raw-IP interfaces such as tun and WireGuard. `--af_xdp` is reserved and not implemented yet.

## Benchmark

### Method

A sniffer's job is to see every packet, so the metric is **packet loss at a given traffic rate**. Rates are given in packets per second as well as Gbps, because the work is per packet. At 1 Gbps, full-size frames arrive at 81 k packets/s, while 64 B frames arrive at 1.49 M packets/s, 18 times more.

`bench/bench.sh` creates a virtual Ethernet link (a veth pair). It captures on one end with Lodestone while [`tcpreplay`](https://tcpreplay.appneta.com/) sends synthetic UDP traffic into the other, then compares packets sent with packets captured and with the kernel's drop counter.

- Frame sizes follow [RFC 2544](https://www.rfc-editor.org/rfc/rfc2544): 64 B is the smallest Ethernet frame, 1518 B the largest.
- Output goes to `/dev/null`, so the test measures capture, not the disk.
- CPU time is measured with `time`, minus an idle run that isolates the startup cost (mapping and locking the 256 MiB ring).

### Results

Intel Core i7-5500U @ 2.40 GHz (2 cores / 4 threads), Linux 7.2.4, `performance` CPU governor.

| Traffic | Offered rate | Sent | Captured | Dropped | Loss | CPU (one core) |
|---|---:|---:|---:|---:|---:|---:|
| 1518 B frames, 30 s | 81.0 k pkt/s (1 Gbps line rate) | 2,440,000 | 2,440,000 | 0 | **0 %** | 5.8 % |
| 64 B frames, 1 sender | 173 k pkt/s | 5,000,000 | 5,000,000 | 0 | **0 %** | 1.3 % |
| 64 B frames, 3 senders | 309 k pkt/s | 15,000,000 | 15,000,000 | 0 | **0 %** | 3.4 % |
| 64 B frames, ~3 min | 170 k pkt/s | 29,385,004 | 29,385,004 | 0 | **0 %** | – |

Measured CPU cost is roughly **75–110 ns per packet** for 64 B frames.

### Reading these numbers

- **The traffic generator is the bottleneck.** Three parallel `tcpreplay` instances topped out at 309 k pkt/s, with Lodestone at 3.4 % of one core and no drops. Lodestone's actual limit on this machine is higher and hasn't been measured yet.
- **1 Gbps line rate with 64 B frames (1.49 M pkt/s) was not reached,** because the generator could not produce it. Further benchmarks will target this.
- **veth is not a physical NIC.** It exercises the kernel's software receive path and AF_PACKET, but not driver nor interrupt. The generator also shares the CPU with the sniffer.

### Reproduce

```sh
sudo pacman -S tcpreplay        # Debian/Ubuntu: sudo apt install tcpreplay
make release
sudo bash bench/bench.sh        # close to 2 minutes, cleans up after itself
```

## Project layout

```
src/capture/main.c         capture loop, block walker, statistics
src/capture/packet_mmap.*  AF_PACKET socket, TPACKET_V3 ring setup/teardown, promiscuous mode, link type
src/capture/pcap.*         pcap writer (nanosecond timestamps)
src/capture/output.*       output: pcap file or readable text on stdout
src/common/args.*          small table-driven command-line parser
bench/                     traffic generator (gen_pcap.py) and benchmark script (bench.sh)
docs/design.md             design document
```

## Roadmap

- [ ] `lodestone-parse`: pcap to JSONL decoder (Ethernet, IPv4/6, TCP, UDP, ICMP, ARP), fuzz-tested
- [ ] Separate writer thread, so slow disk writes don't cause ring drops
- [ ] Configurable ring size and snap length
- [ ] pcap streaming to stdout (`-o -`) for `lodestone-capture | lodestone-parse`
- [ ] Faster traffic generation (kernel `pktgen`) to test 64 B line rate
- [ ] Hardware timestamps

## References

- [packet_mmap.rst](https://www.kernel.org/doc/Documentation/networking/packet_mmap.rst): kernel documentation for PACKET_MMAP / TPACKET_V3
- [`net/packet/af_packet.c`](https://github.com/torvalds/linux/blob/master/net/packet/af_packet.c)
- [packet(7)](https://man7.org/linux/man-pages/man7/packet.7.html), [RFC 2544](https://www.rfc-editor.org/rfc/rfc2544)

## License

See [LICENSE](LICENSE).
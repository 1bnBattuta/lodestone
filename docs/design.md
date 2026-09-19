# Lodestone architecture

## Executive summary
Lodestone is a single threaded packet sniffer based on AF_PACKET kernel API with zero-copy memory ring buffer to communicate packets directly between kernel space and user space (TPACKET_V3 mmap).

## Architecture

Lodestone is composed of two components : 
 - lodestone-capture: Opens a AF_PACKET socket with TPACKET_V3 mmap ring buffer interface. It then reads frames and writes pcap entries to stdout (default) or a file via -w (-w file.pcap). Reports drop counts and basic stats to stderr. Requires superuser privileges.
 - lodestone-parse: reads pcap from stdin of a file (-r file.pcap), decodes protocols, and exports a JSONL to stdout. One packet per line, ISO 8601 UTC timestamps, nested per-layer structure, optional hex payload. No privileges required.

The reason behind this split is privilege separation (parser does not need root priv). Also, separation makes it way more easier to test the parser using dedicated fuzzing pcaps. Another reason is being able to reuse each component independently. And finally: Do one thing and do it well.

Typical Invocations:
 - `lodestone-capture -i eth0 -w capture.pcap` then `lodestone-parse -r capture.pcap > events.jsonl` for forensic
 - `lodestone-capture -i eth0 | lodestone-parse > events.jsonl` (live streaming)
 - `lodestone-parse -r some-fuzz-corpus.pcap` (testing / development)



```
lodestone/
├── Makefile
├── README.md
├── docs/design.md
├── src/
│   ├── capture/          # everything for lodestone-capture
│   ├── parse/            # everything for lodestone-parse
│   └── common/           # pcap format, shared utils
├── tests/
└── fuzz/                 # later
```

## Capture side (lodestone-capture)

### Kernel interface: AF_PACKET + TPACKET_V3
Why use PACKET_MMAP? Plain AF_PACKET requires one system call to capture a packet, another call is needed to fetch a timestamp. It also uses very limited buffers. This makes AF_PACKET very inefficient for high throughput needs such as packet sniffing.

PACKET_MMAP on the other hand provides a size configurable circular buffer  mapped in user space that can be used to receive packets. That means reading packets becomes a memory operation instead of a delegated system call.

The shared memory buffer is composed of blocks, each block contains a number of frames holding a single packet.

What makes TPAKCET_V3 the adapted choice here is the fact that the size of frames is not fixed. In v2, the user must specify a fixed frame size that should hold all type of different packets, this introduces a significant memory waste. 
V3 solves this by bypassing frames and storing packets directly in blocks, these blocks are then send directly to user space.
In addition, v3 introduces a per-block release timeout `tp_retire_blk_tov` so the kernel doesn't hold partially-filled blocks indefinitely on quiet links. 
Those features makes v3 much more performant hence our choice.

The use of PACKET_MMAP involves the following process:
 - Setup: socket creation and configuration -> allocation of the circular buffer -> mapping of the allocated buffer to user process.
 - Capture: poll() to wait for incoming packets
 - Shutdown: close() destruction of the capture socket and deallocation of all associated resources.

### Ring configuration

The following parameters define the ring buffer:

 - tp_block_size:    Minimal size of contiguous block
 - tp_block_nr:      Number of blocks
 - tp_frame_size:	   Size of frame
 - tp_frame_nr:	   Total number of frames
 - tp_retire_blk_tov timeout in msecs

`tp_frame_nr` is redundant: `tp_frame_nr = tp_block_nr * (tp_block_size / tp_frame_size)`.
Both frame parameters don't influence the performance of the engine. `tp_frame_nr is genuinely` redundant, kernel just checks consistency. But `tp_frame_size` in V3 acts as the snap length: if a packet exceeds tp_frame_size: header_overhead, the kernel truncates it. This filters out Jumbo packets which are out of scope.
We take the following value from the provided example in packet_mmap.rst:
`tp_frame_size = 1 << 11` 2048 bytes.
`tp_block_size = 1 << 22` 4 MiB.
`tp_block_nr = 64`.
`tp_retire_blk_tov = 60` 60 msec.

Ring parameters follow the kernel packet_mmap.rst V3 example. This gives 256 MiB of ring. These are placeholder defaults; if benchmarking reveals drops or excess latency, revisit.

Additionally, ring mmap must have the following options `PROT-READ | PROT-WRITE` because the program must zero block after reading packets and before sending them back to the kernel.

### Timestamp source
The provided AF_PACKET default timestamp is drawn from the kernel's CLOCK_REALTIME. Because of that, consumer must not assume monotonicity.
Optional NIC hardware timestamp is to be added.

### Drop reporting
The engine will forward TPACKET_V3 stats to stderr on shutdown, additional stats will be added for protocols and rates.

### Promiscuous mode
Promiscuous mode enables the NIC to pass all network traffic it receives to the kernel rather than dropping packets not sent to the host.
This mode can be enabled using the -P flag, disabled by default. 

### Shutdown
To terminate the capture, send a SIGINT signal to the program (Ctl + C). When receiving said signal, the engine frees all allocated memory and close the socket.

### Output
As mentioned, Lodestone outputs pcap records to stdout, -w flag can be used to write into a file.

## Parse side (lodestone-parse)

### Scope
The parser supports the following protocols (to be implemented) : Ethernet, IPv4, IPv6, TCP, UDP, ICMP, ARP.

### JSON schema
Each line represented a packet instead of a single array because the latter risks a file corruption if the parser crashes in the last packet.
Each line contains a nested per-layer parse of the packet with an ISO 8601 UTC timestamp. A dedicated hex payload is also supported via the -h flag.

### Error model
Whether to skip or to partially record a malformed packet is to be determined.


## Non-goals for Lodestone
Multithreading, AF_XDP, jumbo frames, monitor mode, live decode.

## Learning goals coverage
 - The kernel-userspace interface for packet capture
 - Network protocol layout in memory
 - The Linux networking stack topology
 - Performance reasoning in systems C
 - Defensive C against adversarial input

## References
 - https://www.kernel.org/doc/Documentation/networking/packet_mmap.rst
 - https://github.com/torvalds/linux/blob/master/net/packet/af_packet.c
 - https://man7.org/linux/man-pages/man2/socket.2.html
 - https://man7.org/linux/man-pages/man7/packet.7.html
 - https://man7.org/linux/man-pages/man2/mmap.2.html
 - https://csulrong.github.io/blogs/2022/03/10/linux-afpacket/
 - https://man7.org/linux/man-pages/man2/poll.2.html
 - https://blog.cloudflare.com/a-debugging-story-corrupt-packets-in-af_xdp-kernel-bug-or-user-error/
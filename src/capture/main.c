/**
 * @file main.c
 * @author Omar Merroun
 * @brief lodestone-capture's entry point
 * @version 0.1
 * @date 2026-09-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/types.h>

#include "../common/args.h"
#include "output.h"
#include "packet_mmap.h"
#include "pcap.h"

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

// Global config
typedef struct {
    int af_xdp;         // Address family: 0 AF_PACKET, 1 AF_XDP
    int show_help;
    int promisc;
    const char *interface_name;
    const char *output_file;
} config_t;

static config_t cfg = {
    .af_xdp = 0,
    .show_help = 0,
    .interface_name = NULL,
    .output_file = NULL,
    .promisc = 0
};

// Supported command line arguments
const arg_opt_t opts[] = {
    {'i', "interface", ARG_STRING, &cfg.interface_name, NULL, "Network interface to capture on", "name"},
    {'h', "help", ARG_FLAG, &cfg.show_help, NULL, "Show this help menu and exit", NULL},
    {'P', "promiscuous", ARG_FLAG, &cfg.promisc, NULL, "Enable NIC Promiscuous mode", NULL},
    {0, "af_xdp", ARG_FLAG, &cfg.af_xdp, NULL, "Run in AF_XDP mode", NULL},
    {'o', "output", ARG_STRING, &cfg.output_file, NULL, "Output file", "name"},
    ARG_END
};

static unsigned long packets_total = 0, bytes_total = 0;
static volatile sig_atomic_t sigint = 0;

static void sighandler(int num) {
    sigint = 1;
}

static int walk_block(struct tpacket_block_desc *pbd, output_cfg_t *out_cfg) {
    unsigned long bytes = 0;
    int num_pkts = pbd->hdr.bh1.num_pkts;
    struct tpacket3_hdr *ppd;
    ppd = (struct tpacket3_hdr *) ( (uint8_t *) pbd + pbd->hdr.bh1.offset_to_first_pkt);

    for (int i = 0; i < num_pkts; ++i) {
        bytes += ppd->tp_snaplen;

        uint32_t caplen = ppd->tp_snaplen;
        if (caplen > out_cfg->snaplen)
            caplen = out_cfg->snaplen;

        pcap_rec_hdr_t hdr = {
            .ts_sec = ppd->tp_sec,
            .ts_nsec = ppd->tp_nsec,
            .captured_len = caplen,
            .original_len = ppd->tp_len,
        };

        uint8_t *data = (uint8_t *) ppd + ppd->tp_mac; 
        if (output_write(out_cfg, &hdr, data) < 0) {
            return -1;
        }
        ppd = (struct tpacket3_hdr *) ( (uint8_t *) ppd + ppd->tp_next_offset);
    }

    packets_total += num_pkts;
    bytes_total += bytes;

    return 0;
}

int main(int argc, char **argv)
{
    int err;

    err = args_parse(argc, argv, opts);
    if (err < 0) {
        fprintf(stderr, "argument parser\n");
        return EXIT_FAILURE;
    }

    if (cfg.show_help) {
        args_print_help(argv[0], " [OPTIONS]", opts);
        return 0;
    }

    if (cfg.af_xdp) {
        fprintf(stderr, "AF_XDP is not yet supported\n");
        return EXIT_FAILURE;
    }

    if (cfg.interface_name == NULL) {
        fprintf(stderr, "Interface name must be provided\n");
        args_print_help(argv[0], " [OPTIONS]", opts);
        return EXIT_FAILURE;
    }

    struct sigaction sa;
    sa.sa_handler = sighandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    } 

    struct tpacket_ring ring;
    memset(&ring, 0, sizeof(ring));
    int fd = tpacket_setup(&ring, cfg.interface_name);
    if (fd < 0) {
        perror("tpacket_setup");
        exit(EXIT_FAILURE);
    }

    if (cfg.promisc) {
        err = tpacket_promisc(fd, cfg.interface_name);
        if (err < 0) {
            perror("setsockopt");
            return EXIT_FAILURE;
        }
    }

    int linktype = tpacket_linktype(fd);
    if (linktype < 0) {
        fprintf(stderr, "%s: unsupported link type\n", cfg.interface_name);
        tpacket_teardown(&ring, fd);
        return EXIT_FAILURE;
    }

    output_cfg_t out_cfg = {
        .pcap_path = cfg.output_file,              /* NULL if no -o */
        .display   = (cfg.output_file == NULL),    /* like tcpdump -w: file or screen */
        .snaplen   = PCAP_DEFAULT_SNAPLEN,
        .linktype  = (uint32_t)linktype,
    };

    if (output_open(&out_cfg) < 0) {
        perror(cfg.output_file);                /* fopen sets errno */
        tpacket_teardown(&ring, fd);
        return EXIT_FAILURE;
    }

    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    struct tpacket_block_desc *pbd;
    unsigned int block_num = 0, blocks = ring.req.tp_block_nr;
    while (likely(!sigint)) {
        pbd = (struct tpacket_block_desc *) ring.rd[block_num].iov_base;

        if (!tpacket_block_ready(pbd)) {
            err = poll(&pfd, 1, -1);
            if (err < 0 && errno != EINTR) {
                perror("poll");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        if (walk_block(pbd, &out_cfg) < 0) {
            perror("write");
            break;  // To release resources and prints stats
        }
        tpacket_block_flush(pbd);
        block_num = (block_num + 1) % blocks;
    }

    struct tpacket_stats_v3 stats;
    socklen_t len = sizeof(stats);
    err = getsockopt(fd, SOL_PACKET, PACKET_STATISTICS, &stats, &len);
    if (err < 0) {
        perror("getsockopt");
        exit(1);
    }

    fflush(stdout);
    printf("\nReceived %u packets, %lu bytes, %u dropped, freeze_q_cnt: %u\n",
    stats.tp_packets, bytes_total, stats.tp_drops,
    stats.tp_freeze_q_cnt);

    output_close(&out_cfg);

    tpacket_teardown(&ring, fd);
    return 0;
}

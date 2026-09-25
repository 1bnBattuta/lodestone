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
#include <linux/if_ether.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>

#include "../common/args.h"
#include "packet_mmap.h"

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
    const char *interface_name;
    int promisc;
} config_t;

static config_t cfg = {
    .af_xdp = 0,
    .show_help = 0,
    .interface_name = NULL,
    .promisc = 0
};

// Supported command line arguments
const arg_opt_t opts[] = {
    {'i', "interface", ARG_STRING, &cfg.interface_name, NULL, "Network interface to capture on", "name"},
    {'h', "help", ARG_FLAG, &cfg.show_help, NULL, "Show this help menu and exit", NULL},
    {'P', "promiscuous", ARG_FLAG, &cfg.promisc, NULL, "Enable NIC Promiscuous mode", NULL},
    {0, "af_xdp", ARG_FLAG, &cfg.af_xdp, NULL, "Run in AF_XDP mode", NULL},
    ARG_END
};

static unsigned long packets_total = 0, bytes_total = 0;
static volatile sig_atomic_t sigint = 0;

static void sighandler(int num) {
    sigint = 1;
}

// Small parser for Ethernet
static void display(struct tpacket3_hdr *ppd) {
    struct ethhdr *eth = (struct ethhdr *) ( (uint8_t *) ppd + ppd->tp_mac);
    
    printf("Source address: %02x:%02x:%02x:%02x:%02x:%02x\n", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);
    printf("Destination address: %02x:%02x:%02x:%02x:%02x:%02x\n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
    printf("Packet Type: %x\n\n", ntohs(eth->h_proto));
}

static void walk_block(struct tpacket_block_desc *pbd) {
    unsigned long bytes = 0;
    int num_pkts = pbd->hdr.bh1.num_pkts;
    struct tpacket3_hdr *ppd;
    ppd = (struct tpacket3_hdr *) ( (uint8_t *) pbd + pbd->hdr.bh1.offset_to_first_pkt);

    for (int i = 0; i < num_pkts; ++i) {
        bytes += ppd->tp_snaplen;

        display(ppd);
        ppd = (struct tpacket3_hdr *) ( (uint8_t *) ppd + ppd->tp_next_offset);
    }

    packets_total += num_pkts;
    bytes_total += bytes;
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
        fprintf(stdout, "AF_XDP is not yet supported\n");
        return EXIT_FAILURE;
    }

    if (cfg.interface_name == NULL) {
        fprintf(stderr, "Interface name must be provided\n");
        args_print_help(argv[0], " [OPTIONS]", opts);
        return 1;
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
            return -1;
        }
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

        walk_block(pbd);
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

    tpacket_teardown(&ring, fd);
    return 0;
}

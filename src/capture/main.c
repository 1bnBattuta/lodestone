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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>

#include "packet_mmap.h"

#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

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

int main(int argc, char **argp)
{
    int err;
    if (argc != 2) {
        fprintf(stderr, "Usage: %s INTERFACE\n", argp[0]);
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
    int fd = tpacket_setup(&ring, argp[argc - 1]);
    if (fd < 0) {
        perror("tpacket_setup");
        exit(EXIT_FAILURE);
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

        if ((pbd->hdr.bh1.block_status & TP_STATUS_USER) == 0 ) {
            err = poll(&pfd, 1, -1);
            if (err < 0 && errno != EINTR) {
                perror("poll");
                exit(EXIT_FAILURE);
            }
            continue;
        }

        walk_block(pbd);
        tpacket_flush_block(pbd);
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

/**
 * @file packet_mmap.c
 * @author Omar Merroun
 * @brief AF_PACKET socket implementation
 * @version 0.1
 * @date 2026-09-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "packet_mmap.h"
#include <net/if_arp.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

int tpacket_setup(struct tpacket_ring *ring, const char *ifname) {
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int err = setsockopt(fd, SOL_PACKET, PACKET_VERSION, &TPACKET_VERSION, sizeof(TPACKET_VERSION));
    if (err < 0) {
        perror("setsockopt");
        return -1;
    }

    memset(&ring->req, 0, sizeof(ring->req));
    ring->req.tp_block_size = BLOCK_SIZE;
    ring->req.tp_block_nr = BLOCK_NR;
    ring->req.tp_frame_size = FRAME_SIZE;
    ring->req.tp_frame_nr = (ring->req.tp_block_size * ring->req.tp_block_nr) / ring->req.tp_frame_size;
    ring->req.tp_retire_blk_tov = RETIRE_BLK_TOV;
    err = setsockopt(fd, SOL_PACKET, PACKET_RX_RING, &ring->req, sizeof(ring->req));
    if (err < 0) {
        perror("setsockopt");
        return -1;
    }

    unsigned int total_size = ring->req.tp_block_size * ring->req.tp_block_nr;
    ring->map = mmap(NULL, total_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_LOCKED, fd, 0);
    if (ring->map == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    ring->rd = malloc(ring->req.tp_block_nr * sizeof(*ring->rd));
    if (ring->rd == NULL) {
        perror("malloc");
        return -1;
    }

    for (unsigned int i = 0; i < ring->req.tp_block_nr; ++i) {
        ring->rd[i].iov_base = ring->map + (i * ring->req.tp_block_size);
        ring->rd[i].iov_len = ring->req.tp_block_size;
    }

    struct sockaddr_ll ll;
    memset(&ll, 0, sizeof(ll));
    ll.sll_family = PF_PACKET;
    ll.sll_protocol = htons(ETH_P_ALL);
    ll.sll_ifindex = if_nametoindex(ifname);

    err = bind(fd, (struct sockaddr *)&ll, sizeof(ll));
    if (err < 0) {
        perror("bind");
        return -1;
    }

    return fd;
}

void tpacket_teardown(struct tpacket_ring *ring, int fd) {
    free(ring->rd);
    munmap(ring->map, ring->req.tp_block_nr * ring->req.tp_block_size);
    close(fd);
}

int tpacket_promisc(int fd ,const char *ifname) {
    struct packet_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    
    mreq.mr_ifindex = if_nametoindex(ifname);
    mreq.mr_type = PACKET_MR_PROMISC;
    
    int err = setsockopt(fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if (err < 0) {
        perror("setsockopt: Promiscuous mode setup");
        return -1;
    }
    return 0;
}

int tpacket_linktype(int fd) {
    struct sockaddr_ll sll;
    socklen_t len = sizeof(sll);

    if (getsockname(fd, (struct sockaddr *)&sll, &len) < 0) {
        return -1;
    }

    switch (sll.sll_hatype) {
        case ARPHRD_ETHER:
        case ARPHRD_LOOPBACK:   /* Linux loopback uses a zero Ethernet header */
            return 1;           /* LINKTYPE_ETHERNET */
        case ARPHRD_NONE:       /* like: tun and WireGuard. no link header */
            return 101;         /* LINKTYPE_RAW */
        default:
            return -1;
    }
}

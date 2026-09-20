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

int tpacket_setup(struct tpacket_ring *ring, char *ifname) {
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

void tpacket_flush_block(struct tpacket_block_desc *pbd) {
    pbd->hdr.bh1.block_status = TP_STATUS_KERNEL;
}

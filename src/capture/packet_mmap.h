/**
 * @file packet_mmap.h
 * @author Omar Merroun
 * @brief AF_PACKET/PACKET_MMAP socket API
 * @version 0.1
 * @date 2026-09-19
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef LS_PACKET_MMAP_H
#define LS_PACKET_MMAP_H

#include <linux/if_packet.h>
#include <stdint.h>
#include <sys/uio.h>

// Maybe changed to macros when adding a dynamic way to set 
// ring buffer dimensions.
static const int TPACKET_VERSION = TPACKET_V3;
static const unsigned int BLOCK_SIZE = 1 << 22; // 4MiB
static const unsigned int BLOCK_NR = 64;
static const unsigned int FRAME_SIZE = 2048;
static const unsigned int RETIRE_BLK_TOV = 60; //msec


/** TPACKET_V3 ring buffer context */
struct tpacket_ring {
    struct iovec *rd;       /**< A pointer to an iovec array mapping the
                                user memory to the kernel ring blocks*/
    uint8_t *map;           /**< A pointer to the mmap region*/
    struct tpacket_req3 req;/**< Configuration struct*/
};

/**
 * \brief create and configure both tpacketv3 socket and mmap.
 * 
 * \param ring pointer to the context struct to be populated
 * \param ifname interface name
 * \return file descriptor of the created socket
 */
int tpacket_setup(struct tpacket_ring *ring, const char *ifname);

/**
 * \brief frees allocated memory, unmap shared memory and closes the socket
 * 
 * \param ring pointer to context struct
 * \param fd socket file descriptor
 */
void tpacket_teardown(struct tpacket_ring *ring, int fd);

/**
 * \brief Checks if a block is released by the kernel to userspace
 * 
 * Uses gcc/clang built-in atomic load to prevent the program from reading
 * the unreleased block (nothing after this can move before it)
 * \param pbd block descriptor
 * \return int whether the block is ready or not
 */
static inline int tpacket_block_ready(struct tpacket_block_desc *pbd) {
    return (__atomic_load_n(&pbd->hdr.bh1.block_status, __ATOMIC_ACQUIRE)
            & TP_STATUS_USER) != 0;
}

/**
 * \brief sends a processed block back to the kernel
 *
 * Uses gcc/clang built-in atomic store to ensure all reads of the block
 * complete before the kernel can reuse it.
 * (nothing before this can move after it)
 * \param pbd block descriptor
 */
static inline void tpacket_block_flush(struct tpacket_block_desc *pbd) {
    __atomic_store_n(&pbd->hdr.bh1.block_status, TP_STATUS_KERNEL,
                        __ATOMIC_RELEASE);
}

int tpacket_promisc(int fd ,const char *ifname);

#endif // LS_PACKET_MMAP_H

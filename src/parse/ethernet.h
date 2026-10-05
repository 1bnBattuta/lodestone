/**
 * \file ethernet.h
 * \author Omar Merroun
 * \brief Ethernet header parser API
 * \version 0.1
 * \date 2026-10-05
 * 
 * \copyright Copyright (c) 2026
 */

#ifndef LS_ETHERNET_H
#define LS_ETHERNET_H

#include <netinet/in.h>
#include <stdint.h>

#define ETH_ADDR_LEN    6u
#define ETH_HDR_LEN     14u

typedef enum {
    ipv4    = 0x0800,
    arp     = 0x0806,
    ipv6    = 0x86DD,
} ether_type_t;

typedef struct {
    unsigned char mac_dst[ETH_ADDR_LEN];
    unsigned char mac_src[ETH_ADDR_LEN];
    uint16_t ether_type;
} ls_eth_hdr;

#endif // LS_ETHERNET_H

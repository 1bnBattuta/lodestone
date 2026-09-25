/**
 * @file pcap.c
 * @author Omar Merroun
 * @brief pcap writer implementation
 * @version 0.1
 * @date 2026-09-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "pcap.h"

FILE *pcap_file_create(const char *filename) {
    FILE *file = fopen(filename, "wb");
    return file;
}

int pcap_file_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype) {
    pcap_hdr_t hdr = {
        .magic_number = PCAP_MAGIC_NSEC,
        .major_ver    = 2,
        .minor_ver    = 4,
        .reserved1    = 0,
        .reserved2    = 0,
        .snaplen      = snaplen,
        .linktype     = linktype,
    };

    if (fwrite(&hdr, sizeof(pcap_hdr_t), 1, fp) != 1) {
        return -1;
    }

    return 0;
}

int pcap_file_write_packet(FILE *fp, const uint8_t *data,
                           uint32_t caplen, uint32_t origlen,
                           uint32_t ts_sec, uint32_t ts_nsec)
{
    pcap_rec_hdr_t rec = {
        .ts_sec   = ts_sec,
        .ts_nsec  = ts_nsec,
        .captured_len = caplen,
        .original_len = origlen,
    };
    
    if (fwrite(&rec, sizeof(pcap_rec_hdr_t), 1, fp) != 1) {
        return -1;
    }

    if (fwrite(data, 1, caplen, fp) != caplen) {
        return -1;
    }
    
    return 0;
}

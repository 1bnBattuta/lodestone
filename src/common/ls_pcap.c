/**
 * \file ls_pcap.c
 * \author Omar Merroun
 * \brief pcap writer implementation
 * \version 0.1
 * \date 2026-09-25
 * 
 * \copyright Copyright (c) 2026
 * 
 */

#include "ls_pcap.h"

FILE *ls_pcap_file_create(const char *filename) {
    FILE *file = fopen(filename, "wb");
    return file;
}

int ls_pcap_file_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype) {
    ls_pcap_hdr_t hdr = {
        .magic_number = LS_PCAP_MAGIC_NSEC,
        .major_ver    = 2,
        .minor_ver    = 4,
        .reserved1    = 0,
        .reserved2    = 0,
        .snaplen      = snaplen,
        .linktype     = linktype,
    };

    if (fwrite(&hdr, sizeof(ls_pcap_hdr_t), 1, fp) != 1) {
        return -1;
    }

    return 0;
}

int ls_pcap_file_write_packet(FILE *fp, const uint8_t *data, const ls_pcap_rec_hdr_t *rec_hdr)
{   
    if (fwrite(rec_hdr, sizeof(ls_pcap_rec_hdr_t), 1, fp) != 1) {
        return -1;
    }

    if (fwrite(data, 1, rec_hdr->captured_len, fp) != rec_hdr->captured_len) {
        return -1;
    }
    
    return 0;
}

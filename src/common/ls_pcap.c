/**
 * \file ls_pcap.c
 * \author Omar Merroun
 * \brief stateless pcap encoder implementation
 * \version 0.1
 * \date 2026-09-25
 * 
 * \copyright Copyright (c) 2026
 */

#include <errno.h>

#include "ls_pcap.h"

FILE *ls_pcap_file_create(const char *filename) {
    FILE *file = fopen(filename, "wb");
    return file;
}

int ls_pcap_rec_hdr_check(const ls_pcap_rec_hdr_t *rec_hdr, uint32_t snaplen) {
    if (rec_hdr->ts_nsec >= LS_PCAP_NSEC_PER_SEC)
        return -1;
    if (rec_hdr->captured_len > snaplen ||
        rec_hdr->captured_len > rec_hdr->original_len)
        return -1;
    return 0;
}

int ls_pcap_file_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype) {
    if (fp == NULL) {
        return -1;
    }
    
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
    if (fp == NULL || rec_hdr == NULL ||
        (data == NULL && rec_hdr->captured_len != 0)) {
        errno = EINVAL;
        return -1;
    }

    if (fwrite(rec_hdr, sizeof(*rec_hdr), 1, fp) != 1) {
        return -1;
    }

    if (rec_hdr->captured_len != 0 &&
        fwrite(data, 1, rec_hdr->captured_len, fp) != rec_hdr->captured_len)
        return -1;

    return 0;
}

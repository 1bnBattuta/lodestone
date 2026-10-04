/**
 * \file ls_pcap.c
 * \author Omar Merroun
 * \brief stateless pcap encoder/decoder implementation
 * \version 0.1
 * \date 2026-09-25
 * 
 * \copyright Copyright (c) 2026
 */

#include <errno.h>

#include "ls_bswap.h"
#include "ls_pcap.h"


static void bswap_pcap_hdr(ls_pcap_hdr_t *hdr) {
    hdr->magic_number   = ls_bswap32(hdr->magic_number);
    hdr->major_ver      = ls_bswap16(hdr->major_ver);
    hdr->minor_ver      = ls_bswap16(hdr->minor_ver);
    hdr->reserved1      = ls_bswap32(hdr->reserved1);
    hdr->reserved2      = ls_bswap32(hdr->reserved2);
    hdr->snaplen        = ls_bswap32(hdr->snaplen);
    hdr->linktype       = ls_bswap32(hdr->linktype);
}

FILE *ls_pcap_file_create(const char *filename) {
    if (filename == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return fopen(filename, "wb");
}

FILE *ls_pcap_file_open(const char *filename) {
    if (filename == NULL) {
        errno = EINVAL;
        return NULL;
    }
    return fopen(filename, "rb");
}

int ls_pcap_file_read_header(FILE *fp, ls_pcap_hdr_t *hdr) {
    int status = 0;
    if (fp == NULL || hdr == NULL) {
        return -2;  // fallback, conditions must be checked by the caller
    }

    if (fread(hdr, LS_PCAP_HEADER_SIZE, 1, fp) != 1) {
        return ferror(fp) ? -2 : -1;
    }

    switch (hdr->magic_number) {
        case LS_PCAP_MAGIC_USEC:
            break;
        case LS_PCAP_MAGIC_NSEC:
            break;
        case 0xD4C3B2A1u:   // swapped case for LS_PCAP_MAGIC_USEC
            bswap_pcap_hdr(hdr);
            status = 1;
            break;
        case 0x4D3CB2A1u:    // swapped case for LS_PCAP_MAGIC_NSEC
            bswap_pcap_hdr(hdr);
            status = 1;
            break;
        default:
            return -1;
    }

    if (hdr->major_ver != 2 || hdr->minor_ver != 4) {
        return -1;
    }

    if (hdr->snaplen == 0) {return -1;}

    return status;
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
        errno = EINVAL;
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

    if (fwrite(&hdr, LS_PCAP_HEADER_SIZE, 1, fp) != 1) {
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

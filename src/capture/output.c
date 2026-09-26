/**
 * @file output.c
 * @author Omar Merroun
 * @brief output implementation
 * @version 0.1
 * @date 2026-09-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdio.h>
#include <linux/if_ether.h>
#include <netinet/in.h>

#include "output.h"
#include "pcap.h"

int output_open(output_cfg_t *cfg)
{
    cfg->fp = NULL;

    if (cfg->pcap_path == NULL)
        return 0;

    cfg->fp = pcap_file_create(cfg->pcap_path);
    if (cfg->fp == NULL)
        return -1;

    if (pcap_file_write_header(cfg->fp, cfg->snaplen, cfg->linktype) != 0) {
        fclose(cfg->fp);
        cfg->fp = NULL;
        return -1;
    }
    return 0;
}

/**
 * \brief Prints packet details to stdout (best effort).
 * TODO: verbosity level
 */
static void output_stdout(const output_cfg_t *cfg, const pcap_rec_hdr_t *hdr,
                          const uint8_t *data)
{
    printf("%u.%09u: %u/%u\n", hdr->ts_sec, hdr->ts_nsec,
           hdr->captured_len, hdr->original_len);

    if (cfg->linktype != PCAP_LINKTYPE_ETHERNET ||
        hdr->captured_len < sizeof(struct ethhdr)) {
        putchar('\n');
        return;
    }

    const struct ethhdr *eth = (const struct ethhdr *)data;
    const uint8_t *s = eth->h_source, *d = eth->h_dest;

    printf("Source address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           s[0], s[1], s[2], s[3], s[4], s[5]);
    printf("Destination address: %02x:%02x:%02x:%02x:%02x:%02x\n",
           d[0], d[1], d[2], d[3], d[4], d[5]);
    printf("Packet type: 0x%04x\n\n", ntohs(eth->h_proto));
}

int output_write(const output_cfg_t *cfg, const pcap_rec_hdr_t *hdr,
                 const uint8_t *data)
{
    if (cfg->display)
        output_stdout(cfg, hdr, data);

    if (cfg->fp && pcap_file_write_packet(cfg->fp, data, hdr) != 0)
        return -1;

    return 0;
}

void output_close(output_cfg_t *cfg)
{
    if (cfg->fp) {
        fclose(cfg->fp);
        cfg->fp = NULL;
    }

    if (cfg->display && (fflush(stdout) == EOF || ferror(stdout)))
        fprintf(stderr, "error writing to stdout\n");
}

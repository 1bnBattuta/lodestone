/**
 * @file pcap.h
 * @author Omar Merroun
 * @brief pcap writer API
 * @version 0.1
 * @date 2026-09-25
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef LS_PCAP_H
#define LS_PCAP_H

#include <stdint.h>
#include <stdio.h>

#define PCAP_MAGIC_NSEC 0xA1B23C4Du   /* nanosecond resolution timestamps */
#define PCAP_LINKTYPE_ETHERNET 1u

/** pcap file header (24 bytes, host byte order) */
typedef struct {
    uint32_t magic_number;  /**< PCAP_MAGIC_NSEC */
    uint16_t major_ver;     /**< 2 */
    uint16_t minor_ver;     /**< 4 */
    uint32_t reserved1;     /**< 0 */
    uint32_t reserved2;     /**< 0 */
    uint32_t snaplen;       /**< max bytes captured per packet */
    uint32_t linktype;      /**< low 16 bits: link type; high bits: FCS info */
} pcap_hdr_t;

typedef struct {
    uint32_t ts_sec;        /**< timestamp, seconds */
    uint32_t ts_nsec;       /**< timestamp, nanoseconds (see magic number)*/
    uint32_t captured_len;  /**< bytes saved in the file*/
    uint32_t original_len;  /**< original length on the wire */
} pcap_rec_hdr_t;

_Static_assert(sizeof(pcap_hdr_t) == 24, "pcap file header must be 24 bytes");
_Static_assert(sizeof(pcap_rec_hdr_t) == 16, "pcap record header must be 16 bytes");

/**
 * \brief Creates a pcap file, truncating it if it exists.
 * \param filename path of the file to create
 * \return file pointer, or NULL on error
 */
FILE *pcap_file_create(const char *filename);

/**
 * \brief Writes the pcap file header.
 * \param fp file pointer
 * \param snaplen max bytes captured per packet
 * \param linktype link type (such as PCAP_LINKTYPE_ETHERNET)
 * \return 0 on success, -1 else
 */
int pcap_file_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype);

/**
 * \brief Writes one packet (record header + data) to a pcap file.
 *
 * \param fp      file pointer
 * \param data    pointer to the first byte of the packet (link-layer header)
 * \param caplen  number of bytes available in data (saved to the file)
 * \param origlen original length of the packet on the wire
 * \param ts_sec  timestamp, seconds
 * \param ts_nsec timestamp, nanoseconds
 * \return 0 on success, -1 otherwise
 */
int pcap_file_write_packet(FILE *fp, const uint8_t *data, const pcap_rec_hdr_t *hdr);

#endif /* LS_PCAP_H */

/**
 * \file ls_pcap.h
 * \author Omar Merroun
 * \brief A stateless pcap format encoder/decoder API
 * \version 0.1
 * \date 2026-09-25
 * 
 * \copyright Copyright (c) 2026
 * 
 */

#ifndef LS_PCAP_H
#define LS_PCAP_H

#include <stdint.h>
#include <stdio.h>

#define LS_PCAP_HEADER_SIZE 24u
#define LS_PCAP_REC_HEADER_SIZE 16u
#define LS_PCAP_NSEC_PER_SEC 1000000000u
#define LS_PCAP_USEC_PER_SEC 1000000u
#define LS_PCAP_MAGIC_USEC 0xA1B2C3D4u   /* microsecond resolution timestamps */
#define LS_PCAP_MAGIC_NSEC 0xA1B23C4Du   /* nanosecond resolution timestamps */
#define LS_PCAP_MAGIC_USEC_SWAPPED 0xD4C3B2A1u
#define LS_PCAP_MAGIC_NSEC_SWAPPED 0x4D3CB2A1u
#define LS_PCAP_LINKTYPE_ETHERNET 1u
#define LS_PCAP_DEFAULT_SNAPLEN 262144u

/** pcap file header (24 bytes, host byte order) */
typedef struct {
    uint32_t magic_number;  /**< LS_PCAP_MAGIC_NSEC or LS_PCAP_MAGIC_USEC */
    uint16_t major_ver;     /**< 2 */
    uint16_t minor_ver;     /**< 4 */
    uint32_t reserved1;     /**< 0 */
    uint32_t reserved2;     /**< 0 */
    uint32_t snaplen;       /**< max bytes captured per packet */
    uint32_t linktype;      /**< low 16 bits: link type; high bits: FCS info */
} ls_pcap_hdr_t;

/**
 * pcap record header (16 bytes, host byte order), followed in the file
 * by captured_len bytes of packet data.
 */
typedef struct {
    uint32_t ts_sec;        /**< timestamp, seconds */
    uint32_t ts_nsec;       /**< nanoseconds. Always ns in memory; the reader
                                converts µs files, the writer writes ns */
    uint32_t captured_len;  /**< bytes saved in the file*/
    uint32_t original_len;  /**< original length on the wire */
} ls_pcap_rec_hdr_t;

_Static_assert(sizeof(ls_pcap_hdr_t) == 24, "pcap file header must be 24 bytes");
_Static_assert(sizeof(ls_pcap_rec_hdr_t) == 16, "pcap record header must be 16 bytes");

/**
 * \brief Creates a pcap file for writing, truncating it if it exists.
 *
 * Does not write the file header: call ls_pcap_file_write_header() next
 * The caller must close the file with fclose() and must check its return
 * value as buffered write errors are reported there.
 *
 * \param filename path of the file to create
 * \return file pointer, or NULL on error (errno set by fopen())
 */
FILE *ls_pcap_file_create(const char *filename);

/**
 * \brief Opens an existing pcap file if exists.
 *
 * Does not check the file header: call ls_pcap_file_read_header() next
 * The caller must close the file with fclose().
 *
 * \param filename path of the file to open
 * \return file pointer, or NULL on error (errno set by fopen())
 */
FILE *ls_pcap_file_open(const char *filename);

/**
 * \brief Validates the pcap file header and populates pcap_hdr
 * 
 * The returned header is guaranteed to be in host endianness except the
 * magic number field which is left for the record header reader to detect
 * whether record fields are in correct endianness.
 * the header is unspecified on failure (must be discarded).
 * \param fp 
 * \param hdr empty pcap_hdr to be filled
 * \retval 0 parsed successfully, host endianness
 * \retval -1 invalid header
 * \retval -2 I/O error or invalid argument (errno set)
 */
int ls_pcap_file_read_header(FILE *fp, ls_pcap_hdr_t *hdr);

/**
 * \brief Checks a record header against the pcap header values.
 *
 * Checks that captured_len <= snaplen and captured_len <= original_len
 * and ts_nsec < LS_PCAP_NSEC_PER_SEC.
 *
 * \pre rec_hdr is non-NULL, in host byte order, with ts_nsec in
 *      nanoseconds. Readers must byte-swap and convert µs timestamps
 *      before calling this, an unconverted µs value cannot be detected.
 *
 * \param rec_hdr record header to check
 * \param snaplen snaplen from the pcap file header
 * \return 0 if valid, -1 otherwise
 */
int ls_pcap_rec_hdr_check(const ls_pcap_rec_hdr_t *rec_hdr, uint32_t snaplen);

/**
 * \brief Writes the pcap file header.
 *
 * Writes nanosecond magic (LS_PCAP_MAGIC_NSEC), version 2.4,
 * host byte order. 
 * Must be called exactly once, before any packet.
 *
 * \param fp       file pointer
 * \param snaplen  max bytes captured per packet
 * \param linktype link type (such as LS_PCAP_LINKTYPE_ETHERNET),
 *                 FCS bits included if any
 * \return 0 on success, -1 on error (errno set)
 */
int ls_pcap_file_write_header(FILE *fp, uint32_t snaplen, uint32_t linktype);

/**
 * \brief Writes one packet (record header + data) to a pcap file.
 *
 * The record header is written as-is, without validation: callers
 * should check it with ls_pcap_rec_hdr_check() first. On failure, the
 * file may end with a partial record.
 *
 * \param fp      file pointer
 * \param data    first byte of the packet (link-layer header); may be
 *                NULL only when rec_hdr->captured_len is 0
 * \param rec_hdr record header with ts_nsec in nanoseconds
 * \return 0 on success, -1 on error (errno set)
 */
int ls_pcap_file_write_packet(FILE *fp, const uint8_t *data, const ls_pcap_rec_hdr_t *rec_hdr);

#endif /* LS_PCAP_H */

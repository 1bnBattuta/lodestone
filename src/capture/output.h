/**
 * @file output.h
 * @author Omar Merroun
 * @brief output API
 * @version 0.1
 * @date 2026-09-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#ifndef LS_OUTPUT_H
#define LS_OUTPUT_H

#include <stdint.h>
#include <stdio.h>

#include "pcap.h"

/**
 * \brief Configuration struct for the output module
 */
typedef struct {
    const char *pcap_path;  /**< NULL if no file */
    int display;            /**< print packets to stdout */
    uint32_t snaplen;
    uint32_t linktype;
    FILE *fp;                /**< File pointer */
} output_cfg_t;


/**
 * \brief creates output file
 * \return 0 on success, -1 otherwise
 */
int output_open(output_cfg_t *cfg);

/**
 * \brief Write a record to the output file or stdout
 * \return int 0 on success, -1 otherwise
 */
int output_write(const output_cfg_t *cfg, const pcap_rec_hdr_t *hdr, const uint8_t *data);

/**
 * \brief flushes and closes the output file
 */
void output_close(output_cfg_t *cfg);


#endif // LS_OUTPUT_H

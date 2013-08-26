/******************************************************************************
 * main.c
 *
 * mc_barf - Intel Microcode Dumper
 *
 * Copyright (C) 2013, Matt Davis (enferex)
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or (at
 * your option) any later version.
 *             
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more
 * details.
 *                             
 * You should have received a copy of the GNU
 * General Public License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


#define FLAG_SHOW_HEADERS 1 /* Print headers (normal and optional extended) */
#define FLAG_SHOW_DATA    2 /* Dump update data (encrypted)                 */


/* Normal update header */
typedef struct _mc_hdr_d
{
    uint32_t ver;
    int32_t  rev;
    uint32_t date;
    uint32_t sig;
    uint32_t csum;
    uint32_t loader_rev;
    uint32_t cpu_flags;
    uint32_t data_size;
    uint32_t total_size;
    char     reserved[12];
} mc_hdr_t;


/* Extened update header */
typedef struct _mc_ext_hdr_d
{
    uint32_t ext_sig_count;
    uint32_t ext_csum;
    char     reserved[12];
} mc_ext_hdr_t;


/* A single update (does not contain any bits for actual data, just header
 * information.  The parser will skip over the data or print it.
 */
typedef struct _mc_d
{
    mc_hdr_t hdr;
    mc_ext_hdr_t ext_hdr;
} mc_t;


static void usage(const char *execname)
{
    printf("Usage: %s <microcode.bin>\n", execname);
    exit(EXIT_SUCCESS);
}


static void print_header(const mc_hdr_t *hdr, const int hdr_num)
{
    printf("-- Header (%d) --\n"
           "Header Version : 0x%x\n"
           "Update Revision: %d\n"
           "Date           : %x/%x/%x\n"
           "CPU Signature  : 0x%x\n"
           "Checksum       : 0x%x\n"
           "Loader Revision: 0x%x\n"
           "CPU Flags      : 0x%x\n"
           "Data Size      : 0x%x\n"
           "Total Size     : 0x%x\n"
           "------------\n",
           hdr_num,
           hdr->ver,
           hdr->rev,
           hdr->date>>24, hdr->date>>16&0x000000FF, hdr->date&0x0000FFFF,
           hdr->sig,
           hdr->csum,
           hdr->loader_rev,
           hdr->cpu_flags,
           hdr->data_size,
           hdr->total_size);
}


static void print_ext_header(const mc_ext_hdr_t *hdr)
{
    printf("-- Extender Header --\n"
           "Extended Header Count                : %d\n"
           "Extended CPU Signature Table Checksum: 0x%x\n",
           hdr->ext_sig_count,
           hdr->ext_csum);
}


/* Read the data and output relebavant bits
 * The value/address stored at 'data_ptr' will be updated to point to the
 * beginning of the next microcode entry header.
 */
static void parse(void **data_ptr, const char flags)
{
    uint32_t i;
    unsigned char byte;
    mc_t mc;
    static int header_count;
    const unsigned char *data = *(unsigned char **)data_ptr;

    /* Get header */
    memcpy(&mc.hdr, data, sizeof(mc_hdr_t));
    data += sizeof(mc_hdr_t);
    if (mc.hdr.data_size == 0x0)
      mc.hdr.data_size = 2000;
    print_header(&mc.hdr, ++header_count);

    if (flags & FLAG_SHOW_DATA)
    {
        printf("-- Data --\n");
        for (i=0; i<mc.hdr.data_size; ++i)
        {
            byte = *data;
            ++data;
            printf("0x%02x ", byte);
            if ((i+1)%16 == 0)
              printf("\n");
            else if ((i+1)%8 == 0)
              printf("  ");
        }
    }
    else
      data += mc.hdr.data_size;

    if (!mc.hdr.total_size ||
        (mc.hdr.total_size - mc.hdr.data_size + sizeof(mc_hdr_t)) > 0)
    {
        *data_ptr = (void *)data;
        return;
    }
    
    /* Get extended header */
    memcpy(&mc.ext_hdr, data, sizeof(mc_ext_hdr_t));
    print_ext_header(&mc.ext_hdr);
    data += sizeof(mc_ext_hdr_t);

    /* Read extended data  */
    for (i=0; i<mc.ext_hdr.ext_sig_count; i++)
    {
        printf("[%d] Processor Signature: 0x%x\n", i, *(int32_t *)data);
        data += sizeof(int32_t);
    }
    for (i=0; i<mc.ext_hdr.ext_sig_count; i++)
    {
        printf("[%d] Processor Flags    : 0x%x\n", i, *(int32_t *)data);
        data += sizeof(int32_t);
    }
    for (i=0; i<mc.ext_hdr.ext_sig_count; i++)
    {
        printf("[%d] Checksum           : 0x%x\n", i, *(int32_t *)data);
        data += sizeof(int32_t);
    }
    
    *data_ptr = (void *)data;
}

/* Args:
 * -d (dump data)
 */
int main(int argc, char **argv)
{
    int i, fd;
    void *data, *st_data;
    const char *fname = NULL;
    struct stat st;
    char flags = FLAG_SHOW_HEADERS;

    for (i=1; i<argc; ++i)
    {
        if (strncmp("-d", argv[i], 2) == 0)
          flags |= FLAG_SHOW_DATA;
        else
          fname = argv[0];
    }
    
    if (!fname)    
      usage(argv[0]);

    /* Open the file so it can be mmap'd
     * TODO: Verify that this file is the proper format
     */
    if (!(fd = open(argv[1], O_RDONLY)))
    {
        printf("Could not open: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    /* Figure out how large the microcode file is */
    fstat(fd, &st);
    data = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    st_data = data;
    close(fd);

    /* For each header in the microcode file... */
    while ((uintptr_t)data < ((uintptr_t)st_data + st.st_size))
      parse(&data, flags);
    
    munmap(st_data, st.st_size);
    return 0;
}

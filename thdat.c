/*
 * Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain this list
 *    of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce this
 *    list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "datpacker.h"
#include "util.h"
#include "thdat.h"
#include "thrle.h"
#include "thlzss.h"

archive_t*
archive_open(FILE* fd, uint32_t version, uint32_t offset, unsigned int count)
{
    archive_t* archive;

    /* Reserve some space for the header. */
    /* TODO: Use a seek wrapper. */
    if (fseek(fd, offset, SEEK_SET) == -1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't seek: %s", strerror(errno));
        return NULL;
    }

    archive = malloc(sizeof(archive_t));
    archive->entries = malloc(count * sizeof(entry_t));

    archive->version = version;
    archive->fd = fd;
    archive->offset = offset;
    archive->count = 0;

    return archive;
}

unsigned char*
thdat_read_file(entry_t* entry, FILE* fd)
{
    unsigned char* data = malloc(entry->size);

    if (fread(data, entry->size, 1, fd) != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't read: %s", strerror(errno));
        free(data);
        return NULL;
    }

    return data;
}

unsigned char*
thdat_read_file_lzss(entry_t* entry, FILE* fd)
{
    return th_lz_fd(fd, &entry->zsize);
}

unsigned char*
thdat_rle(entry_t* entry, unsigned char* data)
{
    unsigned char* zdata = th_rle(data, entry->size, &entry->zsize);

    if (entry->zsize >= entry->size) {
        entry->zsize = entry->size;
        free(zdata);
        zdata = data;
    } else {
        free(data);
    }

    return zdata;
}

int
thdat_write_entry(archive_t* archive, entry_t* entry, unsigned char* data)
{
    size_t ret;

#pragma omp critical
{
    ret = fwrite(data, entry->zsize, 1, archive->fd);
    entry->offset = archive->offset;
    archive->offset += entry->zsize;
}

    free(data);

    if (ret != 1) {
        snprintf(library_error, LIBRARY_ERROR_SIZE, "couldn't write: %s", strerror(errno));
        free(data);
        return -1;
    } else
        return 0;
}

static int
entry_compar(const void* a, const void* b)
{
    entry_t* ea = (entry_t*)a;
    entry_t* eb = (entry_t*)b;
    if (ea->offset < eb->offset)
        return -1;
    else if (ea->offset > eb->offset)
        return 1;
    else
        return 0;
}

void
thdat_sort(archive_t* archive)
{
    qsort(archive->entries, archive->count, sizeof(entry_t), entry_compar);
}
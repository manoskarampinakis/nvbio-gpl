/*
 * nvbio
 * Copyright (C) 2011-2014, NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <nvbio/io/sequence/sequence.h>
#include <stdio.h>
#include <stdlib.h>

namespace nvbio {
namespace io {

///@addtogroup IO
///@{

///@addtogroup SequenceIO
///@{

///@addtogroup SequenceIODetail
///@{

/// abstract file-backed SequenceDataStream
///
struct SequenceDataFile : public SequenceDataStream
{
    static const uint32 LONG_READ = 32*1024;

    /// enum describing various possible file states
    ///
    typedef enum {
        // not yet opened (initial state)
        FILE_NOT_READY,
        // unable to open file (e.g., file not found)
        FILE_OPEN_FAILED,
        // ready to read
        FILE_OK,
        // reached EOF
        FILE_EOF,
        // file stream error (e.g., gzip CRC failure)
        FILE_STREAM_ERROR,
        // file format parsing error (e.g., bad FASTQ file)
        FILE_PARSE_ERROR
    } FileState;

protected:
    SequenceDataFile(
        const uint32            max_reads,
        const uint32            truncate_read_len,
        const SequenceEncoding  flags)
      : SequenceDataStream(),
        m_max_reads(max_reads),
        m_truncate_read_len( truncate_read_len ),
        m_flags(flags),
        m_loaded(0),
        m_file_state(FILE_NOT_READY)
    {};

public:
    /// virtual destructor
    ///
    virtual ~SequenceDataFile() {}

    /// grab the next batch of reads into a host memory buffer
    ///
    virtual int next(struct SequenceDataEncoder* encoder, const uint32 batch_size, const uint32 batch_bps);

    /// returns true if the stream is ready to read from
    ///
    virtual bool is_ok(void)
    {
        return m_file_state == FILE_OK;
    };

protected:
    virtual int nextChunk(struct SequenceDataEncoder* encoder, uint32 max_reads, uint32 max_bps) = 0;

    uint32                  m_max_reads;
    uint32                  m_truncate_read_len; ///< maximum length of a read; longer reads are truncated to this size
    SequenceEncoding        m_flags;
    uint32                  m_loaded;

    // current file state
    FileState               m_file_state;
};

///@} // SequenceIODetail
///@} // SequenceIO
///@} // IO

} // namespace io
} // namespace nvbio

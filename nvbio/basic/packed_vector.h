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

#include <nvbio/basic/packedstream.h>
#include <nvbio/basic/numbers.h>
#include <nvbio/basic/vector.h>


namespace nvbio {

///@addtogroup Basic
///@{

///@addtogroup PackedStreams
///@{

/// Bit-Packed Vector
///
template <typename SystemTag, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T = false, typename IndexType = uint32>
struct PackedVector
{
    static const uint32 SYMBOL_SIZE = SYMBOL_SIZE_T;
    static const uint32 BIG_ENDIAN  = BIG_ENDIAN_T;

    static const uint32 SYMBOLS_PER_WORD = 32 / SYMBOL_SIZE;

    typedef SystemTag   system_tag;
    typedef IndexType   index_type;

    typedef typename nvbio::vector<system_tag,uint32>::pointer                        pointer;
    typedef typename nvbio::vector<system_tag,uint32>::const_pointer            const_pointer;

    typedef PackedStream<      pointer,uint8,SYMBOL_SIZE,BIG_ENDIAN,IndexType>        stream_type;
    typedef PackedStream<const_pointer,uint8,SYMBOL_SIZE,BIG_ENDIAN,IndexType>  const_stream_type;
    typedef typename stream_type::iterator                                            iterator;
    typedef typename const_stream_type::iterator                                const_iterator;

    typedef uint8                                                                     value_type;
    typedef PackedStreamRef<stream_type>                                              reference;
    typedef PackedStreamRef<const_stream_type>                                  const_reference;

    typedef stream_type                                                               plain_view_type;
    typedef const_stream_type                                                   const_plain_view_type;

    /// constructor
    ///
    PackedVector(const index_type size = 0);

    /// resize
    ///
    void resize(const index_type size);

    /// size
    ///
    void size() const { return m_size; }

    /// length
    ///
    void length() const { return m_size; }

    /// return the begin iterator
    ///
    iterator begin();

    /// return the end iterator
    ///
    iterator end();

    /// return the begin iterator
    ///
    const_iterator begin() const;

    /// return the end iterator
    ///
    const_iterator end() const;

    /// push back a symbol
    ///
    void push_back(const uint8 s);

    nvbio::vector<system_tag,uint32> m_storage;
    index_type                       m_size;
};


/// return a plain view of a PackedVector object
///
template <typename SystemTag, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
inline
typename PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>::plain_view_type
plain_view(PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>& vec)
{
    typedef typename PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>::plain_view_type stream;
    return stream( &vec.m_storage.front() );
}

/// return a plain view of a const PackedVector object
///
template <typename SystemTag, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
inline
typename PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>::const_plain_view_type
plain_view(const PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>& vec)
{
    typedef typename PackedVector<SystemTag,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType>::const_plain_view_type stream;
    return stream( &vec.m_storage.front() );
}

///@} PackedStreams
///@} Basic

} // namespace nvbio

#include <nvbio/basic/packed_vector_inl.h>
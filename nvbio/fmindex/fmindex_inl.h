/*
 * nvbio
 * Copyright (C) 2012-2014, NVIDIA Corporation
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

namespace nvbio {

// return the number of occurrences of c in the range [0,k] of the given FM-index.
//
// \param fmi      FM-index
// \param k        range search delimiter
// \param c        query character
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::index_type rank(
    const fm_index<TRankDictionary,TSuffixArray>&                   fmi,
    typename fm_index<TRankDictionary,TSuffixArray>::index_type     k,
    uint8                                                           c)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;

    if (k == index_type(-1))
        return 0;
    if (k == fmi.length())
        return fmi.count( c );

    if (k >= fmi.primary()) // because $ is not in bwt
        --k;

    return rank( fmi.rank_dict(), k, c );
}

// return the number of occurrences of c in the ranges [0,l] and [0,r] of the
// given FM-index.
//
// \param fmi      FM-index
// \param range    range query [l,r]
// \param c        query character
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type rank(
    const fm_index<TRankDictionary,TSuffixArray>&                   fmi,
    typename fm_index<TRankDictionary,TSuffixArray>::range_type     range,
    uint8                                                           c)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;

    if (range.x == range.y)
    {
        const index_type r = rank( fmi, range.x, c );
        return make_vector( r, r );
    }
    else if (range.x == index_type(-1))
    {
        const index_type r = rank( fmi, range.y, c );
        return make_vector( index_type(0), r );
    }
    if (range.y == fmi.length())
    {
        return make_vector(
            rank( fmi, range.x, c ),
            fmi.count( c ) );
    }

    if (range.x >= fmi.primary()) --range.x; // because $ is not in bwt
    if (range.y >= fmi.primary()) --range.y; // because $ is not in bwt

    return rank( fmi.rank_dict(), range, c );
}

// return the number of occurrences of all characters in the range [0,k] of the
// given FM-index.
//
// \param fmi      FM-index
// \param k        range search delimiter
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::vec4_type rank4(
    const fm_index<TRankDictionary,TSuffixArray>&                   fmi,
    typename fm_index<TRankDictionary,TSuffixArray>::index_type     k)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;

    const index_type zero = index_type(0);

    if (k == index_type(-1))
        return make_vector( zero, zero, zero, zero );
    else if (k == fmi.length())
    {
        return make_vector(
            fmi.count(0),
            fmi.count(1),
            fmi.count(2),
            fmi.count(3) );
    } 

    if (k >= fmi.primary()) // because $ is not in bwt
        --k;

    return rank4( fmi.rank_dict(), k );
}

// return the number of occurrences of all characters in the range [0,k] of the
// given FM-index.
//
// \param fmi      FM-index
// \param range    range query [l,r]
// \param outl     first output
// \param outh     second output
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void rank4(
    const fm_index<TRankDictionary,TSuffixArray>&                   fmi,
    typename fm_index<TRankDictionary,TSuffixArray>::range_type     range,
    typename fm_index<TRankDictionary,TSuffixArray>::vec4_type*     outl,
    typename fm_index<TRankDictionary,TSuffixArray>::vec4_type*     outh)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;

    const index_type zero = index_type(0);

    if (range.x == range.y)
    {
        *outl = rank4( fmi, range.x );
        *outh = *outl;
        return;
    }
    else if (range.x == index_type(-1))
    {
        *outl = make_vector( zero, zero, zero, zero );
        *outh = rank4( fmi, range.y );
        return;
    }
    else if (range.y == fmi.length())
    {
        *outl = rank4( fmi, range.x );
        *outh = make_uint4(
            fmi.count(0),
            fmi.count(1),
            fmi.count(2),
            fmi.count(3) );
        return;
    }

    if (range.x >= fmi.primary()) --range.x; // because $ is not in bwt
    if (range.y >= fmi.primary()) --range.y; // because $ is not in bwt

    rank4( fmi.rank_dict(), range, outl, outh );
}

// return the range of occurrences of a pattern in the given FM-index.
//
// \param fmi          FM-index
// \param pattern      query string
// \param pattern_len  query string length
//
template <
    typename TRankDictionary,
    typename TSuffixArray,
    typename Iterator>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type match(
    const fm_index<TRankDictionary,TSuffixArray>&   fmi,
    const Iterator                                  pattern,
    const uint32                                    pattern_len)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    // backward search
    const range_type range = make_vector( index_type(0), fmi.length() );

    return match( fmi, pattern, pattern_len, range );
}

// return the range of occurrences of a pattern in the given FM-index.
//
// \param fmi          FM-index
// \param pattern      query string
// \param pattern_len  query string length
// \param in_range     starting range
//
template <
    typename TRankDictionary,
    typename TSuffixArray,
    typename Iterator>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type  match(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const Iterator                                                      pattern,
    const uint32                                                        pattern_len,
    const typename fm_index<TRankDictionary,TSuffixArray>::range_type   in_range)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    // backward search
    range_type range = in_range;

    for (int32 i = pattern_len-1; i >= 0 && range.x <= range.y; --i)
    {
        const uint8 c = pattern[i];
        if (c > 3) // there is an N here. no match 
            return make_vector(index_type(1),index_type(0));

        const range_type c_rank = rank(
            fmi,
            make_vector( range.x-1, range.y ),
            c );

        range.x = fmi.L2(c) + c_rank.x + 1;
        range.y = fmi.L2(c) + c_rank.y;
    }
    return range;
}

// return the range of occurrences of a reversed pattern in the given FM-index.
//
// \param fmi          FM-index
// \param pattern      query string
// \param pattern_len  query string length
//
template <
    typename TRankDictionary,
    typename TSuffixArray,
    typename Iterator>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type match_reverse(
    const fm_index<TRankDictionary,TSuffixArray>&   fmi,
    const Iterator                                  pattern,
    const uint32                                    pattern_len)
{
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    // forward search
    range_type range = make_vector( index_type(0), fmi.length() );

    for (uint32 i = 0; i < pattern_len && range.x <= range.y; ++i)
    {
        const uint8 c = pattern[i];
        if (c > 3) // there is an N here. no match 
            return make_vector(index_type(1),index_type(0));

        const range_type c_rank = rank(
            fmi,
            make_vector( range.x-1, range.y ),
            c );

        range.x = fmi.L2(c) + c_rank.x + 1;
        range.y = fmi.L2(c) + c_rank.y;
    }
    return range;
}

// computes the inverse psi function at a given index, without using the reduced SA
//
// \param fmi          FM-index
// \param i            query index
// \return             base inverse psi function value and offset
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::index_type basic_inv_psi(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const typename fm_index<TRankDictionary,TSuffixArray>::index_type   i)
{
    typedef fm_index<TRankDictionary,TSuffixArray> FMIndexType;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
//    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    NVBIO_CUDA_ASSERT( i <= fmi.length() );
    typename FMIndexType::bwt_type bwt = fmi.bwt();

    if (i == fmi.primary())
        return 0;

    const index_type k = i < fmi.primary() ? i : i-1;

    const uint8 c = bwt[k];
    return fmi.L2(c) + 
        rank( fmi.rank_dict(), k, c );
}

// computes the inverse psi function at a given index
//
// \param fmi          FM-index
// \param i            query index
// \return             base inverse psi function value and offset
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type inv_psi(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const typename fm_index<TRankDictionary,TSuffixArray>::index_type   i)
{
    typedef fm_index<TRankDictionary,TSuffixArray> FMIndexType;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
//    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    NVBIO_CUDA_ASSERT( i <= fmi.length() );
    index_type j = i;
    index_type t = 0;
    index_type suffix;

    typename FMIndexType::suffix_array_type sa  = fmi.sa();
    typename FMIndexType::bwt_type          bwt = fmi.bwt();

    while (sa.fetch( j, suffix ) == false)
    {
        if (j != fmi.primary())
        {
            const uint8 c = j < fmi.primary() ? bwt[j] : bwt[j-1];
            j = fmi.L2(c) + rank( fmi, j, c );
            NVBIO_CUDA_ASSERT( j <= fmi.length() );
        }
        else
            j = 0;

        ++t;
    }
    return make_vector( j, t );
}

// given a suffix array index i, return its linear coordinate (or equivalently, return the
// linear coordinate of the suffix that prefixes the i-th row of the BWT matrix).
//
// \param fmi          FM-index
// \param i            query index
// \return             position of the suffix that prefixes the query index
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::index_type locate(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const typename fm_index<TRankDictionary,TSuffixArray>::index_type   i)
{
    typedef fm_index<TRankDictionary,TSuffixArray> FMIndexType;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
//    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    NVBIO_CUDA_ASSERT( i <= fmi.length() );
    index_type j = i;
    index_type t = 0;
    index_type suffix;

    typename FMIndexType::suffix_array_type sa  = fmi.sa();
    typename FMIndexType::bwt_type          bwt = fmi.bwt();

    while (sa.fetch( j, suffix ) == false)
    {
        if (j != fmi.primary())
        {
            const uint8 c = j < fmi.primary() ? bwt[j] : bwt[j-1];
            j = fmi.L2(c) + rank( fmi, j, c );
            NVBIO_CUDA_ASSERT( j <= fmi.length() );
        }
        else
            j = 0;

        ++t;
    }
    return (suffix + t) /*% (fmi.length()+1)*/;
}

// given a suffix array index i, return the position of the closest suffix in the sampled SA,
// and its relative offset
//
// \param fmi          FM-index
// \param i            query index
// \return             a pair formed by the position of the suffix that prefixes the query index
//                     in the sampled SA and its relative offset
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::range_type locate_ssa_iterator(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const typename fm_index<TRankDictionary,TSuffixArray>::index_type   i)
{
    typedef fm_index<TRankDictionary,TSuffixArray> FMIndexType;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
//    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    NVBIO_CUDA_ASSERT( i <= fmi.length() );
    index_type j = i;
    index_type t = 0;

    typename FMIndexType::suffix_array_type sa  = fmi.sa();
    typename FMIndexType::bwt_type          bwt = fmi.bwt();

    while (sa.has( j ) == false)
    {
        if (j != fmi.primary())
        {
            const uint8 c = j < fmi.primary() ? bwt[j] : bwt[j-1];
            j = fmi.L2(c) + rank( fmi, j, c );
            NVBIO_CUDA_ASSERT( j <= fmi.length() );
        }
        else
            j = 0;

        ++t;
    }
    return make_vector( j, t );
}

// given a sampled suffix array index i and an offset j, return the corresponding linear coordinate SSA[i]+j
//
// \param fmi          FM-index
// \param iter         iterator to the sampled SA
// \return             final linear coordinate
//
template <
    typename TRankDictionary,
    typename TSuffixArray>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename fm_index<TRankDictionary,TSuffixArray>::index_type lookup_ssa_iterator(
    const fm_index<TRankDictionary,TSuffixArray>&                       fmi,
    const typename fm_index<TRankDictionary,TSuffixArray>::range_type   it)
{
    typedef fm_index<TRankDictionary,TSuffixArray> FMIndexType;
    typedef typename fm_index<TRankDictionary,TSuffixArray>::index_type index_type;
//    typedef typename fm_index<TRankDictionary,TSuffixArray>::range_type range_type;

    typename FMIndexType::suffix_array_type sa = fmi.sa();
    index_type suffix; sa.fetch( it.x, suffix );
    return suffix + it.y;
}

#ifdef __CUDACC__
#if defined(MOD_NAMESPACE)
MOD_NAMESPACE_BEGIN
#endif
// indexing operator
//
NVBIO_FORCEINLINE NVBIO_DEVICE uint32 count_table_texture::operator[] (const uint32 i) const
{
#if USE_TEX
    return binary_cast<uint32>( tex1Dfetch( s_count_table_tex, i ) );
#else
    return 0;
#endif
}

// bind texture
NVBIO_FORCEINLINE NVBIO_HOST void count_table_texture::bind(const uint32* count_table)
{
#if USE_TEX
    cudaChannelFormatDesc channel_desc = cudaCreateChannelDesc<uint32>();
    s_count_table_tex.normalized = false;
    s_count_table_tex.filterMode = cudaFilterModePoint;
    cudaBindTexture( 0, &s_count_table_tex, count_table, &channel_desc, 256*sizeof(uint32) );
#endif
}

// unbind texture
NVBIO_FORCEINLINE NVBIO_HOST void count_table_texture::unbind()
{
#if USE_TEX
    cudaUnbindTexture( &s_count_table_tex );
#endif
}
#if defined(MOD_NAMESPACE)
MOD_NAMESPACE_END
#endif
#endif

} // namespace nvbio

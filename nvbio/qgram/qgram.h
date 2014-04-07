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

#include <nvbio/basic/types.h>
#include <nvbio/basic/numbers.h>
#include <nvbio/basic/algorithms.h>
#include <nvbio/basic/cuda/primitives.h>
#include <nvbio/basic/thrust_view.h>
#include <nvbio/basic/exceptions.h>
#include <nvbio/basic/iterator.h>
#include <nvbio/basic/vector.h>
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/for_each.h>
#include <thrust/binary_search.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/iterator/counting_iterator.h>

///\page qgram_page Q-Gram Module
///\htmlonly
/// <img src="nvidia_cubes.png" style="position:relative; bottom:-10px; border:0px;"/>
///\endhtmlonly
///\par
/// This module contains a series of functions to operate on q-grams as well as two q-gram index
/// data-structures together with very high throughput parallel construction algorithms.
///
///\section QGramIndicesSection Q-Gram Indices
///\par
/// Q-gram indices are data-structures providing fast searching of exact or approximate <i>q-grams</i> (or k-mers),
/// i.e. short strings of text containing <i>q</i> symbols.
/// This module provides two such data-structures:
///\par
/// - the \ref QGroupIndex "Q-Group Index", replicating the data-structure described in:\n
///   <i>Massively parallel read mapping on GPUs with PEANUT</i> \n
///   Johannes Koester and Sven Rahmann \n
///   http://arxiv.org/pdf/1403.1706v1.pdf \n
///   this data-structure requires O(A^q) bits of storage in the alphabet-size <i>A</i> and the q-gram length <i>q</i>,
///   and provides O(1) query time; \n \n
///
/// - the compact \ref QGramIndex "Q-Gram Index", which can be built over a string T, with memory consumption and query time proportional
///   to O(|T|) and O(log(unique(T))) respectively, where <i>unique(T)</i> is the number of unique q-grams in T.
///   This is achieved by keeping a plain sorted list of the unique q-grams in T, together with an index of their occurrences
///   in the original string T.
///   This data-structure offers up to 5x higher construction speed and a potentially unbounded improvement in memory consumption 
///   compared to the \ref QGroupIndex "Q-Group Index", though the query time is asymptotically higher.
///
///\par
/// Q-gram indices can be built both on strings and on string sets (in which case we call them <i>set-indices</i>).
/// The difference relies on the format of the coordinates associated to their q-grams: for strings, the coordinates
/// are simple linear indices, whereas for string-sets the coordinates are <i>(string-id,string-position)</i> index pairs.
///\par
/// The following code sample shows how to build a QGramIndex over a simple string:
///\code
/// // consider a DNA string in ASCII format
/// const char*  a_string = "ACGTACGTACGTACGTACGTACGTACGTACGT";
/// const uint32 string_len = strlen( a_string );
///
/// // convert the string to a 2-bit DNA alphabet
/// thrust::host_vector<uint8> h_string( string_len );
/// string_to_dna( a_string, string_len, h_string.begin() );
///
/// // copy the string to the device
/// thrust::device_vector<uint8> d_string( h_string );
///
/// // build a q-gram index on the device
/// QGramIndexDevice qgram_index;
///
/// qgram_index.build(
///     20u,                        // q-group size
///     2u,                         // the alphabet size, in bits
///     string_len,                 // the length of the string we want to index
///     d_string.begin() );         // the string we want to index
///\endcode
///\par
/// The next example shows how to do the same with a string-set:
///\code
/// // consider a DNA string in ASCII format
/// const char*  a_string = "ACGTACGTACGTACGTACGTACGTACGTTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTTACGTACGTACGTACGTACGTACGTACGTACGT";
/// const uint32 string_len = strlen( a_string );
///
/// // convert the string to a 2-bit DNA alphabet
/// thrust::host_vector<uint8> h_string( string_len );
/// string_to_dna( a_string, string_len, h_string.begin() );
///
/// // copy the string to the device
/// const uint32 n_strings = 2u;
/// thrust::device_vector<uint8>  d_string( h_string );
/// thrust::device_vector<uint32> d_string_offsets( n_strings+1 );
/// d_string_offsets[0] = 0;        // offset to the first string
/// d_string_offsets[1] = 20;       // offset to the second string
/// d_string_offsets[2] = 40;       // end of the last string
///
/// typedef ConcatenatedStringSet<uint8*,uint32*> string_set_type;
/// const string_set_type string_set(
///     n_strings,
///     nvbio::plain_view( d_string ),
///     nvbio::plain_view( d_string_offsets ) );
///
/// // build a q-gram index on the device
/// QGramSetIndexDevice qgram_index;
///
/// qgram_index.build(
///     20u,                        // q-group size
///     2u,                         // the alphabet size, in bits
///     string_set );               // the string-set we want to index
///\endcode
///
///\section QGramIndexQueriesSection Q-Gram Index Queries
///\par
/// Once a q-gram index is built, it would be interesting to perform some queries on it.
/// There's various ways to accomplish this, and here we'll show a couple.
/// The simplest query one can do is locating for a given q-gram the range of indexed q-grams
/// which match it exactly.
/// This can be done as follows:
///\code
/// // consider a sample string - we'll want to find all matching locations between all
/// // q-grams in this string and all q-grams in the index
/// const char* a_query_string = "CGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTACGTA"
/// const uint32 query_string_len = strlen( a_query_string );
///
/// // convert the string to a 2-bit DNA alphabet
/// thrust::host_vector<uint8> h_string( query_string_len );
/// string_to_dna( a_query_string, query_string_len, h_query_string.begin() );
///
/// // copy the string to the device
/// thrust::device_vector<uint8> d_query_string( h_query_string );
///
/// const uint32 q = 20u;
/// const uint32 n_query_qgrams = query_string_len - q
///
/// // build the set of query q-grams: this can be done using the string_qgram_functor
/// // to extract them; note that we need at least 20 x 2 = 40 bits per q-gram, hence we
/// // store them in a uint64 vector
/// thrust::device_vector<uint64> d_query_qgrams( n_query_qgrams );
/// thrust::transform(
///     thrust::make_counting_iterator<uint32>(0u),
///     thrust::make_counting_iterator<uint32>(0u) + n_query_qgrams,
///     d_query_qgrams.begin(),
///     string_qgram_functor<uint8*>(
///         q,                                          // q-gram length
///         2u,                                         // bits per symbol
///         query_string_len,                           // string length
///         nvbio::plain_view( d_query_string ) ) );    // string
///
/// // find the above q-gram
/// thrust::device_vector<uint32> d_ranges( query_string_len - 20 );
///
/// // use the plain-view of the q-gram index itself as a search functor, that we "apply"
/// // to our query q-grams to obtain the corresponding match ranges
/// thrust::transform(
///     d_query_qgrams.begin(),
///     d_query_qgrams.begin() + n_query_qgrams,
///     d_ranges.begin(),
///     nvbio::plain_view( qgram_index ) );
///\endcode
///
///\par
/// This of course was just a toy example; in reality, you'll want to this kind of operations with much
/// larger q-gram indices, and much larger batches of queries. Moreover, it's often beneficial to
/// sort the query q-grams upfront, as in:
///\code
/// // keep track of the original q-gram indices before sorting
/// thrust::device_vector<uint32> d_query_indices( n_query_qgrams );
/// thrust::copy(
///     thrust::make_counting_iterator<uint32>(0u),
///     thrust::make_counting_iterator<uint32>(0u) + n_query_qgrams,
///     d_query_indices.begin() );
///
/// // now sort the q-grams and their original indices
/// thrust::sort_by_key(
///     d_query_qgrams.begin(),
///     d_query_qgrams.begin() + n_query_qgrams,
///     d_query_indices.begin() );
///\endcode
///
///\section QGramFilterSection Q-Gram Filtering
///\par
/// The previous example was only showing how to get the <i>ranges</i> of matching q-grams inside an index: it didn't
/// show how to get the actual list of hits.
/// One way to go about it is to ask the q-gram index, which given an entry inside each non-empty range, can provide
/// its location.
/// This can be done using the qgram_locate_functor.
/// However, if one is interested in getting the complete list of hits things are more difficult, as the process
/// involves a variable rate data-expansion (as each range might expand to a variable number of hits).
///\par
/// The \ref QGramFilter provides a convenient way to do this:
///\code
/// // suppose we have the previous vectors d_query_qgrams and d_query_indices
/// ...
///
/// // find all hits using a q-gram filter
/// QGramFilterDevice qgram_filter;
/// qgram_filter.enact(
///     qgram_index,
///     n_query_qgrams,
///     d_query_qgrams.begin(),
///     d_query_indices.begin() );
///
/// const uint32  n_hits = qgram_filter.n_hits();
/// const uint2*  d_hits = qgram_filter.hits();
///   // d_hits is a device pointer to a list of <i>(qgram-pos,query-pos)</i> pairs,
///   // where <i>qgram-pos</i> is the index of the hit into the string used to build,
///   // qgram-index and <i>query-pos</i> corresponds to one of the input query q-gram
///   // indices.
///\endcode
///
/// \section TechnicalOverviewSection Technical Overview
///\par
/// A complete list of the classes and functions in this module is given in the \ref QGram documentation.
///

namespace nvbio {

///
///@defgroup QGram Q-Gram Module
/// This module contains a series of classes and functions to operate on q-grams and q-gram indices.
/// It implements the following data-structures:
///\par
/// - the \ref QGroupIndex "Q-Group Index"
/// - the \ref QGramIndex "Q-Gram Index"
/// - the \ref QGramFilter "Q-Gram Filter"
///
///@{
///

///
///@defgroup QGramIndex Q-Gram Index Module
/// This module contains a series of classes and functions to build a compact Q-Gram Index over
/// a string T, with memory consumption and query time proportional to O(|T|) and O(log(unique(T))) respectively,
/// where <i>unique(T)</i> is the number of unique q-grams in T.
/// This is achieved by keeping a plain sorted list of the unique q-grams in T, together with an index of their occurrences
/// in the original string T.
/// This data-structure offers up to 5x higher construction speed and a potentially unbounded improvement in memory consumption 
/// compared to the \ref QGroupIndex "Q-Group Index", though the query time is asymptotically higher.
///
///@{
///

/// The base q-gram index core class (see \ref QGramIndex)
///
template <typename QGramVectorType, typename IndexVectorType, typename CoordVectorType>
struct QGramIndexViewCore
{
    typedef QGramVectorType                                                 qgram_vector_type;
    typedef IndexVectorType                                                 index_vector_type;
    typedef CoordVectorType                                                 coord_vector_type;
    typedef typename std::iterator_traits<qgram_vector_type>::value_type    qgram_type;
    typedef typename std::iterator_traits<coord_vector_type>::value_type    coord_type;

    // unary functor typedefs
    typedef qgram_type  argument_type;
    typedef uint2       result_type;

    QGramIndexViewCore() {}

    QGramIndexViewCore(
        const uint32                _Q,
        const uint32                _symbol_size,
        const uint32                _n_unique_qgrams,
        const qgram_vector_type     _qgrams,
        const index_vector_type     _slots,
        const coord_vector_type     _index,
        const uint32                _QL,
        const uint32                _QLS,
        const index_vector_type     _lut) :
        Q               ( _Q ),
        symbol_size     ( _symbol_size ),
        n_unique_qgrams ( _n_unique_qgrams ),
        qgrams          ( _qgrams ),
        slots           ( _slots ),
        index           ( _index ),
        QL              ( _QL ),
        QLS             ( _QLS ),
        lut             ( _lut ) {}

    /// return the slots of P corresponding to the given qgram g
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint2 range(const qgram_type g) const
    {
        const uint2 lut_range = lut ?
            make_uint2( lut[ g >> QLS ], lut[ (g >> QLS) + 1 ] ) :
            make_uint2( 0u, n_unique_qgrams );

        // find the slot where our q-gram is stored
        const uint32 i = uint32( nvbio::lower_bound(
            g,
            qgrams + lut_range.x,
            (lut_range.y - lut_range.x) ) - qgrams );

        // check whether we found what we are looking for
        if ((i >= n_unique_qgrams) || (g != qgrams[i]))
            return make_uint2( 0u, 0u );

        // return the range
        return make_uint2( slots[i], slots[i+1] );
    }

    /// functor operator
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint2 operator() (const qgram_type g) const { return range( g ); }

    /// locate a given occurrence of a q-gram
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    coord_type locate(const uint32 i) const { return index[i]; }


    uint32              Q;                  ///< the q-gram size
    uint32              symbol_size;        ///< symbol size
    uint32              n_unique_qgrams;    ///< the number of unique q-grams in the original string
    qgram_vector_type   qgrams;             ///< the sorted list of unique q-grams
    index_vector_type   slots;              ///< slots[i] stores the first occurrence of q-grams[i] in index
    coord_vector_type   index;              ///< the list of occurrences of all (partially-sorted) q-grams in the original string

    uint32              QL;                 ///< the number of LUT symbols 
    uint32              QLS;                ///< the number of leading bits of a q-gram to lookup in the LUT
    index_vector_type   lut;                ///< a LUT used to accelerate q-gram searches
};

/// The base q-gram index core class (see \ref QGramIndex)
///
template <typename SystemTag, typename QGramType, typename IndexType, typename CoordType>
struct QGramIndexCore
{
    typedef SystemTag                                                       system_tag;

    typedef QGramType                                                       qgram_type;
    typedef IndexType                                                       index_type;
    typedef CoordType                                                       coord_type;
    typedef nvbio::vector<system_tag,qgram_type>                            qgram_vector_type;
    typedef nvbio::vector<system_tag,index_type>                            index_vector_type;
    typedef nvbio::vector<system_tag,coord_type>                            coord_vector_type;

    QGramIndexCore() {}

    /// return the amount of device memory used
    ///
    uint64 used_host_memory() const
    {
        return equal<system_tag,host_tag>() ?
               qgrams.size() * sizeof(qgram_type) +
               slots.size()  * sizeof(uint32)     +
               index.size()  * sizeof(coord_type) + 
               lut.size()    * sizeof(uint32) :
               0u;
    }

    /// return the amount of device memory used
    ///
    uint64 used_device_memory() const
    {
        return equal<system_tag,device_tag>() ?
               qgrams.size() * sizeof(qgram_type) +
               slots.size()  * sizeof(uint32)     +
               index.size()  * sizeof(coord_type) + 
               lut.size()    * sizeof(uint32) :
               0u;
    }

    uint32              Q;                  ///< the q-gram size
    uint32              symbol_size;        ///< symbol size
    uint32              n_unique_qgrams;    ///< the number of unique q-grams in the original string
    qgram_vector_type   qgrams;             ///< the sorted list of unique q-grams
    index_vector_type   slots;              ///< slots[i] stores the first occurrence of q-grams[i] in index
    coord_vector_type   index;              ///< the list of occurrences of all (partially-sorted) q-grams in the original string

    uint32              QL;                 ///< the number of LUT symbols 
    uint32              QLS;                ///< the number of leading bits of a q-gram to lookup in the LUT
    index_vector_type   lut;                ///< a LUT used to accelerate q-gram searches
};

typedef QGramIndexViewCore<uint64*,uint32*,uint32*> QGramIndexView;
typedef QGramIndexViewCore<uint64*,uint32*,uint2*>  QGramSetIndexView;

/// A host-side q-gram index (see \ref QGramIndex)
///
struct QGramIndexHost : public QGramIndexCore<host_tag,uint64,uint32,uint32>
{
    typedef host_tag                                system_tag;

    typedef QGramIndexCore<host_tag,uint64,uint32,uint32>   core_type;

    typedef core_type::qgram_vector_type            qgram_vector_type;
    typedef core_type::index_vector_type            index_vector_type;
    typedef core_type::coord_vector_type            coord_vector_type;
    typedef core_type::qgram_type                   qgram_type;
    typedef core_type::coord_type                   coord_type;
    typedef QGramIndexView                          view_type;

    /// copy operator
    ///
    template <typename SystemTag>
    QGramIndexHost& operator= (const QGramIndexCore<SystemTag,uint64,uint32,uint32>& src);
};

/// A device-side q-gram index for strings (see \ref QGramIndex)
///
struct QGramIndexDevice : public QGramIndexCore<device_tag,uint64,uint32,uint32>
{
    typedef device_tag                              system_tag;

    typedef QGramIndexCore<
        device_tag,
        uint64,
        uint32,
        uint32>                                     core_type;

    typedef core_type::qgram_vector_type            qgram_vector_type;
    typedef core_type::index_vector_type            index_vector_type;
    typedef core_type::coord_vector_type            coord_vector_type;
    typedef core_type::qgram_type                   qgram_type;
    typedef core_type::coord_type                   coord_type;
    typedef QGramIndexView                          view_type;

    /// build a q-gram index from a given string T; the amount of storage required
    /// is basically O( A^q + |T|*32 ) bits, where A is the alphabet size.
    ///
    /// \tparam string_type     the string iterator type
    ///
    /// \param q                the q parameter
    /// \param symbol_sz        the size of the symbols, in bits
    /// \param string_len       the size of the string
    /// \param string           the string iterator
    /// \param qlut             the number of symbols to include in the LUT (of size O( A^qlut ))
    ///                         used to accelerate q-gram searches
    ///
    template <typename string_type>
    void build(
        const uint32        q,
        const uint32        symbol_sz,
        const uint32        string_len,
        const string_type   string,
        const uint32        qlut = 0);

    /// copy operator
    ///
    template <typename SystemTag>
    QGramIndexDevice& operator= (const QGramIndexCore<SystemTag,uint64,uint32,uint32>& src);
};

/// A device-side q-gram index for string-sets (see \ref QGramIndex)
///
struct QGramSetIndexHost : public QGramIndexCore<host_tag,uint64,uint32,uint2>
{
    typedef host_tag                                system_tag;

    typedef QGramIndexCore<
        host_tag,
        uint64,
        uint32,
        uint2>                                      core_type;

    typedef core_type::qgram_vector_type            qgram_vector_type;
    typedef core_type::index_vector_type            index_vector_type;
    typedef core_type::coord_vector_type            coord_vector_type;
    typedef core_type::qgram_type                   qgram_type;
    typedef core_type::coord_type                   coord_type;
    typedef QGramIndexView                          view_type;

    /// copy operator
    ///
    template <typename SystemTag>
    QGramSetIndexHost& operator= (const QGramIndexCore<SystemTag,uint64,uint32,uint2>& src);
};

/// A device-side q-gram index for string-sets (see \ref QGramIndex)
///
struct QGramSetIndexDevice : public QGramIndexCore<device_tag,uint64,uint32,uint2>
{
    typedef device_tag                              system_tag;

    typedef QGramIndexCore<
        device_tag,
        uint64,
        uint32,
        uint2>                                      core_type;

    typedef core_type::qgram_vector_type            qgram_vector_type;
    typedef core_type::index_vector_type            index_vector_type;
    typedef core_type::coord_vector_type            coord_vector_type;
    typedef core_type::qgram_type                   qgram_type;
    typedef core_type::coord_type                   coord_type;
    typedef QGramIndexView                          view_type;

    /// build a q-gram index from a given string T; the amount of storage required
    /// is basically O( A^q + |T|*32 ) bits, where A is the alphabet size.
    ///
    /// \tparam string_type     the string iterator type
    ///
    /// \param q                the q parameter
    /// \param symbol_sz        the size of the symbols, in bits
    /// \param string_len       the size of the string
    /// \param string           the string iterator
    /// \param qlut             the number of symbols to include in the LUT (of size O( A^qlut ))
    ///                         used to accelerate q-gram searches
    ///
    template <typename string_set_type>
    void build(
        const uint32            q,
        const uint32            symbol_sz,
        const string_set_type   string,
        const uint32            qlut = 0);

    /// copy operator
    ///
    template <typename SystemTag>
    QGramSetIndexDevice& operator= (const QGramIndexCore<SystemTag,uint64,uint32,uint2>& src);
};

/// return the plain view of a QGramIndex
///
template <typename SystemTag, typename QT, typename IT, typename CT>
QGramIndexViewCore<QT*,IT*,CT*> plain_view(QGramIndexCore<SystemTag,QT,IT,CT>& qgram)
{
    return QGramIndexViewCore<QT*,IT*,CT*>(
        qgram.Q,
        qgram.symbol_size,
        qgram.n_unique_qgrams,
        nvbio::plain_view( qgram.qgrams ),
        nvbio::plain_view( qgram.slots ),
        nvbio::plain_view( qgram.index ),
        qgram.QL,
        qgram.QLS,
        nvbio::plain_view( qgram.lut ) );
}

/// return the plain view of a QGramIndex
///
template <typename SystemTag, typename QT, typename IT, typename CT>
QGramIndexViewCore<const QT*,const IT*,const CT*> plain_view(const QGramIndexCore<SystemTag,QT,IT,CT>& qgram)
{
    return QGramIndexViewCore<const QT*,const IT*,const CT*>(
        qgram.Q,
        qgram.symbol_size,
        qgram.n_unique_qgrams,
        nvbio::plain_view( qgram.qgrams ),
        nvbio::plain_view( qgram.slots ),
        nvbio::plain_view( qgram.index ),
        qgram.QL,
        qgram.QLS,
        nvbio::plain_view( qgram.lut ) );
}

/// a functor to locate the hit corresponding to a given range slot inside a q-gram index
///
template <typename qgram_index_type>
struct qgram_locate_functor
{
    typedef uint32  argument_type;
    typedef uint32  result_type;

    /// constructor
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    qgram_locate_functor(const qgram_index_type _index) : index(_index) {}

    /// unary functor operator : locate the hit corresponding to a given range slot
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    result_type operator() (const uint32 slot) const { return index.locate( slot ); }

    const qgram_index_type index;   ///< the q-gram index
};

///@} // end of the QGramIndex group

/// A utility functor to extract the i-th q-gram out of a string
///
/// \tparam string_type         the string iterator type
///
template <typename string_type>
struct string_qgram_functor
{
    typedef uint32  argument_type;
    typedef uint64  result_type;

    /// constructor
    ///
    /// \param _Q                the q-gram length
    /// \param _symbol_size      the size of the symbols, in bits
    /// \param _string_len       string length
    /// \param _string           string iterator
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    string_qgram_functor(const uint32 _Q, const uint32 _symbol_size, const uint32 _string_len, const string_type _string) :
        Q           ( _Q ),
        symbol_size ( _symbol_size ),
        symbol_mask ( (1u << _symbol_size) - 1u ),
        string_len  ( _string_len ),
        string      ( _string ) {}

    /// functor operator
    ///
    /// \param i        position along the string
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint64 operator() (const uint32 i) const
    {
        uint64 qgram = 0u;
        for (uint32 j = 0; j < Q; ++j)
            qgram |= uint64(i+j < string_len ? (string[i + j] & symbol_mask) : 0u) << (j*symbol_size);

        return qgram;
    }

    const uint32        Q;              ///< q-gram size
    const uint32        symbol_size;    ///< symbol size
    const uint32        symbol_mask;    ///< symbol size
    const uint32        string_len;     ///< string length
    const string_type   string;         ///< string iterator
};

/// A utility functor to extract the i-th q-gram out of a string-set
///
/// \tparam string_set_type         the string-set type
///
template <typename string_set_type>
struct string_set_qgram_functor
{
    typedef uint32  argument_type;
    typedef uint64  result_type;

    typedef typename string_set_type::string_type string_type;

    /// constructor
    ///
    /// \param _Q                the q-gram length
    /// \param _symbol_size      the size of the symbols, in bits
    /// \param _string_set       the string-set
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    string_set_qgram_functor(const uint32 _Q, const uint32 _symbol_size, const string_set_type _string_set) :
        Q           ( _Q ),
        symbol_size ( _symbol_size ),
        symbol_mask ( (1u << _symbol_size) - 1u ),
        string_set  ( _string_set ) {}

    /// functor operator
    ///
    /// \param id       string-set coordinate
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint64 operator() (const uint2 id) const
    {
        const uint32 string_id  = id.x;
        const uint32 string_pos = id.y;
        const string_type string = string_set[ string_id ];

        const uint32 string_len = string.length();

        uint64 qgram = 0u;
        for (uint32 j = 0; j < Q; ++j)
            qgram |= uint64(string_pos + j < string_len ? (string[string_pos + j] & symbol_mask) : 0u) << (j*symbol_size);

        return qgram;
    }

    const uint32            Q;              ///< q-gram size
    const uint32            symbol_size;    ///< symbol size
    const uint32            symbol_mask;    ///< symbol size
    const string_set_type   string_set;     ///< string-set
};

/// define a simple q-gram search functor
///
template <typename qgram_index_type, typename string_type>
struct string_qgram_search_functor
{
    typedef uint32          argument_type;
    typedef uint2           result_type;

    /// constructor
    ///
    string_qgram_search_functor(
        const qgram_index_type _qgram_index,
        const uint32           _string_len,
        const string_type      _string) :
        qgram_index ( _qgram_index ),
        string_len  ( _string_len ),
        string      ( _string ) {}

    /// functor operator
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint2 operator() (const uint32 i) const
    {
        const string_qgram_functor<string_type> qgram( qgram_index.Q, qgram_index.symbol_size, string_len, string );

        return qgram_index.range( qgram(i) );
    }

    const qgram_index_type  qgram_index;
    const uint32            string_len;
    const string_type       string;
};

///@} // end of the QGram group

} // namespace nvbio

#include <nvbio/qgram/qgram_inl.h>

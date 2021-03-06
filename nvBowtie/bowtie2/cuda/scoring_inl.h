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

#pragma once

#include <stdio.h>

namespace nvbio {
namespace bowtie2 {
namespace cuda {

inline
SmithWatermanScoringScheme<> load_scoring_scheme(const char* name, const AlignmentType type)
{
    FILE* file = fopen( name, "r" );
    if (file == NULL)
        return SmithWatermanScoringScheme<>();

    std::map<std::string,std::string> options;
    char key[1024];
    char value[1024];

    while (fscanf( file, "%s %s", key, value ) == 2)
        options[ key ] = std::string( value );

    fclose( file );

    return SmithWatermanScoringScheme<>( options, type );
}

template <
    typename MMCost,
    typename NCost>
SmithWatermanScoringScheme<MMCost,NCost> SmithWatermanScoringScheme<MMCost,NCost>::base1()
{
    std::map<std::string,std::string> options;
    options["match"]            = std::string("1");
    options["mm-penalty-min"]   = std::string("3");
    options["mm-penalty-max"]   = std::string("3");
    options["N-penalty-min"]    = std::string("3");
    options["N-penalty-max"]    = std::string("3");
    options["score-min-const"]  = std::string("37.0f");
    options["score-min-coeff"]  = std::string("0.3f");
    options["N-ceil-const"]     = std::string("2.0f");
    options["N-ceil-coeff"]     = std::string("0.1f");
    options["read-gap-const"]   = std::string("11");
    options["read-gap-coeff"]   = std::string("4");
    options["ref-gap-const"]    = std::string("11");
    options["ref-gap-coeff"]    = std::string("4");
    options["gap-free"]         = std::string("5");
    return SmithWatermanScoringScheme<MMCost,NCost>( options );
}

template <
    typename MMCost,
    typename NCost>
SmithWatermanScoringScheme<MMCost,NCost> SmithWatermanScoringScheme<MMCost,NCost>::local()
{
    std::map<std::string,std::string> options;
    options["match"]            = std::string("2");
    options["mm-penalty-min"]   = std::string("2");
    options["mm-penalty-max"]   = std::string("6");
    options["N-penalty-min"]    = std::string("1");
    options["N-penalty-max"]    = std::string("1");
    options["score-min-const"]  = std::string("0.0f");
    options["score-min-coeff"]  = std::string("10.0f");
    options["score-min-type"]   = std::string("log");
    options["N-ceil-const"]     = std::string("0.0f");
    options["N-ceil-coeff"]     = std::string("0.15f");
    options["read-gap-const"]   = std::string("5");
    options["read-gap-coeff"]   = std::string("3");
    options["ref-gap-const"]    = std::string("5");
    options["ref-gap-coeff"]    = std::string("3");
    options["gap-free"]         = std::string("5");
    return SmithWatermanScoringScheme<MMCost,NCost>( options, LocalAlignment );
}

// default constructor
//
template <
    typename MMCost,
    typename NCost>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
SmithWatermanScoringScheme<MMCost,NCost>::SmithWatermanScoringScheme() :
    m_score_min( LinearFunc, -0.6f, -0.6f ),
    m_n_ceil_const( 0.0f ),
    m_n_ceil_coeff( 0.15f ),
    m_read_gap_const( 5 ),
    m_read_gap_coeff( 3 ),
    m_ref_gap_const( 5 ),
    m_ref_gap_coeff( 3 ),
    m_gap_free( 5 ),
    m_match( 0, 0 ),
    m_mmp( 2, 6 ),
    m_np( 1, 1 ),
    m_monotone( true ),
    m_local( false )
{}

// constructor
//
// \param options          key/value string options
template <
    typename MMCost,
    typename NCost>
SmithWatermanScoringScheme<MMCost,NCost>::SmithWatermanScoringScheme(
    const std::map<std::string,std::string>& options,
    const AlignmentType                      type) :
    m_score_min( min_score_function(options) ),
    m_n_ceil_const( float_option( options, "N-ceil-const", 0.0f ) ),
    m_n_ceil_coeff( float_option( options, "N-ceil-coeff", 0.15f ) ),
    m_read_gap_const( int_option( options, "read-gap-const", 5 ) ),
    m_read_gap_coeff( int_option( options, "read-gap-coeff", 3 ) ),
    m_ref_gap_const( int_option( options, "ref-gap-const", 5 ) ),
    m_ref_gap_coeff( int_option( options, "ref-gap-coeff", 3 ) ),
    m_gap_free( int_option( options, "gap-free", 5 ) ),
    m_match( match_cost(options) ),
    m_mmp( mm_cost(options) ),
    m_np( n_cost(options) ),
    m_monotone( m_match(0) == 0 ),
    m_local( type == LocalAlignment ? true : false )
{}


template <
    typename MMCost,
    typename NCost>
ScoringFuncType SmithWatermanScoringScheme<MMCost,NCost>::func_type(const std::string& type)
{
    if (strcmp( type.c_str(), "log" ) == 0)
        return LogFunc;

    return LinearFunc;
}

template <
    typename MMCost,
    typename NCost>
MinScoreFunc SmithWatermanScoringScheme<MMCost,NCost>::min_score_function(const std::map<std::string,std::string>& options)
{
    return MinScoreFunc(
        func_type( string_option( options, "score-min-type", "linear" ) ),
        float_option( options, "score-min-const", -0.6f ), // 37.0f
        float_option( options, "score-min-coeff", -0.6f ) ); // 0.3f
}
template <
    typename MMCost,
    typename NCost>
typename SmithWatermanScoringScheme<MMCost,NCost>::MatchCost SmithWatermanScoringScheme<MMCost,NCost>::match_cost(const std::map<std::string,std::string>& options)
{
    const int match_cost = int_option( options, "match", 0 );
    return MatchCost( match_cost, match_cost );
}
template <
    typename MMCost,
    typename NCost>
MMCost SmithWatermanScoringScheme<MMCost,NCost>::mm_cost(const std::map<std::string,std::string>& options)
{
    const int mmp_min = int_option( options, "mm-penalty-min", 2 );
    const int mmp_max = int_option( options, "mm-penalty-max", 6 );
    return MMCost( mmp_min, mmp_max );
}
template <
    typename MMCost,
    typename NCost>
NCost SmithWatermanScoringScheme<MMCost,NCost>::n_cost(const std::map<std::string,std::string>& options)
{
    const int np_min = int_option( options, "N-penalty-min", 1 );
    const int np_max = int_option( options, "N-penalty-max", 1 );
    return NCost( np_min, np_max );
}

} // namespace cuda
} // namespace bowtie2
} // namespace nvbio

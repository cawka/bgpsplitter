
#ifndef _HELPER_H_
#define _HELPER_H_

#include <parsers/MRTCommonHeader.h>

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/program_options/variables_map.hpp>

/**
 * Sets up a stream
 *
 * @param[in,out]	stream			Stream to set up
 * @param[in]		filename		Filename
 * @param[in]		default_format	Default file format ("gz", "bz2", or "").
 * 									If not specified, format will be guessed based on
 * 									file extension
 */
template<class T>
std::string setStream(  boost::iostreams::filtering_stream<T> &stream,
						const std::string &filename,
				   	    const std::string &default_format="" );


/**
 * Sets up a log4cxx subsystem based on supplied config
 * If exists, it uses config["log"] for PropertyConfigurator
 *
 * @param[in]	config		Parsed configuration
 */
void setLogging( const boost::program_options::variables_map &config );

const int MRT_ANY=0;
const int MRT_IPv4=1;
const int MRT_IPv6=2;
const int MRT_UNKNOWN=99;

/**
 * Determines type of MRT message
 */
int determineMRTType( const MRTMessagePtr &msg );

#endif

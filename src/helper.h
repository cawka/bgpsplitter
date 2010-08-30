
#ifndef _HELPER_H_
#define _HELPER_H_

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
						const char *filename,
				   	    const char *default_format="" );


/**
 * Sets up a log4cxx subsystem based on supplied config
 * If exists, it uses config["log"] for PropertyConfigurator
 *
 * @param[in]	config		Parsed configuration
 */
void setLogging( const boost::program_options::variables_map &config );

#endif

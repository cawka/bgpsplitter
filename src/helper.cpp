
#include "helper.h"

#include <boost/regex.hpp>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/detail/iostream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

namespace io = boost::iostreams;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#include <log4cxx/logger.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/patternlayout.h>
#include <log4cxx/level.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/defaultconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

using namespace std;
using namespace boost;

static LoggerPtr _Logger=Logger::getLogger( "simple.helper" );


static void setStream( io::filtering_stream<io::input> &stream, const string &format )
{
	if( format=="gz" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has GZIP format" );
		stream.push( io::gzip_decompressor() );
	}
	else if( format=="bz2" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has BZIP2 format" );
		stream.push( io::bzip2_decompressor() );
	}
}

static void setStream( io::filtering_stream<io::output> &stream, const string &format )
{
	if( format=="gz" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has GZIP format" );
		stream.push( io::gzip_compressor(io::gzip::best_compression) );
	}
	else if( format=="bz2" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has BZIP2 format" );
		stream.push( io::bzip2_compressor() );
	}
}

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
string setStream( io::filtering_stream<T> &stream,
				  const char *filename,
				  const char *default_format )
{
	string format=default_format;

	smatch m;
	if( default_format=="" &&
		regex_match(string(filename), m, regex("^.*\\.(gz|bz2)$",regex_constants::icase)) )
	{
		format=m[1];
	}

	if( string(format)!="" && string(default_format)!="" && string(format)!=string(default_format) )
	{
		LOG4CXX_WARN( _Logger, "Default format is specified [" << default_format
				<< "], but guessed format is ["<<format<<"]" );
	}

	setStream( stream, format );

	return format;
}

template string setStream( io::filtering_stream<io::input> &stream,
		  const char *filename,
		  const char *default_format );

template string setStream( io::filtering_stream<io::output> &stream,
		  const char *filename,
		  const char *default_format );


/**
 * Sets up a log4cxx subsystem based on supplied config
 * If exists, it uses config["log"] for PropertyConfigurator
 *
 * @param[in]	config		Parsed configuration
 */
void setLogging( const boost::program_options::variables_map &config )
{
	// configure Logger
	if( config.count("log")>0 )
		PropertyConfigurator::configureAndWatch( config["log"].as<string>() );
    else if( fs::exists("log4cxx.properties") )
		PropertyConfigurator::configureAndWatch( "log4cxx.properties" );
	else
	{
		PatternLayoutPtr   layout   ( new PatternLayout("%d{HH:mm:ss} %p %c{1} - %m%n") );
		ConsoleAppenderPtr appender ( new ConsoleAppender( layout ) );

		BasicConfigurator::configure( appender );
		Logger::getRootLogger()->setLevel( log4cxx::Level::getError() );
	}
}

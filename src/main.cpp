/*
 * Copyright (c) 2010 University of California, Los Angeles All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of NLnetLabs nor the names of its
 *     contributors may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <bgpparser.h>
#include <MRTCommonHeader.h>
#include <MRTBgp4MPStateChange.h>
#include <MRTBgp4MPMessage.h>
#include <MRTBgp4MPEntry.h>
#include <MRTBgp4MPSnapshot.h>
#include <MRTTblDump.h>
#include <MRTTblDumpV2PeerIndexTbl.h>
#include <MRTTblDumpV2RibIPv4Unicast.h>
#include <MRTTblDumpV2RibIPv4Multicast.h>
#include <MRTTblDumpV2RibIPv6Unicast.h>
#include <MRTTblDumpV2RibIPv6Multicast.h>

#include <BGPCommonHeader.h>
#include <BGPUpdate.h>

#include <AttributeType.h>
#include <AttributeTypeOrigin.h>
#include <Exceptions.h>

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

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
namespace fs = boost::filesystem;

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
namespace po = boost::program_options;

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/detail/iostream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/scoped_array.hpp>
namespace io = boost::iostreams;

#include <MRTStructure.h>
#include <MRTCommonHeader.h>

using namespace std;

static LoggerPtr _log=Logger::getLogger( "simple" );


int main( int argc, char** argv )
{
    po::variables_map CONFIG;

	po::options_description opts( "General options" );
	opts.add_options()
		( "help", "Print this help message" )
		( "log",   po::value<string>(), "log4cxx configuration file" )
		( "file",  po::value<string>(),
				  "MRT file or - to input from stdin" )
		( "format", po::value<string>(),
				  "Compression format: txt, z, gz, or bz2 (will be overridden by the actual extension of the file)" )
	;

	po::positional_options_description p;
	p.add("file", -1);

	try
	{
		po::store( po::command_line_parser(argc, argv).
						options(opts).
						positional(p).run(),
				   CONFIG );
	}
	catch( const po::error &error )
	{
		cerr << "ERROR: " << error.what() << endl
			 <<	endl
			 <<	opts << endl;

		exit( 2 );
	}

	// configure Logger
	if( CONFIG.count("log")>0 )
		PropertyConfigurator::configureAndWatch( CONFIG["log"].as<string>() );
    else if( fs::exists("log4cxx.properties") )
		PropertyConfigurator::configureAndWatch( "log4cxx.properties" );
	else
	{
		PatternLayoutPtr   layout   ( new PatternLayout("%d{HH:mm:ss} %p %c{1} - %m%n") );
		ConsoleAppenderPtr appender ( new ConsoleAppender( layout ) );

		BasicConfigurator::configure( appender );
		Logger::getRootLogger()->setLevel( log4cxx::Level::getError() );
	}

	if( CONFIG.count("file")==0 )
	{
		cerr << "ERROR: Input file should be specified" << endl
			 << endl
			 << opts << endl;
		exit( 1 );
	}

	////////////////////////////////////////////////////////////////////////////
	string format=CONFIG.count("format")>0 ? CONFIG["format"].as<string>( ) : "";
	////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////
	string filename=CONFIG["file"].as<string>();
	boost::smatch m;
	if( boost::regex_match(filename, m, boost::regex("^.*\\.(gz|bz2)$")) ) format=m[1];
	////////////////////////////////////////////////////////////////////////////

	io::filtering_istream in;

	if( format=="gz" )
	{
		_log->debug( "Input file has GZIP format" );
		in.push( io::gzip_decompressor() );
	}
	else if( format=="bz2" )
	{
		_log->debug( "Input file has BZIP2 format" );
		in.push( io::bzip2_decompressor() );
	}

	ifstream input_file( filename.c_str(), ios_base::in | ios_base::binary );
	if( !input_file.is_open() )
	{
		cerr << "ERROR: " << "cannot open file [" <<  filename  << "] for reading" << endl
			 << endl;
		exit( 3 );
	}

	if( filename=="-" )
		in.push( cin );
	else
		in.push( input_file );

    try
    {
    	while( in.peek()!=-1 )
    	{
           	try
        	{
                MRTMessagePtr msg=MRTCommonHeader::newMessage( in );
            }
            catch( BGPParserError e )
            {
                LOG4CXX_ERROR( _log, e.what() );
            }
    	}
	}
//	catch( io::gzip_error e )
//	catch( io::bzip2_error e )
	catch( std::istream::failure e )
	{
		cerr << "ERROR: " << e.what() << endl;
		exit( 10 );
	}
	catch( ... )
	{
		cerr << "Unknown exception" << endl;
		exit( 10 );
	}

	_log->info( "Parsing ended" );
	return 0;
}

// vim: sw=4 ts=4 sts=4 expandtab

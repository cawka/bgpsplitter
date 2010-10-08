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
#include <Exceptions.h>

#include <iostream>
#include <fstream>
#include <signal.h>

#ifdef LOG4CXX
#include <log4cxx/logger.h>
using namespace log4cxx;
#endif

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
namespace po = boost::program_options;

#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/flush.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
namespace io = boost::iostreams;

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

using namespace std;
using namespace boost;

#include "helper.h"

#ifdef LOG4CXX
static LoggerPtr _log=Logger::getLogger( "bgpsplitter" );
#endif

static volatile bool NeedStop=false;
static volatile int RetCode=0;

void forceStop( int sig )
{
	NeedStop=true;
    RetCode=999;
}

int main( int argc, char** argv )
{
	po::variables_map CONFIG;

	po::options_description opts( "General options" );
	opts.add_options()
		( "help,h",  "Print this help message" )
#ifdef LOG4CXX
		( "log,l",   po::value<string>(), "log4cxx configuration file" )
#endif
		( "input,i", po::value<string>(),
				     "Input MRT file" )
		( "output,o",  po::value<string>()	->default_value("-"),
				     "Output MRT file for IPv6 data only (should be in the same format as input)")
		( "force-output",
					 "Force overwriting existing output file")
        ( "skip-if-exists",
                     "Silently skip existing output file")
        ( "ipv6",    "Output IPv6-related MRT records (default)")
        ( "ipv4",    "Output IPv4-related MRT records")
	;

	po::positional_options_description p;
	p.add( "input", 1 );
	p.add( "output", 1 );

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


	if( CONFIG.count("help")>0 )
	{
		cout << opts << endl;
		exit( 0 );
	}

	setLogging( CONFIG );

	if( CONFIG.count("input")==0 )
	{
		cerr << "ERROR: Input file should be specified" << endl
			 << endl
			 << opts << endl;
		exit( 1 );
	}

	if( CONFIG.count("output")==0 )
	{
		cerr << "ERROR: Output file should be specified" << endl
			<< endl
			<< opts << endl;
		exit( 1 );
	}

	bool ipv6=true;
	bool ipv4=false;

	if( CONFIG.count("ipv4")>0 )
	{
		ipv4=true; ipv6=false;
	}
	if( CONFIG.count("ipv6")>0 ) //if ipv6 is explicitly specified
	{
		ipv6=true;
	}

	/////////////////////////////////////////////////////////////////////////////
	string ifilename=CONFIG["input"].as<string>( );
	string ofilename=CONFIG["output"].as<string>( );
	if( ofilename!="-" && ofilename!="/dev/null"
			&& fs::exists(ofilename)
			&& CONFIG.count("force-output")==0 )
	{
        if( CONFIG.count("skip-if-exists")>0 )
        {
            LOG4CXX_INFO( _log, "Skipping existent file " << ofilename ); 
            exit( 0 );
        }
        else
        {
	    	cerr << "ERROR: Output file exists, use --force-output to force overwriting" << endl;
    		exit( 11 );
        }
	}

    //////////////////////////////////////////////////////////////////////////
    io::filtering_istream in;

	string iformat=
			setStream( in, ifilename.c_str()/*, default_format.c_str()*/ );

	bool useStdIn=false;
	ifstream ifile( ifilename.c_str(), ios_base::in | ios_base::binary );
	if( ifilename!="-" )
	{
		if( !ifile.is_open() )
		{
			cerr << "ERROR: Cannot open file [" << ifilename << "] for reading" << endl;
			exit( 10 );
		}
		in.push( ifile );
	}
	else
		useStdIn=true;

	/////////////////////////////////////////////////////////////////////////////
	io::filtering_ostream out;

	string oformat=
			setStream( out, ofilename.c_str()/*, default_format.c_str()*/ );

	if( iformat!=oformat )
	{
		LOG4CXX_WARN( _log, "Input ["<< iformat
				<<"] and output ["<< oformat
				<<"] formats are not the same" );
	}

	bool useStdOut=false;
	ofstream ofile( ofilename.c_str(), ios_base::out | ios_base::trunc | ios_base::binary );
	if( ofilename!="-" )
	{
		if( !ofile.is_open() )
		{
			cerr << "ERROR: Cannot open file [" << ofilename << "] for writing" << endl;
			exit( 10 );
		}
		out.push( ofile );
	}
	else
		useStdOut=true;

	//set up signalig
	signal( SIGINT, forceStop );
	signal( SIGQUIT, forceStop );
	signal( SIGTERM, forceStop );

	LOG4CXX_INFO( _log, "Parsing input file [" << ifilename << "]" );
	unsigned long long count=0;
	unsigned long long count_error=0;
	unsigned long long count_output=0;
    try
    {
        io::flush( out );

    	while( !NeedStop && in.peek()!=-1 )
    	{
    		count++;
           	try
        	{
                MRTMessagePtr msg=MRTCommonHeader::newMessage( in );

                switch( determineMRTType(msg) )
                {
                case MRT_IPv4:
                	if( ipv4 )
                		break;
                	else
                		continue;
                	break;

                case MRT_IPv6:
                	if( ipv6 )
                		break;
                	else
                		continue;
                	break;

                case MRT_ANY:
                case MRT_UNKNOWN:
                	break;
                }

                //case
                io::write( out, reinterpret_cast<const char*>(&msg->getHeader()),
                				sizeof(msg->getHeader()) );
                io::write( out, msg->getData().get(), msg->getLength() );
                count_output++;
            }
            catch( MRTException e )
            {
                LOG4CXX_ERROR( _log, e.what() );
                count_error++;
            }
            catch( BGPTextError e )
            {
                LOG4CXX_ERROR( _log, e.what() );
                count_error++;
            }
            catch( BGPError e )
            {
                //information should be already logged, if the logger for bgpparser is enabled
                count_error++;
            }
    	}
	}
    catch( BGPParserError e )
    {
		cerr << "ERROR (bgpparser): " << e.what() << endl;
        LOG4CXX_ERROR( _log, "ERROR (bgpparser): " << e.what() );
		NeedStop=true;
    }
	catch( io::gzip_error e )
	{
		cerr << "ERROR (gzip): " << e.what() << endl;
        LOG4CXX_ERROR( _log, "ERROR (gzip): " << e.what() );
		NeedStop=true;
	}
	catch( io::bzip2_error e )
	{
		cerr << "ERROR (bzip): " << e.what() << endl;
        LOG4CXX_ERROR( _log, "ERROR (bzip): code: " << e.error() << ", message: " << e.what() );
		NeedStop=true;
	}
	catch( ios_base::failure e )
	{
		cerr << "ERROR (ios_base): " << e.what() << endl;
        LOG4CXX_ERROR( _log, "ERROR (ios_base): " << e.what() );
		NeedStop=true;
	}
	catch( ... )
	{
		cerr << "Unknown exception" << endl;
        LOG4CXX_ERROR( _log, "ERROR: Unknown exception" );
		NeedStop=true;
        throw;
	}

    io::close( out );
    io::close( in );
    if( !useStdOut ) ofile.close( ); /* handle the case of stdout */
    if( !useStdIn  ) ifile.close( ); /* handle the case of stdin  */

	LOG4CXX_INFO( _log, count << " MRTs parsed" );
    if( count_error>0 )
    {
	    LOG4CXX_INFO( _log, count_error << " MRTs skipped due to parsing error" );
    }
	LOG4CXX_INFO( _log, count_output << " MRTs filtered" );

	if( NeedStop )
	{
		LOG4CXX_INFO( _log, "Parsing was terminated, removing partially processed files" );
		// @TODO remove any output file that was produced
		std::remove( ofilename.c_str() );
	}

	LOG4CXX_INFO( _log, "Parsing ended" );
	return RetCode;
}

// vim: sw=4 ts=4 sts=4 expandtab

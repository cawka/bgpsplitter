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
#include <MRTBgp4MPMessage.h>
#include <BGPCommonHeader.h>
#include <BGPUpdate.h>
#include <BGPAttribute.h>
#include <AttributeType.h>
#include <AttributeTypeMPReachNLRI.h>
#include <AttributeTypeMPUnreachNLRI.h>
#include <Exceptions.h>

#include <iostream>
#include <fstream>
#include <signal.h>

#include <log4cxx/logger.h>
using namespace log4cxx;

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

#include <boost/foreach.hpp>

using namespace std;
using namespace boost;

#include "helper.h"

static LoggerPtr _log=Logger::getLogger( "bgpsplitter" );
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
		( "log,l",   po::value<string>(), "log4cxx configuration file" )
		( "input,i", po::value<string>(),
				     "Input MRT file" )
		( "output,o",  po::value<string>(),
				     "Output MRT file for IPv6 data only (should be in the same format as input)")
		( "force-output",
					 "Force overwriting existing output file")
        ( "skip-if-exists",
                     "Silently skip existing output file")
//		( "out4,O",  po::value<string>(),
//				     "Output MRT file for IPv4 data only (should be in the same format as input)")
//		( "format", po::value<string>(),
//				  "Compression format: txt, z, gz, or bz2 (will be overridden by the actual extension of the file)" )
	;

	try
	{
		po::store( po::command_line_parser(argc, argv).
						options(opts).run(),
//						positional(p).run(),
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

	/////////////////////////////////////////////////////////////////////////////
	string ifilename=CONFIG["input"].as<string>( );
	io::filtering_istream in;

	string iformat=
			setStream( in, ifilename.c_str()/*, default_format.c_str()*/ );

	ifstream ifile( ifilename.c_str(), ios_base::in | ios_base::binary );
	if( !ifile.is_open() )
	{
		cerr << "ERROR: Cannot open file [" << ifilename << "] for reading" << endl;
		exit( 10 );
	}
	in.push( ifile );

	/////////////////////////////////////////////////////////////////////////////
	string ofilename=CONFIG["output"].as<string>( );
	io::filtering_ostream out;

	string oformat=
			setStream( out, ofilename.c_str()/*, default_format.c_str()*/ );

	if( iformat!=oformat )
	{
		LOG4CXX_WARN( _log, "Input ["<< iformat
				<<"] and output ["<< oformat
				<<"] formats are not the same" );
	}

	if( fs::exists(ofilename) && CONFIG.count("force-output")==0 )
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

	ofstream ofile( ofilename.c_str(), ios_base::out | ios_base::trunc | ios_base::binary );
	if( !ofile.is_open() )
	{
		cerr << "ERROR: Cannot open file [" << ofilename << "] for writing" << endl;
		exit( 10 );
	}
	out.push( ofile );

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

                switch( msg->getType() )
                {
                case BGP4MP:
                	switch( msg->getSubType() )
                	{
                	case BGP4MP_STATE_CHANGE:
                		//OK, will write these subtypes
                		break;
                	case BGP4MP_MESSAGE:
                	case BGP4MP_MESSAGE_AS4:
                	{
                		BGPMessagePtr bgp_message=
                				dynamic_pointer_cast<MRTBgp4MPMessage>( msg )
                				->getPayload( );

                		switch( bgp_message->getType() )
                		{
                		case BGPCommonHeader::OPEN:
                			//OK, will write these subtypes
                			break;
                		case BGPCommonHeader::UPDATE:
                		{
                			BGPUpdatePtr bgp_update=
                					dynamic_pointer_cast<BGPUpdate>( bgp_message );

                			const list<BGPAttributePtr> &attrs=bgp_update->getPathAttributes();

                			bool isIPv6=false;

                			BOOST_FOREACH( BGPAttributePtr attr, attrs )
                			{
                				if( attr->getAttributeTypeCode()==AttributeType::MP_REACH_NLRI )
                				{
                					isIPv6 |=
                						(AFI_IPv6 ==
										dynamic_pointer_cast<AttributeTypeMPReachNLRI>( attr->getAttributeValue() )
												->getAFI( ));
                				}
                				else if( attr->getAttributeTypeCode()==AttributeType::MP_UNREACH_NLRI )
                				{
                					isIPv6 |=
                						(AFI_IPv6 ==
										dynamic_pointer_cast<AttributeTypeMPUnreachNLRI>( attr->getAttributeValue() )
												->getAFI( ));
                				}
                			}

                			if( !isIPv6 )
                			{

                        		///////////////////////////////////////////////////////////
                        		continue; //Nope, will skip these subtypes
                        		///////////////////////////////////////////////////////////

                			}

                			//OK, will write these

                			if( bgp_update->getNlriLength()>0 )
                			{
                				LOG4CXX_ERROR(_log,
                						"BGP_UPDATE carries IPv6 MP_REACH_NLRI and contains non-zero NLRI section"
                						);
                			}

                			if( bgp_update->getWithdrawnRoutesLength()>0 )
                			{
                				LOG4CXX_ERROR(_log,
                						"BGP_UPDATE carries IPv6 MP_UNREACH_NLRI and contains non-zero Withdrawn section"
                						);
                			}

                			break;
                		}
                		case BGPCommonHeader::NOTIFICATION:
                		case BGPCommonHeader::KEEPALIVE:
                		case BGPCommonHeader::ROUTE_REFRESH:
                			//OK, will write these subtypes
                			break;
                		}

                		break;
                	}
                	case BGP4MP_STATE_CHANGE_AS4:
                		//OK, will write these subtypes
                		break;
//                	case BGP4MP_MESSAGE_LOCAL:
//                	case BGP4MP_MESSAGE_AS4_LOCAL:
                	}
                	break;
                case TABLE_DUMP:
                	switch( msg->getSubType() )
                	{
                	case AFI_IPv4:

                		///////////////////////////////////////////////////////////
                		continue; //Nope, will skip this subtype
                		///////////////////////////////////////////////////////////

                		break;
                	case AFI_IPv6:
                		//OK, will write this subtypes
                		break;
                	}
                	break;
                case TABLE_DUMP_V2:
                	switch( msg->getSubType() )
                	{
                	case PEER_INDEX_TABLE:
                		//OK, will write this subtype
                		break;
                	case RIB_IPV4_UNICAST:
                	case RIB_IPV4_MULTICAST:

                		///////////////////////////////////////////////////////////
                		continue; //Nope, will skip these subtypes
                		///////////////////////////////////////////////////////////

                		break;
                	case RIB_IPV6_UNICAST:
                	case RIB_IPV6_MULTICAST:
                	case RIB_GENERIC:
                		//OK, will write these subtypes
                		break;
                	}
                	break;
                default:
                	// will write an MRT with unknown type without modification
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

    	io::close( out );
    	io::close( in );
        io::flush( out );
    	ofile.close( );
    	ifile.close( );
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

	LOG4CXX_INFO( _log, count << " MRT records are parsed" );
    if( count_error>0 )
    {
	    LOG4CXX_INFO( _log, count_error << " MRT records skipped due to parsing error" );
    }
	LOG4CXX_INFO( _log, count_output << " IPv6 related MRT records are written into the output file" );

	if( NeedStop )
	{
		LOG4CXX_INFO( _log, "Parsing was terminated, removing partially processed files" );
		// @TODO remove any output file that was produced
		ofile.close( );
		std::remove( ofilename.c_str() );
	}

	LOG4CXX_INFO( _log, "Parsing ended [" << ifilename << "]" );
	return RetCode;
}

// vim: sw=4 ts=4 sts=4 expandtab

/*
 * Copyright (c) 2008,2009, University of California, Los Angeles All rights reserved.
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

#include <log4cxx/logger.h>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/propertyconfigurator.h>
#include <log4cxx/defaultconfigurator.h>
#include <log4cxx/helpers/exception.h>
using namespace log4cxx;
using namespace log4cxx::helpers;

#include <boost/filesystem.hpp>
#include <boost/regex.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/option.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/value_semantic.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/detail/iostream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/scoped_array.hpp>

namespace io = boost::iostreams;

#include <boost/asio/streambuf.hpp>


static LoggerPtr _log=Logger::getLogger( "simple" );

struct hdr_t
{
	uint32_t timestamp;
	uint16_t type, subtype;
	uint32_t length;
};

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
	if( fs::exists("log4cxx.properties") )
		PropertyConfigurator::configureAndWatch( "log4cxx.properties" );
	else
		BasicConfigurator::configure( );

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
	// not sure where exception will fire
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
		in.sync( );

	while( !in.eof() )
	{
//		char buf[MRT_COMMON_HDR_LEN];
//		boost::asio::streambuf commonHeader;
//		boost::iostreams::copy( in, commonHeader, MRT_COMMON_HDR_LEN );

/*
 * http://tools.ietf.org/id/draft-ietf-grow-mrt-03.txt

   2.  Basic MRT Format

   All MRT format messages have a common header which includes a
   timestamp, Type, Subtype, and length field.  The header is followed
   by a message field.  The basic MRT format is illustrated below.

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                           Timestamp                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             Type              |            Subtype            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                             Length                            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      Message... (variable)
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
		hdr_t hdr;
		int len=io::read( in, reinterpret_cast<char*>(&hdr), sizeof(hdr_t) );
		if( len<0 || len<sizeof(hdr_t) ) throw string( "MRT file format error" );
		in.seekg( -sizeof(hdr_t), ios_base::cur );

//		cout << "Read: " << len << endl;
//		cout << "+++++++++" << endl;
//		cout << ntohl(hdr.timestamp) << endl;
//		cout << ntohs(hdr.type) << "\t" << ntohs(hdr.subtype) << endl;
//		cout << ntohl( hdr.length ) << endl;

		break;
	}
	}
//	catch( io::gzip_error e )
//	catch( io::bzip2_error e )
	catch( std::istream::failure e )
	{
		cerr << "ERROR: " << e.what() << endl;
		exit( 10 );
	}
	catch( const string &e )
	{
		cerr << "ERROR: " << e << endl;
		exit( 10 );
	}
	catch( ... )
	{
		cerr << "Unknown exception" << endl;
		exit( 10 );
	}
//	MRTTblDumpV2PeerIndexTbl* peerIndexTbl = NULL;
//
//    CFRFILE *f;
//    f = cfr_open(pchFileName);
//    if (f)
//    {
//        // The index table needs to persist over the processing
//        uint32_t unTotalBytesRead = 0;
//
//        while(!cfr_eof(f))
//        {
//            size_t szBytesRead = 0;
//
//            /* Read header from file */
//            szBytesRead += cfr_read_n(f, rchMRTCommonHeader, MRT_COMMON_HDR_LEN);
//            if( szBytesRead < MRT_COMMON_HDR_LEN ) { break; }
//
//            /* Compute MRT message length */
//            uint32_t unMRTBodyLen = ntohl(*(uint32_t*)(rchMRTCommonHeader+8));
//            uint32_t unMRTMsgLen = MRT_COMMON_HDR_LEN + unMRTBodyLen;
//            char *pchMRTMsg = (char *)malloc(unMRTMsgLen);
//            memset(pchMRTMsg, 0, unMRTMsgLen);
//
//            /* Copy the whole MRT record */
//
//            memcpy(pchMRTMsg, (char *)rchMRTCommonHeader, MRT_COMMON_HDR_LEN);
//            szBytesRead += cfr_read_n(f, pchMRTMsg + MRT_COMMON_HDR_LEN, unMRTBodyLen);
//            unTotalBytesRead += szBytesRead;
//
//            /* Parse the common header */
//            int nSaveMsg = false;
//            uint8_t *pchMRTMsgTemp = reinterpret_cast<uint8_t *>(pchMRTMsg);
//            MRTMessage* msg = MRTCommonHeader::newMessage(&pchMRTMsgTemp);
//            if (msg == NULL) { continue; } /* EOF? */
//
//            /* Get time to display */
//            time_t      tmTime = (time_t)msg->getTimestamp();
//            struct tm   *ts;
//            char        rchTime[80];
//
//            /* Format and print the time, "ddd yyyy-mm-dd hh:mm:ss zzz" */
//            ts = localtime(&tmTime);
//            strftime(rchTime, sizeof(rchTime), "%a %Y-%m-%d %H:%M:%S %Z", ts);
//
//            if ((unFlags & PRINT_COMPACT) == 0)
//                printf("\nTIME: %s\n", rchTime);
//            else
//                printf("%u", (uint32_t)tmTime);
//
//            if ((unFlags & PRINT_COMPACT) == 0)
//            {
//                cout << "TYPE: " << rrchMRTTypes[msg->getType()] << "/";
//                cout << rrrchMRTSubTypes[msg->getType()][msg->getSubType()];
//            }
//
//            switch(msg->getType())
//            {
//                case TABLE_DUMP:
//                {
//                    if (unFlags & PRINT_COMPACT) {
//                        cout << "|" << rrchMRTTypes[msg->getType()];
//                        cout << "|" << msg->getSubType();
//                    }
//                    else
//                    {
//                        cout << endl;
//                    }
//                    switch(msg->getSubType())
//                    {
//                        case AFI_IPv4:
//                        case AFI_IPv6:
//                        {
//                            MRTTblDump* tblDump = (MRTTblDump*)msg;
//                            tblDump->printMeCompact();
//                        }
//                        break;
//
//                        default:
//                            break;
//                    }
//                }
//                break;
//
//                case TABLE_DUMP_V2:
//                {
//                    if (unFlags & PRINT_COMPACT) {
//                        cout << "|" << rrchMRTTypes[msg->getType()];
//                        cout << "|";
//                    }
//                    else
//                    {
//                        cout << endl;
//                    }
//                    switch(msg->getSubType())
//                    {
//                        case RIB_IPV4_UNICAST:
//                        {
//                            MRTTblDumpV2RibIPv4Unicast* tblDumpIpv4Msg = (MRTTblDumpV2RibIPv4Unicast*)msg;
//                            // consider passing peerIndexTbl to printMe
//                            if (unFlags & PRINT_COMPACT)
//                                tblDumpIpv4Msg->printMeCompact(peerIndexTbl);
//                            else
//                                tblDumpIpv4Msg->printMe(peerIndexTbl);
//                        }
//                        break;
//
//                        case RIB_IPV4_MULTICAST:
//                        {
//                            MRTTblDumpV2RibIPv4Multicast* tblDumpIpv4Msg = (MRTTblDumpV2RibIPv4Multicast*)msg;
//                            //cout << endl;
//                            tblDumpIpv4Msg->printMe(peerIndexTbl);
//                        }
//                        break;
//
//                        case RIB_IPV6_UNICAST:
//                        {
//                            MRTTblDumpV2RibIPv6Unicast* tblDumpIpv6Msg = (MRTTblDumpV2RibIPv6Unicast*)msg;
//                            //cout << endl;
//                            if (unFlags & PRINT_COMPACT)
//                                tblDumpIpv6Msg->printMeCompact(peerIndexTbl);
//                            else
//                                tblDumpIpv6Msg->printMe(peerIndexTbl);
//                        }
//                        break;
//
//                        case RIB_IPV6_MULTICAST:
//                        {
//                            MRTTblDumpV2RibIPv6Multicast *tblDumpIpv6Msg = (MRTTblDumpV2RibIPv6Multicast *)msg;
//                            //cout << endl;
//                            tblDumpIpv6Msg->printMe(peerIndexTbl);
//                        }
//                        break;
//
//                        case PEER_INDEX_TABLE:
//                        {
//                            // We need to preserve the index while we process the list of MRT table records
//                            nSaveMsg = true;
//                            if (peerIndexTbl) {
//                                delete peerIndexTbl; // Make way for the next one.
//                            }
//
//                            //cout << "  Setting a peer index table." << cout;
//                            peerIndexTbl = (MRTTblDumpV2PeerIndexTbl*)msg;
//                            cout << "PEERINDEXTABLE";
//                        }
//                        break;
//
//                        default:
//                            break;
//                    }
//                }
//                break;
//
//                case BGP4MP:
//                {
//                    switch(msg->getSubType())
//                    {
//                        case BGP4MP_MESSAGE:
//                        {
//                            if (unFlags & PRINT_COMPACT ) {
//                                cout << "|" << rrchMRTTypes[msg->getType()];
//                            }
//                            MRTBgp4MPMessage* bgp4MPmsg = (MRTBgp4MPMessage*)msg;
//                            BGPMessage* bgpMessage = (BGPMessage*)msg->getPayload();
//                            if( bgpMessage == NULL ) { break; }
//                            switch(bgpMessage->Type())
//                            {
//                                case BGPCommonHeader::UPDATE:
//                                {
//                                    if ((unFlags & PRINT_COMPACT) == 0)
//                                    {
//                                        cout << "/" << "Update" ;
//                                        cout << endl;
//                                        BGPUpdate* bgpUpdate = (BGPUpdate*)bgpMessage;
//                                        cout << "FROM: ";
//                                        PRINT_IP_ADDR(bgp4MPmsg->getPeerIP().ipv4);
//                                        cout << " AS" << bgp4MPmsg->getPeerAS() << endl;
//                                        cout << "TO: ";
//                                        PRINT_IP_ADDR(bgp4MPmsg->getLocalIP().ipv4);
//                                        cout << " AS" << bgp4MPmsg->getLocalAS() << endl;
//
//                                        bgpUpdate->printMe();
//                                    }
//                                    else
//                                    {
//                                        cout << "|";
//                                        BGPUpdate* bgpUpdate = (BGPUpdate*)bgpMessage;
//                                        if( bgp4MPmsg->getAddressFamily() == AFI_IPv4 ) {
//                                            PRINT_IP_ADDR(bgp4MPmsg->getPeerIP().ipv4);
//                                            cout << "|" << bgp4MPmsg->getPeerAS() << "|";
//                                            PRINT_IP_ADDR(bgp4MPmsg->getLocalIP().ipv4);
//                                            cout << "|" << bgp4MPmsg->getLocalAS() << "|";
//                                        } else {
//                                            PRINT_IPv6_ADDR(bgp4MPmsg->getPeerIP().ipv6);
//                                            cout << "|" << bgp4MPmsg->getPeerAS() << "|";
//                                            PRINT_IPv6_ADDR(bgp4MPmsg->getLocalIP().ipv6);
//                                            cout << "|" << bgp4MPmsg->getLocalAS() << "|";
//                                        }
//                                        bgpUpdate->printMeCompact();
//                                    }
//                                }
//                                break;
//
//                                case BGPCommonHeader::KEEPALIVE:
//                                {
//                                    cout << "/" << "KEEPALIVE";
//                                }
//                                break;
//
//                                default:
//                                break;
//                            }
//                        }
//                        break;
//
//                        case BGP4MP_MESSAGE_AS4:
//                        {
//                            if (unFlags & PRINT_COMPACT ) {
//                                cout << "|" << rrchMRTTypes[msg->getType()];
//                            }
//                            MRTBgp4MPMessage* bgp4MPmsg = (MRTBgp4MPMessage*)msg;
//                            BGPMessage* bgpMessage = (BGPMessage*)msg->getPayload();
//                            if( bgpMessage == NULL ) { break; }
//                            switch(bgpMessage->Type())
//                            {
//                                case BGPCommonHeader::UPDATE:
//                                {
//                                    if ((unFlags & PRINT_COMPACT) == 0)
//                                    {
//                                        cout << "/" << "Update" ;
//                                        cout << endl;
//                                        BGPUpdate* bgpUpdate = (BGPUpdate*)bgpMessage;
//                                        cout << "FROM: ";
//                                        PRINT_IP_ADDR(bgp4MPmsg->getPeerIP().ipv4);
//                                        cout << " AS" << bgp4MPmsg->getPeerAS() << endl;
//                                        cout << "TO: ";
//                                        PRINT_IP_ADDR(bgp4MPmsg->getLocalIP().ipv4);
//                                        cout << " AS" << bgp4MPmsg->getLocalAS() << endl;
//
//                                        bgpUpdate->printMe();
//                                    }
//                                    else
//                                    {
//                                        cout << "|";
//                                        BGPUpdate* bgpUpdate = (BGPUpdate*)bgpMessage;
//                                        if( bgp4MPmsg->getAddressFamily() == AFI_IPv4 ) {
//                                            PRINT_IP_ADDR(bgp4MPmsg->getPeerIP().ipv4);
//                                        } else {
//                                            PRINT_IPv6_ADDR(bgp4MPmsg->getPeerIP().ipv6);
//                                        }
//
//                                        uint16_t t, b;
//                                        uint32_t asn;
//                                        cout << "|";
//                                        asn = bgp4MPmsg->getPeerAS();
//                                        t = (uint16_t)((asn>>16)&0xFFFF);
//                                        b = (uint16_t)((asn)&0xFFFF);
//                                        if( t == 0 ) {
//                                            printf("%u",b);
//                                        } else {
//                                            printf("%u.%u",t,b);
//                                        }
//                                        cout << "|";
//
//                                        if( bgp4MPmsg->getAddressFamily() == AFI_IPv4 ) {
//                                            PRINT_IP_ADDR(bgp4MPmsg->getLocalIP().ipv4);
//                                        } else {
//                                            PRINT_IPv6_ADDR(bgp4MPmsg->getLocalIP().ipv6);
//                                        }
//
//                                        cout << "|";
//                                        asn = bgp4MPmsg->getLocalAS();
//                                        t = (uint16_t)((asn>>16)&0xFFFF);
//                                        b = (uint16_t)((asn)&0xFFFF);
//                                        if( t == 0 ) {
//                                            printf("%u",b);
//                                        } else {
//                                            printf("%u.%u",t,b);
//                                        }
//                                        cout << "|";
//                                        bgpUpdate->printMeCompact();
//                                    }
//                                }
//                                break;
//
//                                case BGPCommonHeader::KEEPALIVE:
//                                {
//                                    cout << "/" << "KEEPALIVE";
//                                }
//                                break;
//
//                                default: break;
//                            }
//                        }
//                        break;
//
//                        case BGP4MP_STATE_CHANGE:
//                        {
//                            MRTBgp4MPStateChange* bgp4MPmsg = (MRTBgp4MPStateChange*)msg;
//                            if (unFlags & PRINT_COMPACT ) {
//                                cout << "|" << "STATE" << "|";
//                                bgp4MPmsg->printMeCompact();
//                            } else {
//                                cout << "/" << "STATE_CHANGE";
//                                bgp4MPmsg->printMe();
//                            }
//                        }
//                        break;
//                        case BGP4MP_STATE_CHANGE_AS4:
//                        {
//                            MRTBgp4MPStateChange* bgp4MPmsg = (MRTBgp4MPStateChange*)msg;
//                            if (unFlags & PRINT_COMPACT ) {
//                                cout << "|" << "STATE" << "|";
//                                bgp4MPmsg->printMeCompact();
//                            } else {
//                                cout << "/" << "STATE_CHANGE_AS4";
//                                bgp4MPmsg->printMe();
//                            }
//                        }
//                        break;
//
//                        default: break;
//                    }
//                }
//                break;
//
//                default: break;
//            }
//
//            cout << endl;
//            delete [] pchMRTMsg;
//            /* if this is not peerIndexTable, free allocated memory */
//            if( !nSaveMsg ) { if( msg != NULL ) delete msg; }
//        }
//
//        if (peerIndexTbl)
//        {
//            delete peerIndexTbl;
//            peerIndexTbl = NULL;
//        }
//    }

	_log->info( "Parsing ended" );
	return 0;
}

// vim: sw=4 ts=4 sts=4 expandtab

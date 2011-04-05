
#include <bgpparser.h>

#include "helper.h"

#include <parsers/MRTBgp4MPMessage.h>
#include <parsers/BGPCommonHeader.h>
#include <parsers/BGPUpdate.h>
#include <parsers/BGPAttribute.h>
#include <parsers/AttributeType.h>
#include <parsers/AttributeTypeMPReachNLRI.h>
#include <parsers/AttributeTypeMPUnreachNLRI.h>

#include <boost/regex.hpp>

#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/detail/iostream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/bzip2.hpp>

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

#ifdef LOG4CXX
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

static LoggerPtr _Logger=Logger::getLogger( "bgpsplitter" );
#endif

#include <boost/foreach.hpp>

using namespace std;
using namespace boost;
using namespace boost::iostreams;

static void setStream( filtering_stream<input> &stream, const string &format )
{
	if( format=="gz" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has GZIP format" );
		stream.push( gzip_decompressor() );
	}
	else if( format=="bz2" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has BZIP2 format" );
		stream.push( bzip2_decompressor() );
	}
}

static void setStream( filtering_stream<output> &stream, const string &format )
{
	if( format=="gz" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has GZIP format" );
		stream.push( gzip_compressor(gzip::best_compression) );
	}
	else if( format=="bz2" )
	{
		LOG4CXX_TRACE( _Logger, "Input file has BZIP2 format" );
		stream.push( bzip2_compressor() );
	}
}

static void pushSTDIO( filtering_stream<input> &stream )
{
	stream.push( cin );
}

static void pushSTDIO( filtering_stream<output> &stream )
{
	stream.push( cout );
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
string setStream( filtering_stream<T> &stream,
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

	if( string(filename)=="-" ) pushSTDIO( stream );

	return format;
}

template string setStream( filtering_stream<input> &stream,
		  const char *filename,
		  const char *default_format );

template string setStream( filtering_stream<output> &stream,
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
#ifdef LOG4CXX
	// configure Logger
	if( config.count("log")>0 )
		PropertyConfigurator::configure( config["log"].as<string>() );
    else if( fs::exists("log4cxx.properties") )
		PropertyConfigurator::configure( "log4cxx.properties" );
	else
	{
		PatternLayoutPtr   layout   ( new PatternLayout("%d{HH:mm:ss} %p %c{1} - %m%n") );
		ConsoleAppenderPtr appender ( new ConsoleAppender( layout ) );

		BasicConfigurator::configure( appender );
		Logger::getRootLogger()->setLevel( log4cxx::Level::getOff() );
	}
#endif
}

/**
 * Determines type of MRT message
 */
int determineMRTType( const MRTMessagePtr &msg )
{
    switch( msg->getType() )
    {
    case BGP4MP:
    	switch( msg->getSubType() )
    	{
    	case BGP4MP_STATE_CHANGE:

    		///////////////////////////////////////////////////////////
    		return MRT_ANY;
    		///////////////////////////////////////////////////////////
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

        		///////////////////////////////////////////////////////////
    			return MRT_ANY;
        		///////////////////////////////////////////////////////////
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
    				return MRT_IPv4;
            		///////////////////////////////////////////////////////////

    			}

    			//OK, will write these

    			if( bgp_update->getNlriLength()>0 )
    			{
    				LOG4CXX_ERROR(_Logger,
    						"BGP_UPDATE carries IPv6 MP_REACH_NLRI and contains non-zero NLRI section"
    						);
    	    		///////////////////////////////////////////////////////////
    				return MRT_ANY;
    	    		///////////////////////////////////////////////////////////
    			}

    			if( bgp_update->getWithdrawnRoutesLength()>0 )
    			{
    				LOG4CXX_ERROR(_Logger,
    						"BGP_UPDATE carries IPv6 MP_UNREACH_NLRI and contains non-zero Withdrawn section"
    						);
    	    		///////////////////////////////////////////////////////////
    				return MRT_ANY;
    	    		///////////////////////////////////////////////////////////
    			}

        		///////////////////////////////////////////////////////////
    			return MRT_IPv6;
        		///////////////////////////////////////////////////////////
    			break;
    		}
    		case BGPCommonHeader::NOTIFICATION:
    		case BGPCommonHeader::KEEPALIVE:
    		case BGPCommonHeader::ROUTE_REFRESH:

        		///////////////////////////////////////////////////////////
    			return MRT_ANY;
        		///////////////////////////////////////////////////////////
    			break;
    		}

    		break;
    	}
    	case BGP4MP_STATE_CHANGE_AS4:

    		///////////////////////////////////////////////////////////
    		return MRT_ANY;
    		///////////////////////////////////////////////////////////
    		break;
    	}
    	break;
    case TABLE_DUMP:
    	switch( msg->getSubType() )
    	{
    	case AFI_IPv4:

    		///////////////////////////////////////////////////////////
    		return MRT_IPv4;
    		///////////////////////////////////////////////////////////

    		break;
    	case AFI_IPv6:

    		///////////////////////////////////////////////////////////
    		return MRT_IPv6;
    		///////////////////////////////////////////////////////////
    		break;
    	}
    	break;
    case TABLE_DUMP_V2:
    	switch( msg->getSubType() )
    	{
    	case PEER_INDEX_TABLE:

    		///////////////////////////////////////////////////////////
    		return MRT_ANY;
    		///////////////////////////////////////////////////////////
    		break;

    	case RIB_IPV4_UNICAST:
    	case RIB_IPV4_MULTICAST:

    		///////////////////////////////////////////////////////////
    		return MRT_IPv4;
    		///////////////////////////////////////////////////////////

    		break;
    	case RIB_IPV6_UNICAST:
    	case RIB_IPV6_MULTICAST:

    		///////////////////////////////////////////////////////////
    		return MRT_IPv6;
    		///////////////////////////////////////////////////////////
    		break;

    	case RIB_GENERIC:

    		///////////////////////////////////////////////////////////
    		return MRT_ANY;
    		///////////////////////////////////////////////////////////
    		break;
    	}
    	break;

	case MRT_INVALID:
    default:
		///////////////////////////////////////////////////////////
    	return MRT_UNKNOWN;
		///////////////////////////////////////////////////////////
    	break;
    }
}

AM_CPPFLAGS = $(log4cxx_CFLAGS) $(bgpparser_CFLAGS) $(BOOST_CPPFLAGS)

bin_PROGRAMS = bgpsplitter

bgpsplitter_SOURCES = main.cpp   cfile_tools.c cfile_tools.h
bgpsplitter_LDFLAGS = -lz -lbz2 $(log4cxx_LIBS) $(BOOST_SYSTEM_LIB) $(BOOST_FILESYSTEM_LIB) $(bgpparser_LIB) $(BOOST_LDFLAGS)


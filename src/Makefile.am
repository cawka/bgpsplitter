AM_CPPFLAGS = $(log4cxx_CFLAGS) $(bgpparser_CFLAGS) $(BOOST_CPPFLAGS)

bin_PROGRAMS = bgpsplitter

bgpsplitter_SOURCES = main.cpp \
					  helper.h helper.cpp

bgpsplitter_LDFLAGS = \
	$(bgpparser_LIBS) \
	$(log4cxx_LIBS) \
	$(BOOST_LDFLAGS) \
		$(BOOST_SYSTEM_LIB) \
		$(BOOST_FILESYSTEM_LIB) \
		$(BOOST_PROGRAM_OPTIONS_LIB) \
		$(BOOST_REGEX_LIB) \
		$(BOOST_IOSTREAMS_LIB)


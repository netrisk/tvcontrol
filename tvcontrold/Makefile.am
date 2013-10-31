bin_PROGRAMS=tvcontrold
tvcontrold_SOURCES=\
	tvcontrold.cpp \
	tc_log.cpp \
	tc_cec.cpp \
	tc_server.cpp \
	tc_cmd.cpp \
	tc_pioneer.cpp \
	tc_osd.cpp \
	tc_tools.cpp \
	tc_msg.cpp
tvcontrold_CXXFLAGS=@LIBCEC_CFLAGS@ @LIBAOSD_CFLAGS@ @LIBRSVG_CFLAGS@
tvcontrold_LDADD=@LIBCEC_LIBS@ @LIBAOSD_LIBS@ @LIBRSVG_LIBS@ -ldl

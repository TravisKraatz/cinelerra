## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include 

noinst_PROGRAMS = dump_video $(BUILDABLE_EXAMPLES)

# possible contents of BUILDABLE_EXAMPLES:
EXTRA_PROGRAMS = player_example encoder_example

AM_CFLAGS = $(SDL_CFLAGS) $(OGG_CFLAGS)
LDADD = ../lib/libtheora.la $(OGG_LIBS)

dump_video_SOURCES = dump_video.c
dump_video_LDADD = $(LDADD)

player_example_SOURCES = player_example.c
player_example_LDADD = $(LDADD) $(SDL_LIBS) $(VORBIS_LIBS)

encoder_example_SOURCES = encoder_example.c
EXTRA_encoder_example_SOURCES = getopt.c getopt1.c getopt.h
encoder_example_LDADD = $(GETOPT_OBJS) $(LDADD) $(VORBIS_LIBS) $(VORBISENC_LIBS)
encoder_example_DEPENDENCIES = $(GETOPT_OBJS)

debug:
	$(MAKE) all CFLAGS="@DEBUG@ $(CFLAGS)"

profile:
	$(MAKE) all CFLAGS="@PROFILE@ $(CFLAGS)"


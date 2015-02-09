
TOPDIR=../..
CXXFLAGS:=-ggdb -I.. -Wall $(CFLAGS)
LDLIBS:=../x86_64/db.a
LDFLAGS:=

TARGETS:=clip_accesses \
	clip_data \
	clip_del \
	clip_dups \
	clip_ls \
	dbcounts \
	delallclips \
	frame_clips \
	framediff \
	frameweights \
	histeq \
	histplot \
	pbm2frame \
	pbm2key \
	timelinechk \
	clip_assets \
	clip_frames \
	clip_weights \
	dbtv \
	frame2pbm \
	framekey \
	new_db \
	root_info \
	dmp \
	cpdb \
	add_path_pos \
	path_ls \
	dbcvt \
	xtv

all:	$(TARGETS)

dbtv:	dbtv.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) dbtv.C $(LDLIBS) -lX11 -o $@

XTV_MEDIA_OBJS:=$(TOPDIR)/cinelerra/x86_64/mediadb.o $(TOPDIR)/cinelerra/x86_64/filexml.o
XTV_LIBS:=$(TOPDIR)/libzmpeg3/x86_64/libzmpeg3.a $(TOPDIR)/db/x86_64/db.a -lX11 -lXext -lasound -lm
XTV_INCLUDES:=-I$(TOPDIR) -I$(TOPDIR)/guicast

xtv:	xtv.C $(XTV_MEDIA_OBJS) $(XTV_LIBS)
	c++ -pthread $(XTV_INCLUDES) $(CXXFLAGS) xtv.C $(XTV_MEDIA_OBJS) $(XTV_LIBS) -o xtv

clean:
	rm -f $(TARGETS) *.o a.out


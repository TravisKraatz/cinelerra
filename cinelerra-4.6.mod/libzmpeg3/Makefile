CC = gcc
NASM = nasm
USE_MMX = 0
USE_CSS = 1
A52DIR := $(shell expr a52dec* )
DYNAMIC_FONTS = 1


ifeq ("$(DST)", "")
DST=/usr/bin
endif

ifeq ($(origin CFLAGS), environment)
HAVE_CFLAGS := y
else
HAVE_CFLAGS := n
endif

OBJDIR := $(shell uname --machine)
USE_MMX = 0

ifeq ($(OBJDIR), alpha)
  ifneq ($(HAVE_CFLAGS), y)
    CFLAGS := -O4 -arch ev67 -ieee -accept c99_keywords -gcc_messages
  endif
endif

ifneq ($(HAVE_CFLAGS), y)
  #CFLAGS := -O2 -fomit-frame-pointer -I/usr/local/include
  CFLAGS := -O3 -ggdb -fno-omit-frame-pointer -I/usr/local/include \
	-falign-loops=2 -falign-jumps=2 -falign-functions=2
endif

ifeq ($(OBJDIR), i686)
  #USE_MMX = 1
  CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq ($(OBJDIR), x86_64)
  CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq ($(USE_CSS), 1)
  CFLAGS += -DHAVE_CSS
endif

ifeq ($(USE_MMX), 1)
  CFLAGS += -DHAVE_MMX
  ASMOBJS = $(OBJDIR)/video/mmxidct.o
  NASMOBJS = $(OBJDIR)/video/reconmmx.o
endif

CFLAGS += -I.  -I$(A52DIR)/include -I$(A52DIR)/liba52
CFLAGS += -Wall

OBJS = \
	$(OBJDIR)/audio/ac3.o \
	$(OBJDIR)/audio/dct.o \
	$(OBJDIR)/audio/huffman.o \
	$(OBJDIR)/audio/layer2.o \
	$(OBJDIR)/audio/layer3.o \
	$(OBJDIR)/audio/audio.o \
	$(OBJDIR)/audio/pcm.o \
	$(OBJDIR)/audio/synthesizers.o \
	$(OBJDIR)/audio/tables.o \
	$(OBJDIR)/libzmpeg3.o \
	$(OBJDIR)/atrack.o \
	$(OBJDIR)/bits.o \
	$(OBJDIR)/css.o \
	$(OBJDIR)/demux.o \
	$(OBJDIR)/strack.o \
	$(OBJDIR)/title.o \
	$(OBJDIR)/vtrack.o \
	$(OBJDIR)/zio.o \
	$(OBJDIR)/dvb.o \
	$(OBJDIR)/huf.o \
	$(OBJDIR)/xfont.o \
	$(OBJDIR)/video/getpicture.o \
	$(OBJDIR)/video/headers.o \
	$(OBJDIR)/video/idct.o \
	$(OBJDIR)/video/macroblocks.o \
	$(OBJDIR)/video/mmxtest.o \
	$(OBJDIR)/video/motion.o \
	$(OBJDIR)/video/cache.o \
	$(OBJDIR)/video/video.o \
	$(OBJDIR)/video/output.o \
	$(OBJDIR)/video/reconstruct.o \
	$(OBJDIR)/video/seek.o \
	$(OBJDIR)/video/slice.o \
	$(OBJDIR)/video/subtitle.o \
	$(OBJDIR)/video/cc.o \
	$(OBJDIR)/video/vlc.o \
	$(OBJDIR)/ifo.o \
	$(OBJDIR)/toc.o \
	$(OBJDIR)/mpeg3.o \

#OBJS = \
#	$(OBJDIR)/audio/ac3.o \
#	$(OBJDIR)/audio/bit_allocation.o \
#	$(OBJDIR)/audio/exponents.o \
#	$(OBJDIR)/audio/header.o \
#	$(OBJDIR)/audio/huffman.o \
#	$(OBJDIR)/audio/layer2.o \
#	$(OBJDIR)/audio/layer3.o \
#	$(OBJDIR)/audio/mantissa.o \
#	$(OBJDIR)/audio/pcm.o \
#	$(OBJDIR)/audio/tables.o \

DIRS := \
	$(OBJDIR)/audio \
	$(OBJDIR)/video

include Makefile.a52

DIRS += $(A52DIRS)


OUTPUT = $(OBJDIR)/libzmpeg3.a
UTILS := $(OBJDIR)/mpeg3dump
UTILS += $(OBJDIR)/mpeg3peek
UTILS += $(OBJDIR)/mpeg3toc
UTILS += $(OBJDIR)/mpeg3show
UTILS += $(OBJDIR)/mpeg3cat
UTILS += $(OBJDIR)/mpeg31trkpony
UTILS += $(OBJDIR)/mpeg3ifochk
UTILS += $(OBJDIR)/mpeg3cc2txt

#$(OBJDIR)/mpeg3split

LIBS = -lm -lpthread

ifeq ($(DYNAMIC_FONTS), 1)
LIBS += -lX11
else
CFLAGS += -DBUILTIN_FONT_DATA 
OBJS += $(OBJDIR)/xfont1.o
endif

$(shell mkdir -p $(OBJDIR) )

$(shell echo $(CFLAGS) > $(OBJDIR)/c_flags)
$(shell echo $(A52CFLAGS) > $(OBJDIR)/a52_flags)
$(shell echo $(OBJS) $(ASMOBJS) $(A52OBJS) $(NASMOBJS) > $(OBJDIR)/objs)
$(shell mkdir -p $(DIRS) )

TAGS = tags
all: $(OUTPUT) $(UTILS) $(TAGS)
#all: $(OUTPUT) $(TAGS) b.out

xfont1.C:	xfont.C
	$(CXX) `cat $(OBJDIR)/c_flags` -DWRITE_FONT_DATA -o $(OBJDIR)/xfont xfont.C \
		 $(LIBS) -lX11
	$(OBJDIR)/xfont > xfont1.C

b.out:	y.C $(OUTPUT)
	$(CXX) `pkg-config --cflags --libs gtk+-2.0` y.C ./$(OBJDIR)/libzmpeg3.a \
		 -lpthread -lasound -lm -O0 -g -o b.out

$(OUTPUT): $(OBJS) $(ASMOBJS) $(NASMOBJS) $(A52OBJS)
	ar rcs $(OUTPUT) `cat $(OBJDIR)/objs`


mpeg3% $(OBJDIR)/mpeg3%: $(OUTPUT) mpeg3%.C
	$(CXX) `cat $(OBJDIR)/c_flags` -o $(OBJDIR)/$(notdir $@) $(notdir $@).C $(OUTPUT) $(LIBS)


#$(OBJDIR)/mpeg2qt: $(OUTPUT)
#	$(CC) `cat $(OBJDIR)/c_flags` -o $(OBJDIR)/mpeg2qt mpeg2qt.c \
#		$(OUTPUT) $(LIBS) -I.  -I../quicktime \
#		../quicktime/$(OBJDIR)/libquicktime.a \
#		-lpng -lz -ldl

$(TAGS):
	ctags -R .

install: 
	cp $(UTILS) $(DST)

clean:
	rm -rf $(OBJDIR) $(TAGS)

backup: clean
	cd .. && \
	tar -zcf libzmpeg3.tar.gz libmpeg3

wc:
	cat *.c *.h audio/*.c audio/*.h video/*.c video/*.h | wc

$(OBJS): 
	$(CC) -c `cat $(OBJDIR)/c_flags` $(subst $(OBJDIR)/,, $*.C) -o $*.o
$(ASMOBJS): 
	$(CC) -c `cat $(OBJDIR)/c_flags` $(subst $(OBJDIR)/,, $*.S) -o $*.o
$(NASMOBJS): 
	$(NASM) -f elf $(subst $(OBJDIR)/,, $*.s) -o $*.o
$(A52OBJS):
	$(CC) -c `cat $(OBJDIR)/a52_flags` $(subst $(OBJDIR)/,, $*.c) -o $*.o

$(OBJDIR)/libzmpeg3.o: 				    libzmpeg3.C
$(OBJDIR)/atrack.o: 				    atrack.C
$(OBJDIR)/bits.o:  				    bits.C
$(OBJDIR)/css.o: 				    css.C
$(OBJDIR)/demux.o: 				    demux.C
$(OBJDIR)/ifo.o: 				    ifo.C
$(OBJDIR)/zio.o: 				    zio.C
$(OBJDIR)/dvb.o: 				    dvb.C
$(OBJDIR)/huf.o: 				    huf.C
$(OBJDIR)/xfont.o: 				    xfont.C
$(OBJDIR)/xfont1.o: 				    xfont1.C
$(OBJDIR)/strack.o:				    strack.C
$(OBJDIR)/title.o:				    title.C
$(OBJDIR)/mpeg3toc.o: 				    mpeg3toc.C
$(OBJDIR)/mpeg3show.o: 				    mpeg3show.C
$(OBJDIR)/mpeg3dump.o: 				    mpeg3dump.C
$(OBJDIR)/mpeg3cat.o: 				    mpeg3cat.C
$(OBJDIR)/mpeg31trkpony.o: 			    mpeg31trkpony.C
$(OBJDIR)/mpeg3peek.o: 				    mpeg3peek.C
$(OBJDIR)/toc.o:				    toc.C
$(OBJDIR)/mpeg3.o:				    mpeg3.C
$(OBJDIR)/vtrack.o:				    vtrack.C
$(OBJDIR)/audio/ac3.o:  			    audio/ac3.C
$(OBJDIR)/audio/bit_allocation.o: 		    audio/bit_allocation.C
$(OBJDIR)/audio/dct.o:  			    audio/dct.C
#$(OBJDIR)/audio/exponents.o: 			    audio/exponents.c
#$(OBJDIR)/audio/header.o: 			    audio/header.c
$(OBJDIR)/audio/huffman.o: 			    audio/huffman.C
$(OBJDIR)/audio/layer2.o: 			    audio/layer2.C
$(OBJDIR)/audio/layer3.o: 			    audio/layer3.C
#$(OBJDIR)/audio/mantissa.o: 			    audio/mantissa.c
$(OBJDIR)/audio/audio.o: 			    audio/audio.C
$(OBJDIR)/audio/pcm.o:  			    audio/pcm.C
$(OBJDIR)/audio/synthesizers.o: 		    audio/synthesizers.C
$(OBJDIR)/audio/tables.o: 			    audio/tables.C
$(OBJDIR)/video/getpicture.o: 			    video/getpicture.C
$(OBJDIR)/video/headers.o: 			    video/headers.C
$(OBJDIR)/video/idct.o: 			    video/idct.C
$(OBJDIR)/video/macroblocks.o:  		    video/macroblocks.C
$(OBJDIR)/video/mmxtest.o: 			    video/mmxtest.C
$(OBJDIR)/video/motion.o: 			    video/motion.C
$(OBJDIR)/video/cache.o: 			    video/cache.C
$(OBJDIR)/video/video.o: 			    video/video.C
$(OBJDIR)/video/output.o: 			    video/output.C
$(OBJDIR)/video/reconstruct.o:  		    video/reconstruct.C
$(OBJDIR)/video/seek.o: 			    video/seek.C
$(OBJDIR)/video/slice.o: 			    video/slice.C
$(OBJDIR)/video/subtitle.o:                         video/subtitle.C
$(OBJDIR)/video/cc.o:                               video/cc.C
$(OBJDIR)/video/vlc.o:  			    video/vlc.C
#$(OBJDIR)/workarounds.o:  			    workarounds.c

include depend.a52


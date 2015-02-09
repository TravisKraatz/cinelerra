
ifeq ($(origin CFLAGS), environment)
HAVE_CFLAGS := y
else
HAVE_CFLAGS := n
endif

OBJDIR := $(shell uname --machine)
$(shell mkdir -p $(OBJDIR) )

ifneq ($(HAVE_CFLAGS), y)
  #CFLAGS := -O2 -fno-omit-frame-pointer
  CFLAGS := -fno-omit-frame-pointer
endif

ifeq ($(OBJDIR), i686)
  CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

ifeq ($(OBJDIR), x86_64)
  CFLAGS += -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif

CXXFLAGS := -pthread -Wall -ggdb -I.. $(CFLAGS)
LDFLAGS := -pthread
LIBS := $(OBJDIR)/db.a ../libzmpeg3/$(OBJDIR)/libzmpeg3.a -lasound -lm -lX11
OUTPUT := $(OBJDIR)/db.a s.C

all: $(OUTPUT)

.PHONY:	rebuild
rebuild:
	make clean
	make -C utils clean
	make
	+make -C utils

.PHONY:	utils
utils:	$(OBJDIR)/db.a s.C
	+make -C utils

.PHONY:	clean
clean:
	make -C utils clean
	rm -rf a.out s.C tags $(OBJDIR)


OBJECTS := \
	$(OBJDIR)/tdb.o \

$(OBJECTS) $(OBJDIR)/xsch.o $(OBJDIR)/tst.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(*F).C

$(OBJDIR)/db.a:	$(OBJECTS)
	ar rcs $@ $(OBJECTS)
	ctags -R .

s.C:	$(OBJDIR)/xsch sch.txt
	$(OBJDIR)/xsch < sch.txt

.o:
	$(CXX) $(LDFLAGS) -o $* $*.o $(LDLIBS)

#$(OBJDIR)/tdb.o:		tdb.C tdb.h
#	$(CXX) $(CXXFLAGS) -dD -E $(*F).C | grep -v '^# ' > z$(*F).C
#	$(CXX) $(CXXFLAGS) -c -o $@ z$(*F).C

$(OBJDIR)/tdb.o:	tdb.C tdb.h
$(OBJDIR)/xsch.o:	xsch.C tdb.h
$(OBJDIR)/xsch:		$(OBJDIR)/xsch.o


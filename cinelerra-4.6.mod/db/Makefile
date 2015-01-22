
OBJDIR := $(shell uname --machine)
$(shell mkdir -p $(OBJDIR) )

CXXFLAGS := -pthread -Wall -ggdb -I.. $(CFLAGS)
LDFLAGS := -pthread
OUTPUT := $(OBJDIR)/db.a

all: $(OUTPUT)

.PHONY:	rebuild
rebuild:
	make clean
	make

.PHONY:	clean
clean:
	rm -rf $(OBJDIR)


OBJECTS := \
	$(OBJDIR)/s.o \

$(OBJECTS):
	$(CXX) $(CXXFLAGS) -c -o $@ $(*F).C

$(OBJDIR)/db.a:	$(OBJECTS)
	ar rcs $@ $(OBJECTS)


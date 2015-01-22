include ../global_config

OBJS = $(OBJDIR)/mplex.o
CC = gcc
CFLAGS += -I../libzmpeg3

OUTPUT := $(OBJDIR)/mplexlo

$(OUTPUT): $(OBJS)
	$(CXX) -o $(OUTPUT) $(OBJS) ../libzmpeg3/$(OBJDIR)/libzmpeg3.a -lpthread -lm -lX11

$(OBJS):
	$(CXX) -c $(CFLAGS) $(subst $(OBJDIR)/,, $*.c) -o $*.o

clean:
	find \( -name core \
		-o -name '*.o' \
		-o -name '*.a' \
		-o -name '*.so' \) -exec rm -f {} \;
	rm -rf $(OBJDIR)

install:
	cp $(OUTPUT) /usr/bin

$(OBJDIR)/mplex.o: mplex.c

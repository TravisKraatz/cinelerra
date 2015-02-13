include ../../global_config

OBJS = 	$(OBJDIR)/effecttv.o  \

LIBOBJ = $(OBJDIR)/libeffecttv.a

$(LIBOBJ): $(OBJS)
	ar r $@ $^

include ../../plugin_config

$(OBJDIR)/effecttv.o: effecttv.C

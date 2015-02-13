include ../../global_config

OBJS = $(OBJDIR)/dot.o  \
	$(OBJDIR)/dotwindow.o \

OUTPUT = $(PLUGIN_DIR)/dot.plugin
CFLAGS += -I../libeffecttv
LFLAGS += ../libeffecttv/$(OBJDIR)/libeffecttv.a

include ../../plugin_config

$(OBJDIR)/dot.o: dot.C
$(OBJDIR)/dotwindow.o: dotwindow.C




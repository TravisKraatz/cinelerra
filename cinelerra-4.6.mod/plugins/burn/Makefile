#CFLAGS += -g

include ../../global_config

OBJS = $(OBJDIR)/burn.o  \
	$(OBJDIR)/burnwindow.o \

OUTPUT = $(PLUGIN_DIR)/burn.plugin
CFLAGS += -I../libeffecttv
LFLAGS += ../libeffecttv/$(OBJDIR)/libeffecttv.a

include ../../plugin_config

$(OBJDIR)/burn.o: burn.C
$(OBJDIR)/burnwindow.o: burnwindow.C

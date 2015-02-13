
include ../../global_config

OBJS = $(OBJDIR)/aging.o  \
	$(OBJDIR)/agingwindow.o \

OUTPUT = $(PLUGIN_DIR)/aging.plugin
CFLAGS += -I../libeffecttv
LFLAGS += ../libeffecttv/$(OBJDIR)/libeffecttv.a

include ../../plugin_config

$(OBJDIR)/aging.o: aging.C
$(OBJDIR)/agingwindow.o: agingwindow.C

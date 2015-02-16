include ../../global_config

OBJS = $(OBJDIR)/title.o  \
	$(OBJDIR)/titlewindow.o



OUTPUT = $(PLUGIN_DIR)/titler.plugin
CFLAGS += -Wall
#CFLAGS += -I$(FREETYPE_DIR)/include -I../../thirdparty/
#LFLAGS += $(FREETYPE_DIR)/$(OBJDIR)/libfreetype.a
ifneq ($(STATIC_LIBRARIES), y)
LFLAGS += -lfreetype
endif

include ../../plugin_config



$(OBJDIR)/title.o: title.C
$(OBJDIR)/titlewindow.o: titlewindow.C




include ../../global_config

OBJS = $(OBJDIR)/bluedottheme.o
THEME_DATA := $(OBJDIR)/bluedottheme_data.o

OUTPUT_THEME := $(PLUGIN_DIR)/bluedottheme.plugin

CFLAGS += -I../../$(AVC_DIR) -I../../$(LIBRAW_DIR)

$(OUTPUT_THEME): $(OBJS) $(THEME_DATA)
	$(LDLINKER) -o $(OUTPUT_THEME) $(OBJS) $(THEME_DATA)
	$(if $(findstring -g,$(CFLAGS)),,strip $(OUTPUT_THEME))



include ../../plugin_config

$(OBJDIR)/bluedottheme.o: bluedottheme.C

$(THEME_DATA):
	cd $(OBJDIR) && \
	../../../guicast/$(OBJDIR)/bootstrap bluedottheme_data.o ../data/*.png

inst:
	rm -f $(PLUGINDIR)/bluedottheme.plugin
	cp $(OUTPUT_THEME) $(PLUGINDIR)

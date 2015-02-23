include ../../global_config

OBJS = $(OBJDIR)/blondtheme.o
THEME_DATA := $(OBJDIR)/blondtheme_data.o

OUTPUT_THEME := $(PLUGIN_DIR)/blondtheme.plugin

CFLAGS += -I../../$(AVC_DIR) -I../../$(LIBRAW_DIR)

$(OUTPUT_THEME): $(OBJS) $(THEME_DATA)
	$(LDLINKER) -o $(OUTPUT_THEME) $(OBJS) $(THEME_DATA)
	$(if $(findstring -g,$(CFLAGS)),,strip $(OUTPUT_THEME))



include ../../plugin_config

$(OBJDIR)/blondtheme.o: blondtheme.C

$(THEME_DATA):
	cd $(OBJDIR) && \
	../../../guicast/$(OBJDIR)/bootstrap blondtheme_data.o ../data/*.png

inst:
	rm -f $(PLUGINDIR)/blondtheme.plugin
	cp $(OUTPUT_THEME) $(PLUGINDIR)

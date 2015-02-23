include ../../global_config

OBJS = $(OBJDIR)/microtheme.o

OUTPUT_THEME = $(PLUGIN_DIR)/microtheme.plugin

OUTPUT_THEME: $(OBJS)
	$(LDLINKER) -o $(OUTPUT_THEME) $(OBJS)
	../../cinelerra/$(OBJDIR)/bootstrap $(OUTPUT_THEME) data/*.png
	$(if $(findstring -g,$(CFLAGS)),,strip $(OUTPUT_THEME))

include ../../plugin_config

$(OBJDIR)/microtheme.o: microtheme.C



inst:
	rm -f $(PLUGINDIR)/microtheme.plugin
	cp $(OUTPUT_THEME) $(PLUGINDIR)

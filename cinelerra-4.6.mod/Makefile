all:
	$(MAKE) -f build/Makefile.cinelerra

install:
	$(MAKE) -f build/Makefile.cinelerra install

clean:
	$(MAKE) -f build/Makefile.cinelerra clean

rebuild:
	$(MAKE) -C guicast clean
	$(MAKE) -C cinelerra clean
	$(MAKE) -C plugins clean
	$(MAKE) -C guicast
	$(MAKE) -C cinelerra
	$(MAKE) -C plugins

rebuild_all:
	$(MAKE) -C libzmpeg3 clean
	$(MAKE) -C db clean
	$(MAKE) -C libzmpeg3
	$(MAKE) -C db
	$(MAKE) rebuild
	$(MAKE) -C db/utils

rebuild_install:
	$(MAKE) -C cinelerra install
	$(MAKE) -C plugins install

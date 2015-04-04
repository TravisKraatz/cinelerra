# Fixed stuff #

### _this doesn't mean it's perfect, just that it has been reworked_ ###

  * removed cutads soft link and fixed filesystem for broken soft links
  * fixed more libmpeg3->libzmpeg3 references
  * plugged a bunch of memory leaks
  * ??? switched bcdialog BC\_DialogThread to async thread
  * reworked BC\_DisplayInfo
  * cleaned up some dangling threads
  * changed fonts due to broken Sans-xx fontname refs in prefs menus
  * added segv dumper using gdb, needs more work
  * reworked thread.C added dangling thread trap, thread dump, etc...
  * added alsa cache blowout at end of run for valgrind
  * timezone checks in batch.C (cron) and channelinfo (scan)
  * removed refs to strdup, added cstrdup, cstrcat
  * fixed race in auto toc builder thread
  * fixed layer init in filethread
  * fixed unterminated strings in filexml
  * minor rework for mainerror
  * added recipes dropped in merge for cinelerra/Makefile
  * reset panel on load new file replace mode
  * abstracted mwindow delete indecies for rebuild for use
  * fixed undo stack problem using `<alt>-<[1-8]>` for select\_asset
  * minor fixes/rework for pluginclient default xml readin
  * improved thread/buffer cleanup/shutdown in record menu/monitor
  * fixed overlapping memcpy in inverse telecine
  * stubbed css code, media db code
  * fixes to closed captioning and cc to dvd subtitle generator
  * ported mpeg2enc from libmpeg3 to libzmpeg3
  * Ubuntu window restart positions
  * BC\_DisplayInfo mod, check for duplicate probe
  * Add gui for removing undesired plugins from folder listbox
  * create program to rescale and histeq image data for plugin icons
  * create alternate plugin icon data dir for fun customization
  * add gdb dump prefs for segv, ctrl-c, default off
  * centOS build
  * Manual stop of cron recording with auto generate toc hangs
  * zio:562 sync err recovery fails
  * pulldown menu collapes on fedora (done, pushed, testing)
  * create tarball for original plugin icon png files
  * directory missing for index catalog caused segv
  * Manual stop of cron recording with auto generate toc hangs
  * zio:562 sync err recovery fails
  * mpeg3 audio msg: dequantize\_sample: can't rewind stream
  * add font path setup to bin directory and required fonts (dropped)
  * Button up on dismissed menu popup via focus\_out causes segv (avoided)
  * ScrollListBox need\_vscroll height demand calculation wrong
  * autoscale fonts to display geometry
  * add "must be root" to trap sigint/segv prefs
  * improve trap handler logging
  * replacement DB for mediadb
  * TextBox cursor on `<CR>` and mulitiple chars (stutters) on fast backspace
  * CV plugin port
  * CV Icon data
  * CV file changes
  * get rid of findobject plugin
  * change format (set\_format yuv->rgb, etc) with titler segvs
  * add file readin/text clear popup menu and dynamic textbox to titler (dropped)
  * fix mwindow offsets for bluedot and blond themes
  * webcam record window restart problems fixed
  * Manual stop of cron recording with auto generate toc hangs
  * fixed SVG image edit plugin
  * google translate for po construction in the internationalization
  * reworked overlayframe, added porter duff ops et al.
  * several build message cleanups
  * upgraded opengl api for new open interface
  * generalize opengl visual access and context construction for pbuffer, pixmap
  * fixup filebox to use most recent dir for default (done, testing)
  * prevent focus\_out event from deactivating if cursor in menu
  * fix opengl alpha channel setup problems
  * fix textbox problem with formating with seperators
  * tooltip popups on wrong side if tile windows right
  * make mediadb access depend on env var BC\_USE\_COMMECIALS
  * Porter Duff operators for patchbay rendering options
  * add icons for new mode operators in required themes
  * move fader into overlay engine/opengl
  * added overlay CV changes for Lancasos interp type
  * OpenGL direct linkage instead of single threaded strategy. (dropped, wont work)
  * svg plugin segv on access file, missing client->defaults (plugin reworked)
  * Android remote control app
  * yuv422 colormodel mpeg output transcode, filempeg3 updated, encoder fixes
  * turn off enable file fork when debug traps sig\_segv/sig\_int enabled
  * redefined symbol MAX\_INPUT in histogram.inc overlaps limits.h
  * fixup hveg2enc to be multiprocessor safe
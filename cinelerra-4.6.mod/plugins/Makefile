include ../global_config

# burn must come before any other effecttv plugin
# colors must come before any plugin
# parametric must come before fourier plugins
# motion must come before perspective

DIRS = \
	1080to480 \
	1080to540 \
	720to480 \
	aging \
	audioscope \
	bandslide \
	bandwipe \
	blondtheme \
	bluebanana \
	bluedottheme \
	blur \
	brightness \
	brighttheme \
	burn \
	C41 \
	cdripper \
	chromakey \
	chromakeyhsv \
	color3way \
	colorbalance \
	compressor \
	crossfade \
	dcoffset \
	decimate \
	deinterlace \
	deinterlace-cv \
	delayaudio \
	delayvideo \
	denoise \
	denoisefft \
	denoisemjpeg \
	denoiseseltempavg \
	denoisevideo \
	despike \
	diffkey \
	dissolve \
	dot \
	downsample \
	echo \
	echocancel \
	fieldframe \
	findobject \
	flash \
	flip \
	framefield \
	freeverb \
	freezeframe \
	gain \
	gamma \
	gradient \
	graphic \
	histogram \
	holo \
	huesaturation \
	interpolate \
	interpolateaudio \
	interpolatevideo \
	invertaudio \
	invertvideo \
	irissquare \
	ivtc \
	lens \
	level \
	libeffecttv \
	linearblur \
	liveaudio \
	livevideo \
	loopaudio \
	loopvideo \
	motion \
	motion2point \
	motionblur \
	normalize \
	oilpainting \
	overlay \
	overlayaudio \
	parametric \
	perspective \
	photoscale \
	pitch \
	polar \
	radialblur \
	reframe \
	reframert \
	removegaps \
	reroute \
	resample \
	resamplert \
	reverb \
	reverseaudio \
	reversevideo \
	rgb601 \
	rgbshift \
	rotate \
	scale \
	shapewipe \
	sharpen \
	shiftinterlace \
	slide \
	spectrogram \
	suv \
	svg \
	swapchannels \
	swapframes \
	synthesizer \
	threshold \
	timeavg \
	timefront \
	timestretch \
	timestretchrt \
	titler \
	translate \
	unsharp \
	videoscope \
	vocoder \
	wave \
	whirl \
	wipe \
	yuv \
	yuvshift \
	zoom \
	zoomblur \

# Make static table of all plugins
ifeq ($(DO_STATIC), y)

endif


all: $(DIRS)

.PHONY:	force
force:

# force parallel compilation of all directories
$(DIRS): force
	$(MAKE) -C $@

clean:
	find \( -name core \
		-o -name '*.o' \
		-o -name '*.plugin' \) -exec rm -rf {} \;

wc:
	cat *.C *.h | wc
	$(LOOP_BEGIN) \
		 $(MAKE) -C $$i wc; \
	$(LOOP_END)


# Can't strip the themes so instead the plugins strip themselves
install:
#	strip ../bin/*.plugin
#	rm -rf ../bin/*.plugin
#	cp $(OBJDIR)/*.plugin ../bin/
	mkdir -p ../bin/fonts
	cp fonts/* ../bin/fonts
	mkdir -p ../bin/shapes
	cp shapes/* ../bin/shapes
	cp ../thirdparty/mjpegtools*/mpeg2enc/mpeg2enc ../bin/mpeg2enc.plugin

# dependencies for parallel build
aging:		libeffecttv
dot:		libeffecttv
holo:		libeffecttv
motion2point:	downsample motion
motion:		downsample
timestretchrt:	timestretch


#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile,v 1.6 2005/02/24 22:35:50 lucke Exp $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
#
PLUGIN = softdevice

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The C++ compiler and options:

# uncomment ONE of these lines if you want to enable the support

###############################
# don't forget to "make clean" if you change something
###############################

#XV support by Stefan Lucke
XV_SUPPORT=1

# DFB is very experimental (works only with my matrox G200, comments are welcome)
DFB_SUPPORT=1

# FB is unaccellerated and should work with any FB with 16bit color depth
FB_SUPPORT=1

# Vidix support by Vadim Catana
VIDIX_SUPPORT=1

# Set this if you want to use DPMS
LIBXDPMS_SUPPORT = 1

# Set this if you want to be able to toggle suspend mode by keyboard (XV only)
SUSPEND_BY_KEY = 1

# Enable the usage from some deinterlacing pp-filters of libavcodec
# ffmpeg must be compiled with the options --enable-pp --enable-gpl
# to use this feature !
# Then you have to install the header files with 'make install' in
# the directory ffmpeg/libavcodec/libpostproc .
# I case you build static libs only (default) you have to copy them manuell
# to your target directories (example with /usr/local [default] as target):
#
#   mkdir -p /usr/local/include/postproc
#   install -m 644 postprocess.h /usr/local/include/postproc/postprocess.h
#   cp libpostproc.a /usr/local/lib/
#
PP_LIBAVCODEC=1

# -----------------------------------------------------------------------------
# if you want output methods build as separate libs uncomment this
#USE_SUBPLUGINS = 1

# -----------------------------------------------------------------------------
# framebuffer device name
#
#FBDEV = /dev/fb/0
#FBDEV = /dev/fb0

FBDEV = /dev/fb0

# -----------------------------------------------------------------------------
# VIDIX specific definitions
# installation directory
#
#VIDIX_DIR    = /opt/vidix
#VIDIX_DIR    = /usr/local/

VIDIX_DIR    = /usr/local

# -----------------------------------------------------------------------------
# Set up these paths to your enviroment!
#
#LIBAVCODEC=/opt/ffmpeg
#LIBAVCODEC = ../../../../ffmpeg/libavcodec
#
# If you installed ffmpeg's lib (make installlib)
LIBAVCODEC=/usr/local/include/ffmpeg/
#LIBAVCODEC= /home/extra/src/video/ffmpeg-0.4.8/libavcodec
#
#
# DON'T touch this:
FFMPEGLIBS = -lavcodec
#
# -----------------------------------------------------------------------------
# Depending on your ffmpeg lib you may need some additional libs
# uncomment one of the following when you'll get unknow symbol messages:
#
# .. undefined symbol: lame_encode_buffer_interleaved
#FFMPEGLIBS += -lmp3lame

#
# .. undefined symbol: vorbis_encode_init
#FFMPEGLIBS += -lvorbisenc -lvorbis

# -----------------------------------------------------------------------------
#
#DVBDIR 	   = ../../../../dvb-kernel
#DVBDIR 	   = ../../../../DVB

DVBDIR 	   = ../../../../DVB

VDRDIR     = ../../..
LIBDIR     = ../../lib
TMPDIR     = /tmp

#tuning options

#MMX is needed for FB-output!
DEFINES	   += -DUSE_MMX

# uncomment this line if you do not have MMX2
DEFINES	   += -DUSE_MMX2


### normally you shoudn't have to touch something below this line

CXX      ?= g++
CXXFLAGS ?= -O2 -Wall -Woverloaded-virtual -L$(LIBAVCODEC)

### The directory environment:


### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR (taken from VDR's "config.h"):

VDRVERSION = $(shell grep 'define VDRVERSION ' $(VDRDIR)/config.h | awk '{ print $$3 }' | sed -e 's/"//g')

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include -I$(DVBDIR)/include -I$(LIBAVCODEC)

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"' -D_GNU_SOURCE

### The object files (add further files here):

ifdef USE_SUBPLUGINS
  DEFINES += -DUSE_SUBPLUGINS
endif

TARGETS = libvdr-$(PLUGIN).so
LIBS = $(FFMPEGLIBS) -lasound -lz
OBJS = $(PLUGIN).o utils.o i18n.o video.o mpeg2decoder.o audio.o video-dummy.o setup-softdevice.o
ALL_OBJS = $(OBJS)

ifdef DFB_SUPPORT
  ifdef USE_SUBPLUGINS
    DFB_LIBS  = -L/usr/local/lib -ldfb++
    DFB_OBJS  = video.o video-dfb.o
    ALL_OBJS += $(DFB_OBJS)
    TARGETS  += libvdr-$(PLUGIN)-dfb.so
  else
    OBJS     += video-dfb.o
    LIBS     += -L/usr/local/lib -ldfb++
  endif
  DEFINES  += -DDFB_SUPPORT
  INCLUDES += -I/usr/local/include/dfb++ -I/usr/local/include/directfb
endif

ifdef FB_SUPPORT
  ifdef USE_SUBPLUGINS
    FB_LIBS   =
    FB_OBJS   = video.o video-fb.o
    ALL_OBJS  += $(FB_OBJS)
    TARGETS   += libvdr-$(PLUGIN)-fb.so
  else
    OBJS 	+= video-fb.o
  endif
  DEFINES += -DFB_SUPPORT -DFBDEV=\"$(FBDEV)\"
endif

ifdef XV_SUPPORT
  ifdef USE_SUBPLUGINS
    XV_LIBS   = -L/usr/X11R6/lib -lXi -lXext -lX11 -lm -lXv
    XV_OBJS   = video.o video-xv.o xscreensaver.o
    ALL_OBJS  += $(XV_OBJS)
    TARGETS   += libvdr-$(PLUGIN)-xv.so
  else
    OBJS 	+= video-xv.o xscreensaver.o
    LIBS 	+= -L/usr/X11R6/lib -lXi -lXext -lX11 -lm -lXv
  endif
  ifdef LIBXDPMS_SUPPORT
    DEFINES += -DLIBXDPMS_SUPPORT
  endif
  DEFINES += -DXV_SUPPORT
endif

ifdef VIDIX_SUPPORT
  ifdef USE_SUBPLUGINS
    VIDIX_OBJS  = video.o video-vidix.o
    VIDIX_LIBS  = -lvidix
    ALL_OBJS    += $(VIDIX_OBJS)
    TARGETS     += libvdr-$(PLUGIN)-vidix.so
  else
    OBJS        += video-vidix.o
    LIBS        += -lvidix
  endif
  CXXFLAGS    += -L$(VIDIX_DIR)/lib
  INCLUDES    += -I$(VIDIX_DIR)/include/vidix
  DEFINES     += -DVIDIX_SUPPORT -DVIDIX_DIR=\"$(VIDIX_DIR)/lib/vidix/\" -DFBDEV=\"$(FBDEV)\"
endif

ifdef PP_LIBAVCODEC
  DEFINES     += -DPP_LIBAVCODEC
  FFMPEGLIBS  += -lpostproc
endif

ifdef SUSPEND_BY_KEY
  DEFINES     += -DSUSPEND_BY_KEY
endif

### Implicit rules:


%.o: %.c
	$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) $<

# Dependencies:

MAKEDEP = g++ -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Targets:

all: $(TARGETS)

libvdr-$(PLUGIN).so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) $(LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

ifdef USE_SUBPLUGINS

libvdr-$(PLUGIN)-dfb.so: $(DFB_OBJS)
	$(CXX) $(CXXFLAGS) -shared $(DFB_OBJS) $(DFB_LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

libvdr-$(PLUGIN)-fb.so: $(FB_OBJS)
	$(CXX) $(CXXFLAGS) -shared $(FB_OBJS) $(FB_LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

libvdr-$(PLUGIN)-xv.so: $(XV_OBJS)
	$(CXX) $(CXXFLAGS) -shared $(XV_OBJS) $(XV_LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

libvdr-$(PLUGIN)-vidix.so: $(VIDIX_OBJS)
	$(CXX) $(CXXFLAGS) -shared $(VIDIX_OBJS) $(VIDIX_LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

endif

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~

#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile,v 1.1 2004/08/01 05:07:03 lucke Exp $

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

OBJS = $(PLUGIN).o utils.o i18n.o video.o mpeg2decoder.o audio.o video-dummy.o setup-softdevice.o
LIBS = $(FFMPEGLIBS) -lasound -lz

ifdef DFB_SUPPORT
OBJS     += video-dfb.o
DEFINES  += -DDFB_SUPPORT
INCLUDES += -I/usr/local/include/dfb++ -I/usr/local/include/directfb
LIBS 	 += -L/usr/local/lib -ldfb++
#INCLUDES += -I/opt/directfb/include/directfb -I/opt/directfb/include
#LIBS 	 += -L/opt/directfb/lib -ldfb++
endif

ifdef FB_SUPPORT
OBJS 	+= video-fb.o
DEFINES += -DFB_SUPPORT -DFBDEV=\"$(FBDEV)\"
endif

ifdef XV_SUPPORT
OBJS 	+= video-xv.o
DEFINES += -DXV_SUPPORT
LIBS 	+= -L/usr/X11R6/lib -lXi -lXext -lX11 -lm -lXv
endif

ifdef VIDIX_SUPPORT
OBJS        += video-vidix.o
CXXFLAGS    += -L$(VIDIX_DIR)/lib
INCLUDES    += -I$(VIDIX_DIR)/include/vidix
DEFINES     += -DVIDIX_SUPPORT -DVIDIX_DIR=\"$(VIDIX_DIR)/lib/vidix/\" -DFBDEV=\"$(FBDEV)\"
LIBS        += -lvidix
endif

ifdef PP_LIBAVCODEC
DEFINES     += -DPP_LIBAVCODEC
FFMPEGLIBS  += -lpostproc
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

all: libvdr-$(PLUGIN).so


libvdr-$(PLUGIN).so: $(OBJS)
	$(CXX) $(CXXFLAGS) -shared $(OBJS) $(LIBS) -o $@
	@cp $@ $(LIBDIR)/$@.$(VDRVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~

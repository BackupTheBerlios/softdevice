#
# Makefile for a Video Disk Recorder plugin
#
# $Id: Makefile,v 1.9 2007/03/17 08:49:24 lucke Exp $

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.
#
PLUGIN = softplay

#LIBFFMPEG=/home/martin/vdr/ffmpeg
#LIBFFMPEG=/home/martin/vdr/ffmpeg-0.4.9-pre1
LIBFFMPEG=/usr/local/include/ffmpeg/

### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')

### The C++ compiler and options:

CXX      ?= g++
CXXFLAGS ?= -g -O2 -fPIC -Wall -Woverloaded-virtual  

### The directory environment:

DVBDIR = ../../../../DVB
VDRDIR = ../../..
LIBDIR = ../../lib
TMPDIR = /tmp

### Allow user defined options to overwrite defaults:

-include $(VDRDIR)/Make.config

### The version number of VDR (taken from VDR's "config.h"):

VDRVERSION = $(shell grep 'define VDRVERSION ' $(VDRDIR)/config.h | awk '{ print $$3 }' | sed -e 's/"//g')
APIVERSION = $(shell sed -ne '/define APIVERSION/ { s/^.*"\(.*\)".*$$/\1/; p }' $(VDRDIR)/config.h)

ifeq ($(APIVERSION),)
	APIVERSION = $(VDRVERSION)
endif


### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### Includes and Defines (add further entries here):

INCLUDES += -I$(VDRDIR)/include -I$(DVBDIR)/include -I$(LIBFFMPEG) -I$(LIBFFMPEG)/libavformat -I$(LIBFFMPEG)/libavcodec -I$(LIBFFMPEG)/libavutil 

DEFINES += -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -DPLUGIN_NAME_I18N='"$(PLUGIN)"'


### The object files (add further files here):

OBJS = $(PLUGIN).o SoftPlayer.o PlayList.o PlayListMenu.o i18n.o FileIndex.o Receiver.o Setup.o

LIBS = -L$(LIBFFMPEG) -L$(LIBFFMPEG)/libavformat -L$(LIBFFMPEG)/libavcodec -L$(LIBFFMPEG)/libavutil -lavformat -lavcodec -lz

### recent ffmpegs require -lavutil: 

LIBS += $(shell $(CXX) $(CXXFLAGS) $(INCLUDES) getFFmpegLibs.c -o getFFmpegLibs ; ./getFFmpegLibs)

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
	@cp $@ $(LIBDIR)/$@.$(APIVERSION)

dist: clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar czf $(PACKAGE).tgz -C $(TMPDIR) --exclude CVS $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tgz

clean:
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tgz core* *~

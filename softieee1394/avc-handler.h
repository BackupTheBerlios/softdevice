/*
 * avc-handler.h: handle optionally present AVC (firewire) devices.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: avc-handler.h,v 1.1 2005/05/01 22:05:08 lucke Exp $
 */

#ifndef AVC_HANDLER_H
#define AVC_HANDLER_H

#include <vdr/plugin.h>
#include <vdr/player.h>

#if VDRVERSNUM >= 10307
#include <vdr/osd.h>
#endif

#include <libavc1394/avc1394.h>
#include <libavc1394/avc1394_vcr.h>
#include <libavc1394/rom1394.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>

#define MAX_IEEE1394_DEVICES  64

/* ---------------------------------------------------------------------------
 */
class cAVCHandler: public cThread {
private:
    int               numDevices,
                      generation;
    octlet_t          deviceId[MAX_IEEE1394_DEVICES];
    bool              active;
    rom1394_directory romDirs[MAX_IEEE1394_DEVICES];
    raw1394handle_t   handle;
    cMutex            avcMutex;
    static int        busResetHandler (raw1394handle_t handle,
                                       unsigned int generation);
    void              checkDevices(void);

protected:

public:
    cAVCHandler();
    virtual ~cAVCHandler();

    octlet_t        GetGuidForName(const char *name);
    const char      *GetDeviceNameAt(int i);
    virtual void    Action(void);

};

/* ---------------------------------------------------------------------------
 */
class cAVCPlayer: public cPlayer, cThread {
private:
  int                 pollTimeouts,
                      deviceNum;
  bool                paused,
                      winding,
                      playing,
                      deviceOK;

  raw1394handle_t     handle;
  octlet_t            deviceId;

  ePlayMode           softPlayMode;

  AVFormatContext     *ic;
  AVInputFormat       *fmt;
  AVFormatParameters  *ap;


protected:
  virtual void Activate(bool On);
  virtual void Action(void);

public:
  bool  active,
        running;
  cAVCPlayer(octlet_t deviceId);
  virtual ~cAVCPlayer();
  void Command(eKeys key);
  void Stop();
};

/* ---------------------------------------------------------------------------
 */
class cAVCControl : public cControl {
private:
  cAVCPlayer *player;
  
public:
  cAVCControl(octlet_t deviceId);
  virtual ~cAVCControl();
  virtual void Hide(void);
  virtual eOSState ProcessKey(eKeys key);
};

#endif // AVC_HANDLER_H

/*
 * avc-handler.h: handle optionally present AVC (firewire) devices.
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: avc-handler.h,v 1.3 2006/05/26 19:59:21 lucke Exp $
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
 * MIC helper definitions START
 */
#define MAX_MIC_SIZE      (64*1024)
#define MIC_HEADER_SIZE   16
#define MIC_ADDINFO_SIZE  10
#define MIC_ENTRY_SIZE    10

#define MIC_TAG_ATN       0x0B
#define MIC_TAG_TAPENAME  0x18
#define MIC_TAG_TAPEEOM   0x1F
#define MIC_TAG_DATETIME  0x42

#define MIC_TAG_INFO_DT         0x00
#define MIC_TAG_INFO_TAPE_NAME  0x06
#define MIC_TAG_INFO_NONE       0x07

typedef struct {
  int atn;
  int year,
      month,
      day;
  int hour,
      minute;
} t_mic_jump_mark;

typedef struct {
  int             uatn;
  int             eom_atn;
  int             num_marks;
  char            tape_name[256];
  t_mic_jump_mark jump_mark[(MAX_MIC_SIZE-(MIC_HEADER_SIZE+MIC_ADDINFO_SIZE)) /
                            MIC_ENTRY_SIZE];
} t_mic_directory;

/* ---------------------------------------------------------------------------
 * MIC helper definitions END
 */

/* ---------------------------------------------------------------------------
 */
class cAVCHandler: public cThread {
private:
    int               numDevices,
                      generation;
    octlet_t          deviceId[MAX_IEEE1394_DEVICES];
    bool              active;
    int               micSize[MAX_IEEE1394_DEVICES];
    unsigned char     *micData[MAX_IEEE1394_DEVICES];
    t_mic_directory   *micDirectories[MAX_IEEE1394_DEVICES];
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
    const char      *GetDeviceNameAt(int i),
                    *TapePosition(int i, char *pos);
    int             NumDevices(void);
    bool            HasMicInfo(int i);
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

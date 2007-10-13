/*
 * setup-softdevice-menu.c
 *
 * See the README file for copyright information and how to reach the authors.
 *
 * $Id: setup-softdevice-menu.c,v 1.14 2007/10/13 13:38:45 lucke Exp $
 */

//#include "video.h"
#include "setup-softdevice-menu.h"
#include "setup-softlog-menu.h"

/* ---------------------------------------------------------------------------
 */
cMenuSetupVideoParm::cMenuSetupVideoParm(const char *name) : cOsdMenu(name, 33)
{
  copyData = *setupStore;
  data = setupStore;

  if (data->vidCaps & CAP_BRIGHTNESS)
  {
    Add(new cMenuEditIntItem(tr("Brightness"),
                             &data->vidBrightness,
                             0,
                             VID_MAX_PARM_VALUE));
  }
  if (data->vidCaps & CAP_CONTRAST)
  {
    Add(new cMenuEditIntItem(tr("Contrast"),
                             &data->vidContrast,
                             0,
                             VID_MAX_PARM_VALUE));
  }
  if (data->vidCaps & CAP_HUE)
  {
    Add(new cMenuEditIntItem(tr("Hue"),
                             &data->vidHue,
                             0,
                             VID_MAX_PARM_VALUE));
  }
  if (data->vidCaps & CAP_SATURATION)
  {
    Add(new cMenuEditIntItem(tr("Saturation"),
                             &data->vidSaturation,
                             0,
                             VID_MAX_PARM_VALUE));
  }
  if (data->vidCaps & CAP_HWDEINTERLACE)
  {
    Add(new cMenuEditIntItem(tr("HW-Deinterlace"),
                             &data->vidDeinterlace,
                             0,
                             10));
  }
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupVideoParm::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      *setupStore = copyData;
      fprintf (stderr, "[setup-videoparm] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupCropping::cMenuSetupCropping(const char *name) : cOsdMenu(name, 33)
{
  copyData = *setupStore;
  data = setupStore;

  crop_str[0] = tr("none");
  Add(new cMenuEditStraItem(tr("CropMode"),
                            &data->cropMode,
                            SETUP_CROPMODES,
                            crop_str));

  userKeyUsage[0] = tr("none");
  Add(new cMenuEditStraItem(tr("CropModeToggleKey"),
                            &data->cropModeToggleKey,
                            (SETUP_USERKEYS-1),
                            userKeyUsage));

  Add(new cMenuEditBoolItem(tr("Autodetect Movie Aspect"),
                            &data->autodetectAspect, tr("no"), tr("yes")));

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif
  Add(new cMenuEditIntItem(tr("Zoom factor"),
                           &data->zoomFactor,
                           0,
                           128));
  Add(new cMenuEditIntItem(tr("Zoom area shift (left/right)"),
                           &data->zoomCenterX,
                           -100,
                           100));
  Add(new cMenuEditIntItem(tr("Zoom area shift (up/down)"),
                           &data->zoomCenterY,
                           -100,
                           100));

  if (data->outputMethod != VOUT_FB)
  {
#if VDRVERSNUM >= 10334
    Add(new cOsdItem(" ", osUnknown, false));
#else
    Add(new cOsdItem(" ", osUnknown));
#endif

    Add(new cMenuEditIntItem(tr("Crop lines from top"),
                             &data->cropTopLines,
                             0,
                             MAX_CROP_LINES));

    Add(new cMenuEditIntItem(tr("Crop lines from bottom"),
                             &data->cropBottomLines,
                             0,
                             MAX_CROP_LINES));
  }

  if (data->outputMethod == VOUT_XV || data->outputMethod == VOUT_DFB)
  {
    Add(new cMenuEditIntItem(tr("Crop columns from left"),
                             &data->cropLeftCols,
                             0,
                             MAX_CROP_COLS));
    Add(new cMenuEditIntItem(tr("Crop columns from right"),
                             &data->cropRightCols,
                             0,
                             MAX_CROP_COLS));
  }

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  Add(new cMenuEditIntItem(tr("Expand top/bottom lines"),
                           &data->expandTopBottomLines,
                           0,
                           MAX_CROP_LINES/2));
  Add(new cMenuEditIntItem(tr("Expand left/right columns"),
                           &data->expandLeftRightCols,
                           0,
                           MAX_CROP_COLS/2));
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupCropping::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      *setupStore = copyData;
      fprintf (stderr, "[setup-cropping] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupPostproc::cMenuSetupPostproc(const char *name) : cOsdMenu(name, 33)
{
  copyData = *setupStore;
  data = setupStore;

  deint_str[0] = tr("none");
  if (data->outputMethod == VOUT_FB)
  {
    Add(new cMenuEditStraItem(tr("Deinterlace Method"),
                              &data->deintMethod,
#ifdef PP_LIBAVCODEC
                              8,
#else
                              3,
#endif //PP_LIBAVCODEC
                              deint_str));
  }
  else
  {
    Add(new cMenuEditStraItem(tr("Deinterlace Method"),
                              &data->deintMethod,
#ifdef PP_LIBAVCODEC
                              7,
#else
                              2,
#endif //PP_LIBAVCODEC
                              deint_str));
  }

#ifdef PP_LIBAVCODEC
  pp_str[0] = tr("none");
  pp_str[1] = tr("fast");
  pp_str[2] = tr("default");
  Add(new cMenuEditStraItem(tr("Postprocessing Method"),
                              &data->ppMethod,(SETUP_PPMODES-1),pp_str));
  Add(new cMenuEditIntItem(tr("Postprocessing Quality"),
                              &data->ppQuality,0,6));
#endif

  Add(new cMenuEditBoolItem(tr("Picture mirroring"),
                            &data->mirror, tr("off"), tr("on")));

}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupPostproc::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      *setupStore = copyData;
      fprintf (stderr, "[setup-postproc] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
cMenuSetupSoftdevice::cMenuSetupSoftdevice(cPlugin *plugin)
{
  if (plugin)
    SetPlugin(plugin);
  this->plugin = plugin;

  copyData = *setupStore;
  data = setupStore;

  Add(new cOsdItem(tr("Cropping")));
  Add(new cOsdItem(tr("Post processing")));

  if (data->vidCaps)
  {
    Add(new cOsdItem(tr("Video out")));
  }

  Add(new cOsdItem(tr("Logging")));

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  if (data->outputMethod == VOUT_XV ||
      data->outputMethod == VOUT_SHM)
  {
    xv_startup_aspect[0] = tr("16:9 wide");
    xv_startup_aspect[1] = tr("4:3 normal");
    Add(new cMenuEditStraItem(tr("Xv startup aspect"),
                             &data->xvAspect,
                             (SETUP_XVSTARTUPASPECT-1),
                             xv_startup_aspect));
  }

  videoAspectNames[0] = tr("default");
  Add(new cMenuEditStraItem(tr("Screen Aspect"),
                            &data->screenPixelAspect,
                            SETUP_VIDEOASPECTNAMES_COUNT,
                            videoAspectNames));

  if (data->outputMethod == VOUT_XV || data->outputMethod == VOUT_VIDIX
      || data->outputMethod == VOUT_SHM )
  {
    osdModeNames[0] = tr("pseudo");
    osdModeNames[1] = tr("software");
    Add(new cMenuEditStraItem(tr("OSD alpha blending"),
                              &data->osdMode,
                              (SETUP_OSDMODES-1),
                              osdModeNames));
  }

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  bufferModes[0] = tr("safe");
  bufferModes[1] = tr("good seeking");
  bufferModes[2] = tr("HDTV");
  Add(new cMenuEditStraItem(tr("Buffer Mode"),
                              &data->bufferMode,(SETUP_BUFFERMODES-1),bufferModes));

  suspendVideo[0] = tr("playing");
  suspendVideo[1] = tr("suspended");
  Add(new cMenuEditStraItem(tr("Playback"),
                            &data->shouldSuspend,
                            (SETUP_SUSPENDVIDEO-1),
                            suspendVideo));

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  Add(new cMenuEditIntItem(tr("A/V Delay"),
                           &data->avOffset,
                           MINAVOFFSET, MAXAVOFFSET));

  Add(new cMenuEditStraItem(tr("AC3 Mode"),
                            &data->ac3Mode,
                            (SETUP_AC3MODENAMES-1),
                            ac3ModeNames));

  Add(new cMenuEditStraItem(tr("Sync Mode"),
                            &data->syncTimerMode,
                            (SETUP_SYNC_TIMER_NAMES-1),
                            syncTimerNames));

#if VDRVERSNUM >= 10334
  Add(new cOsdItem(" ", osUnknown, false));
#else
  Add(new cOsdItem(" ", osUnknown));
#endif

  if ((data->outputMethod == VOUT_DFB || data->outputMethod == VOUT_VIDIX
       || data->outputMethod == VOUT_XV ) &&
      !data->pixelFormatLocked)
  {
    Add(new cMenuEditStraItem(tr("Pixel Format"),
                              &data->pixelFormat,
                              (SETUP_PIXFMT-1),
                              pix_fmt));
  }

  if (data->outputMethod == VOUT_DFB) {
    if (!data->stretchBlitLocked)
      Add(new cMenuEditBoolItem(tr("Use StretchBlit"),
                                &data->useStretchBlit,
                                tr("off"), tr("on")));

    if (!data->setSourceRectangleLocked)
      Add(new cMenuEditBoolItem(tr("Use SourceRectangle"),
                                &data->useSetSourceRectangle,
                                tr("off"), tr("on")));
  }

  Add(new cMenuEditBoolItem(tr("Hide main menu entry"),
                            &data->mainMenu, tr("yes"), tr("no")));
}

/* ---------------------------------------------------------------------------
 */
eOSState cMenuSetupSoftdevice::ProcessKey(eKeys Key)
{
    eOSState state = cOsdMenu::ProcessKey(Key);

  switch (state)
  {
    case osUnknown:
      switch (Key)
      {
        case kOk:
          if (!strcmp(Get(Current())->Text(),tr("Cropping")))
          {
            return AddSubMenu (new cMenuSetupCropping(tr("Cropping")));
          }
          else if (!strcmp(Get(Current())->Text(),tr("Post processing")))
          {
            return AddSubMenu (new cMenuSetupPostproc(tr("Post processing")));
          }
          else if (!strcmp(Get(Current())->Text(),tr("Video out")))
          {
            return AddSubMenu (new cMenuSetupVideoParm(tr("Video out")));
          }
          else if (!strcmp(Get(Current())->Text(),tr("Logging")))
          {
            return AddSubMenu (new cMenuSetupSoftlog(plugin, tr("Logging")));
          }
          Store();
          state = osBack;
          break;
        default:
          break;
      }
      break;
    case osBack:
      *setupStore = copyData;
      fprintf (stderr, "[setup-softdevice] restoring setup state\n");
      break;
    default:
      break;
  }
  return state;
}

/* ---------------------------------------------------------------------------
 */
void cMenuSetupSoftdevice::Store(void)
{
#if 0
  if (setupStore->deintMethod != data.deintMethod) {
    fprintf(stderr,
            "[setup-softdevice] deinterlace method changed to (%d) %s\n",
            data.deintMethod, deint_str [data.deintMethod]);
  }
#endif


  fprintf (stderr, "[setup-softdevice] storing data\n");
//  setupStore = data;
  SetupStore ("Xv-Aspect",          setupStore->xvAspect);
  // don't save max area value as it is ignored on load
  //SetupStore ("Xv-MaxArea",         setupStore->xvMaxArea);
  SetupStore ("CropMode",           setupStore->cropMode);
  SetupStore ("CropModeToggleKey",     setupStore->cropModeToggleKey);
  SetupStore ("CropTopLines",        setupStore->cropTopLines);
  SetupStore ("CropBottomLines",     setupStore->cropBottomLines);
  SetupStore ("CropLeftCols",        setupStore->cropLeftCols);
  SetupStore ("CropRightCols",       setupStore->cropRightCols);
  SetupStore ("Deinterlace Method", setupStore->deintMethod);
  SetupStore ("Postprocess Method", setupStore->ppMethod);
  SetupStore ("Postprocess Quality", setupStore->ppQuality);
  SetupStore ("PixelFormat",        setupStore->pixelFormat);
  SetupStore ("UseStretchBlit",     setupStore->useStretchBlit);
  SetupStore ("UseSetSourceRectangle",     setupStore->useSetSourceRectangle);
  SetupStore ("Picture mirroring",  setupStore->mirror);
  SetupStore ("avOffset",           setupStore->avOffset);
  SetupStore ("AlsaDevice",         setupStore->alsaDevice);
  SetupStore ("AlsaAC3Device",      setupStore->alsaAC3Device);
  SetupStore ("PixelAspect",        setupStore->screenPixelAspect);
  SetupStore ("Suspend",            setupStore->shouldSuspend);
  SetupStore ("OSDalphablend",      setupStore->osdMode);
  SetupStore ("AC3Mode",            setupStore->ac3Mode);
  SetupStore ("bufferMode",           setupStore->bufferMode);
  SetupStore ("mainMenu",             setupStore->mainMenu);
  SetupStore ("syncTimerMode",        setupStore->syncTimerMode);
  SetupStore ("vidBrightness",        setupStore->vidBrightness);
  SetupStore ("vidContrast",          setupStore->vidContrast);
  SetupStore ("vidHue",               setupStore->vidHue);
  SetupStore ("vidSaturation",        setupStore->vidSaturation);
  SetupStore ("ExpandTopBottomLines", setupStore->expandTopBottomLines);
  SetupStore ("ExpandLeftRightCols",  setupStore->expandLeftRightCols);
  SetupStore ("autodetectAspect",     setupStore->autodetectAspect);
}

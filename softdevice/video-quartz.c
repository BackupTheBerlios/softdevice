/*
 *
 * video-quartz.c: A plugin for the Video Disk Recorder
 *
 * Copyright (c) Martin Wache 2006
 *
 * 
 * This file is based on MPlayers vo_quartz with the original copyright notice:
 */

/*
	vo_quartz.c
	
	by Nicolas Plourde <nicolasplourde@gmail.com>
	
	Copyright (c) Nicolas Plourde - April 2004

	YUV support Copyright (C) 2004 Romain Dolbeau <romain@dolbeau.org>
	
	MPlayer Mac OSX Quartz video out module.
	
*/

/*
 *
 * most of the OpenGL code is based on vlc's opengl.c with the copyright notice:
 *
 * Copyright (C) 2004 the VideoLAN team
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
 *
 */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "video-quartz.h"

namespace MacOs {
#include <Carbon/Carbon.h>
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <AGL/agl.h>
};

#include <stdint.h>
//#include <altivec.h>

#include "utils.h"

using namespace MacOs;


//#define QDEB(out...) {printf("[vout-quartz] "); printf(out);}

#ifndef QDEB
#define QDEB(out...)
#endif

cQuartzVideoOut *vout=NULL;
cSoftRemote *quartzRemote=NULL;


int levelList[] =
{
    kCGDesktopWindowLevelKey,
    kCGNormalWindowLevelKey,
    kCGScreenSaverWindowLevelKey
};

enum
{
	kQuitCmd			= 1,
	kHalfScreenCmd		= 2,
	kNormalScreenCmd	= 3,
	kDoubleScreenCmd	= 4,
	kFullScreenCmd		= 5,
	kKeepAspectCmd		= 6,
	kAspectOrgCmd		= 7,
	kAspectFullCmd		= 8,
	kAspectWideCmd		= 9,
	kPanScanCmd		= 10
};


//default mouse event handler
static OSStatus DefaultMouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData) {
return vout->MouseEventHandler(nextHandler,event,userData);
};

OSStatus cQuartzVideoOut::MouseEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
        OSStatus result = noErr;
	uint32_t evtClass = GetEventClass (event);
	uint32_t kind = GetEventKind (event); 

        //QDEB("MouseEventHandler  %d\n",result);

	result = CallNextEventHandler(nextHandler, event);
	
	if(evtClass == kEventClassMouse)
	{
		WindowPtr tmpWin;
		Point mousePos;
		Point winMousePos;

		GetEventParameter(event, kEventParamMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &mousePos);
		GetEventParameter(event, kEventParamWindowMouseLocation, typeQDPoint, 0, sizeof(Point), 0, &winMousePos);

		switch (kind)
		{
			case kEventMouseMoved:
			{
		//		if(vo_quartz_fs)
				{
					CGDisplayShowCursor(kCGDirectMainDisplay);
		//			mouseHide = FALSE;
				}
			}
			break;
		/*	
			case kEventMouseWheelMoved:
			{
				int wheel;
				short part;
				
				GetEventParameter(event, kEventParamMouseWheelDelta, typeSInt32, 0, sizeof(int), 0, &wheel);

				part = FindWindow(mousePos,&tmpWin);
				
				if(part == inContent)
				{
					if(wheel > 0)
						mplayer_put_key(MOUSE_BTN3);
					else
						mplayer_put_key(MOUSE_BTN4);
				}
			}
			break;
	*/		
			case kEventMouseDown:
			{
				EventMouseButton button;
				short part;
				Rect bounds;
				
				GetWindowPortBounds(theWindow, &bounds);
				GetEventParameter(event, kEventParamMouseButton, typeMouseButton, 0, sizeof(EventMouseButton), 0, &button);
				
				part = FindWindow(mousePos,&tmpWin);
				
				if( (winMousePos.h > (bounds.right - 15)) && (winMousePos.v > (bounds.bottom)) )
				{
	//				if(!vo_quartz_fs)
					{
                                                QDEB("GrowWindow\n");
						GrowWindow(theWindow, mousePos, NULL);
					}
				}
				else if(part == inMenuBar)
				{
					MenuSelect(mousePos);
					HiliteMenu(0);
				}
				else if(part == inContent)
				{
					switch(button)
					{ /*
						case 1: mplayer_put_key(MOUSE_BTN0);break;
						case 2: mplayer_put_key(MOUSE_BTN2);break;
						case 3: mplayer_put_key(MOUSE_BTN1);break;
				*/
						default:result = eventNotHandledErr;break;
					}
				}
			}		
			break;
			
			case kEventMouseUp:
			break;
			
			case kEventMouseDragged:
			break;
				
			default:result = eventNotHandledErr;break;
		}
	}

        //QDEB("MouseEve end %d\n",result);
    return result;
}


//default keyboard event handler
static OSStatus KeyEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
        OSStatus result = noErr;
	uint32_t evtClass = GetEventClass (event);
	uint32_t kind = GetEventKind (event); 

	result = CallNextEventHandler(nextHandler, event);
	
        QDEB("KeyEventHandler\n");

	if( evtClass == kEventClassKeyboard)
	{
		char macCharCodes;
		uint32_t macKeyCode;
		uint32_t macKeyModifiers;
	
		GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof(macCharCodes), NULL, &macCharCodes);
		GetEventParameter(event, kEventParamKeyCode, typeUInt32, NULL, sizeof(macKeyCode), NULL, &macKeyCode);
		GetEventParameter(event, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(macKeyModifiers), NULL, &macKeyModifiers);
		
		if(macKeyModifiers != 256)
		{
			if (kind == kEventRawKeyRepeat || kind == kEventRawKeyDown)
			{
				//printf("keypress %d %d \n",macKeyCode, macCharCodes);
                                if (quartzRemote)
                                        quartzRemote->PutKey(macKeyCode);
			}
		}
		else if(macKeyModifiers == 256)
		{
	/*		switch(macCharCodes)
			{
				case '[': SetWindowAlpha(theWindow, winAlpha-=0.05); break;
				case ']': SetWindowAlpha(theWindow, winAlpha+=0.05); break;		
			}	
	*/	}
		else
			result = eventNotHandledErr;
	}
	
    return result;
}

//default window event handler
static OSStatus DefaultWindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData) {

        if (vout)
                return vout->WindowEventHandler(nextHandler,event,userData);
        return eventNotHandledErr;
};

OSStatus cQuartzVideoOut::WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData) {
        OSStatus result = noErr;
	uint32_t evtClass = GetEventClass (event);
	UInt32 kind = GetEventKind (event); 

	result = CallNextEventHandler(nextHandler, event);
	
        QDEB("WindowEventHandler result %d\n",result);
	//aspect(&d_width,&d_height,A_NOZOOM);

	if( evtClass == kEventClassCommand)
	{
		HICommand theHICommand;
		GetEventParameter( event, kEventParamDirectObject, typeHICommand, NULL, sizeof( HICommand ), NULL, &theHICommand );
		
		switch ( theHICommand.commandID )
		{
			case kHICommandQuit:
                                theWindow = NULL;
				//mplayer_put_key(KEY_CLOSE_WIN);
				break;
				
			case kHalfScreenCmd:
					if (isFullscreen) {
						ToggleFullscreen();
					}
						
					SizeWindow(theWindow,IMAGE_WIDTH/2, IMAGE_HEIGHT/2, 1);
					WindowResized();
				break;

			case kNormalScreenCmd:
					if (isFullscreen) {
						ToggleFullscreen();
					}
						
					SizeWindow(theWindow, IMAGE_WIDTH, IMAGE_HEIGHT, 1);
					WindowResized();
				break;

			case kDoubleScreenCmd:
					if (isFullscreen) {
						ToggleFullscreen();
					}
						
					SizeWindow(theWindow, IMAGE_WIDTH*2, IMAGE_HEIGHT*2, 1);
					WindowResized();
				break;

			case kFullScreenCmd:
				ToggleFullscreen();
				break;
/*
			case kKeepAspectCmd:
				vo_keepaspect = (!(vo_keepaspect));
				CheckMenuItem (aspectMenu, 1, vo_keepaspect);
				window_resized();
				break;
				
			case kAspectOrgCmd:
				movie_aspect = old_movie_aspect;
				if(!vo_quartz_fs)
				{
					SizeWindow(theWindow, dstRect.right, (dstRect.right/movie_aspect),1);
				}
				window_resized();
				break;
				
			case kAspectFullCmd:
				movie_aspect = 4.0f/3.0f;
				if(!vo_quartz_fs)
				{
					SizeWindow(theWindow, dstRect.right, (dstRect.right/movie_aspect),1);
				}
				window_resized();
				break;
				
			case kAspectWideCmd:
				movie_aspect = 16.0f/9.0f;
				if(!vo_quartz_fs)
				{
					SizeWindow(theWindow, dstRect.right, (dstRect.right/movie_aspect),1);
				}
				window_resized();
				break;
				
			case kPanScanCmd:
				vo_panscan = (!(vo_panscan));
				CheckMenuItem (aspectMenu, 2, vo_panscan);
				window_panscan();
				window_resized();
				break;
		*/	
			default:
				result = eventNotHandledErr;
				break;
		}
	}
	else if( evtClass == kEventClassWindow)
	{
		WindowRef     window;
		Rect          rectPort = {0,0,0,0};
		
		GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);
	
		if(window)
		{
			GetPortBounds(GetWindowPort(window), &rectPort);
		}   
	
		switch (kind)
		{
			case kEventWindowClosed:
				theWindow = NULL;
				//mplayer_put_key(KEY_CLOSE_WIN);
				break;
				
			//resize window
			case kEventWindowZoomed:
			case kEventWindowBoundsChanged:
                                QDEB("resized..\n");
				vout->WindowResized();
				//flip_page();
				//window_resized();
				break;
			
			default:
				result = eventNotHandledErr;
				break;
		}
	}

    QDEB("result %d\n",result);	
    return result;
}

void cQuartzVideoOut::CreateWindow(uint32_t d_width, uint32_t d_height, WindowAttributes windowAttrs)  {        
	CFStringRef		titleKey;
	CFStringRef		windowTitle; 
	OSStatus	       	result;
        
        SetRect(&winRect, 0, 0, d_width, d_height);

 	//Clear Menu Bar
	//ClearMenuBar();

	CFStringRef movMenuTitle;
	//CFStringRef aspMenuTitle;
	MenuItemIndex index;
 
	//Create Window Menu
	CreateStandardWindowMenu(0, &windMenu);
	InsertMenu(windMenu, 0);

	
	//Create Movie Menu
	CreateNewMenu (1004, 0, &movMenu);
	movMenuTitle = CFSTR("Movie");
	SetMenuTitleWithCFString(movMenu, movMenuTitle);
	
	AppendMenuItemTextWithCFString(movMenu, CFSTR("Half Size"), 0, kHalfScreenCmd, &index);
	SetMenuItemCommandKey(movMenu, index, 0, '0');
	
	AppendMenuItemTextWithCFString(movMenu, CFSTR("Normal Size"), 0, kNormalScreenCmd, &index);
	SetMenuItemCommandKey(movMenu, index, 0, '1');
	
	AppendMenuItemTextWithCFString(movMenu, CFSTR("Double Size"), 0, kDoubleScreenCmd, &index);
	SetMenuItemCommandKey(movMenu, index, 0, '2');
	
	AppendMenuItemTextWithCFString(movMenu, CFSTR("Full Size"), 0, kFullScreenCmd, &index);
	SetMenuItemCommandKey(movMenu, index, 0, 'F');
/*	
	AppendMenuItemTextWithCFString(movMenu, NULL, kMenuItemAttrSeparator, NULL, &index);

	AppendMenuItemTextWithCFString(movMenu, CFSTR("Aspect Ratio"), 0, NULL, &index);
	
	////Create Aspect Ratio Sub Menu
	CreateNewMenu (0, 0, &aspectMenu);
	aspMenuTitle = CFSTR("Aspect Ratio");
	SetMenuTitleWithCFString(aspectMenu, aspMenuTitle);
	SetMenuItemHierarchicalMenu(movMenu, 6, aspectMenu);
	
	AppendMenuItemTextWithCFString(aspectMenu, CFSTR("Keep"), 0, kKeepAspectCmd, &index);
	//CheckMenuItem (aspectMenu, 1, vo_keepaspect);
	AppendMenuItemTextWithCFString(aspectMenu, CFSTR("Pan-Scan"), 0, kPanScanCmd, &index);
	//CheckMenuItem (aspectMenu, 2, vo_panscan);
	AppendMenuItemTextWithCFString(aspectMenu, NULL, kMenuItemAttrSeparator, NULL, &index);
	AppendMenuItemTextWithCFString(aspectMenu, CFSTR("Original"), 0, kAspectOrgCmd, &index);
	AppendMenuItemTextWithCFString(aspectMenu, CFSTR("4:3"), 0, kAspectFullCmd, &index);
	AppendMenuItemTextWithCFString(aspectMenu, CFSTR("16:9"), 0, kAspectWideCmd, &index);
*/		
	InsertMenu(movMenu, GetMenuID(windMenu)); //insert before Window menu

        DrawMenuBar();


	//create window
	CreateNewWindow(kDocumentWindowClass, windowAttrs, &winRect, &theWindow);
	
	CreateWindowGroup(0, &winGroup);
	SetWindowGroup(theWindow, winGroup);

	//Set window title
	titleKey	= CFSTR("McVDR - Video Disk Recorder for Mac Os X");
	windowTitle     = CFCopyLocalizedString(titleKey, NULL);
	result		= SetWindowTitleWithCFString(theWindow, windowTitle);
	CFRelease(titleKey);
	CFRelease(windowTitle);
  
        //Install event handler
        const EventTypeSpec win_events[] = {
                { kEventClassWindow, kEventWindowClosed },
                { kEventClassWindow, kEventWindowBoundsChanged },
                { kEventClassCommand, kEventCommandProcess }
        };

        const EventTypeSpec key_events[] = {
                { kEventClassKeyboard, kEventRawKeyDown },
                { kEventClassKeyboard, kEventRawKeyRepeat }
        };

	const EventTypeSpec mouse_events[] = {
		{ kEventClassMouse, kEventMouseMoved },
		{ kEventClassMouse, kEventMouseWheelMoved },
		{ kEventClassMouse, kEventMouseDown },
		{ kEventClassMouse, kEventMouseUp },
		{ kEventClassMouse, kEventMouseDragged }
	};
	
	InstallApplicationEventHandler (NewEventHandlerUPP (KeyEventHandler), GetEventTypeCount(key_events), key_events, NULL, NULL);
	InstallApplicationEventHandler (NewEventHandlerUPP (DefaultMouseEventHandler), GetEventTypeCount(mouse_events), mouse_events, NULL, NULL);

	InstallWindowEventHandler (theWindow, NewEventHandlerUPP (DefaultWindowEventHandler), GetEventTypeCount(win_events), win_events, theWindow, NULL);
}


cQuartzVideoOut::cQuartzVideoOut(cSetupStore *store, cSetupSoftlog* Softlog) 
		: cVideoOut(store, Softlog),
        theWindow(NULL), winGroup(NULL), winLevel(1) {
        QDEB("cQuartVideoOut\n");
	WindowAttributes	windowAttrs;
	CGRect tmpBounds;

        if (vout) {
                printf("Quartz already initialized\n");
                exit(-1);
        };
        vout=this;

        glInitialized=false;
        isFullscreen=false;
        lastTime=0;
        osd_shm_id = -1;
        pic_shm_id = -1;

	//Get Main device info///////////////////////////////////////////////////
	deviceHdl = GetMainDevice();
/*	for(i=0; i<device_id; i++)
	{
		deviceHdl = GetNextDevice(deviceHdl);
		
		if(deviceHdl == NULL)
		{
			mp_msg(MSGT_VO, MSGL_FATAL, "Quartz error: Device ID %d do not exist, falling back to main device.\n", device_id);
			deviceHdl = GetMainDevice();
			device_id = 0;
			break;
		}
	}
*/	
	deviceRect = (*deviceHdl)->gdRect;
	int device_width = deviceRect.right-deviceRect.left;
	int device_height = deviceRect.bottom-deviceRect.top;
	
	SetParValues((float)device_width/(float)device_height,(float)device_width/(float)device_height);

	//Create player window//////////////////////////////////////////////////
	windowAttrs =   kWindowStandardDocumentAttributes
					| kWindowStandardHandlerAttribute
					| kWindowLiveResizeAttribute;
					
	windowAttrs &= (~kWindowResizableAttribute);
					
        CreateWindow(IMAGE_WIDTH/2, IMAGE_HEIGHT/2, windowAttrs);
		
        if (theWindow == NULL) {
                esyslog("Quartz error: Couldn't create window !!!!!\n");
                exit(-1);
        }
        tmpBounds = CGRectMake( 0, 0, winRect.right, winRect.bottom);
        CreateCGContextForPort(GetWindowPort(theWindow),&context);
        CGContextFillRect(context, tmpBounds);
       /* 
        //rumprobieren
        SetSystemUIMode( kUIModeNormal, NULL);
        SetWindowGroupLevel(winGroup, CGWindowLevelForKey(levelList[winLevel]));
*/
 
	//Show window
	RepositionWindow(theWindow, NULL, kWindowCenterOnMainScreen);
	ShowWindow( theWindow );
 
        QDEB("init video engine\n");
        
        image_width=IMAGE_WIDTH;
        image_height=IMAGE_HEIGHT;
        int image_bytes=2;

        SetRect(&imgRect, 0, 0, image_width, image_height);

        // video picture
        if ( (pic_shm_id = shmget(IPC_PRIVATE,
                                        (image_width*image_height+10)*image_bytes,
                                        IPC_CREAT | 0666)) < 0 ) {
                fprintf(stderr,"error creating  pic_shm! width %d height %d\n",
                        IMAGE_WIDTH,IMAGE_HEIGHT);
                RmShmMemory();
                exit(-1);
        };

        if ( (image_data[0] = (uint8_t*)shmat(pic_shm_id,NULL,0))
                        == (uint8_t*) -1 ) {
                fprintf(stderr,"error attatching pic shm!\n");
                RmShmMemory();
                exit(-1);
        };
        
        // osd picture
#if 1 
        //image_data[0]= (unsigned char*)malloc(image_width*image_height*image_bytes);
        //image_data[1]= (unsigned char*)malloc(image_width*image_height*image_bytes);

        osd_image_data= (unsigned char*) malloc(IMAGE_WIDTH*IMAGE_HEIGHT*4);
        osd_shm_id=-1;
#else
        if ( (osd_shm_id = shmget(IPC_PRIVATE,
                                        (IMAGE_WIDTH*IMAGE_HEIGHT+10)*4,
                                        IPC_CREAT | 0666)) < 0 ) {
                fprintf(stderr,"error creating  osd_shm! width %d height %d\n",
                        IMAGE_WIDTH,IMAGE_HEIGHT);
                RmShmMemory();
                exit(-1);
        };

        if ( (osd_image_data = (uint8_t*)shmat(osd_shm_id,NULL,0))
                        == (uint8_t*) -1 ) {
                fprintf(stderr,"error attatching osd shm!\n");
                RmShmMemory();
                exit(-1);
        };
#endif


        InitGl();

        if (!quartzRemote)
                quartzRemote = new cSoftRemote("softdevice-quartz");

        QDEB("Finished Quartz constructor\n");
};

cQuartzVideoOut::~cQuartzVideoOut() {
        QDEB("cQuartzVideoOut destructor\n");
        
        RmShmMemory();

        if (quartzRemote) {
                delete quartzRemote;
                quartzRemote = NULL;
        };
};

void cQuartzVideoOut::RmShmMemory() {
        QDEB("RmShmMemory pic %d osd %d\n",pic_shm_id,osd_shm_id);
        if ( pic_shm_id > 0)
                shmctl(pic_shm_id, IPC_RMID, 0);
        pic_shm_id=-1;
        if ( osd_shm_id > 0)
                shmctl(osd_shm_id, IPC_RMID, 0);
        osd_shm_id=-1;
}

void cQuartzVideoOut::ProcessEvents () {
	EventRef theEvent;
	EventTargetRef theTarget;
	OSStatus	theErr;
        //QDEB("ProcessEvents\n");
	
	//Get event
	theTarget = GetEventDispatcherTarget();
        theErr = ReceiveNextEvent(0, 0, kEventDurationNoWait,true, &theEvent);
        if(theErr == noErr && theEvent != NULL) {
                //QDEB("got event\n");
		SendEventToEventTarget(theEvent, theTarget);
		ReleaseEvent(theEvent);
	}

	//update activity every 30 seconds to prevent
	//screensaver from starting up.
	int curTime = TickCount()/60;
		
	if( ((curTime - lastTime) >= 5) || (lastTime == 0) ) {
		UpdateSystemActivity(UsrActivity);
		lastTime = curTime;
	}

};

void cQuartzVideoOut::InitGl() {
        QDEB("initgl\n");

        glMutex.Lock();
        //GLint attrib[]= {AGL_DEPTH_SIZE, 16, AGL_RGBA,AGL_NONE };
       //GLint attrib[]= {AGL_DEPTH_SIZE, 16, AGL_RGBA, AGL_DOUBLEBUFFER, AGL_FULLSCREEN, AGL_NONE };
        GLint  attrib[]= {AGL_RGBA, AGL_DOUBLEBUFFER, AGL_DEPTH_SIZE, 32, AGL_NONE};

        aglFmt = aglChoosePixelFormat(NULL,0,attrib);
        if (!aglFmt) {
                printf("aglFmt NULL. Fatal.\n");
                exit(-1);
        };
        
        aglCtx = aglCreateContext(aglFmt,NULL);
        if (!aglCtx) {        
                printf("aglCtx NULL. Fatal.\n");
                exit(-1);
        };
 
        if ( !aglSetDrawable( aglCtx, GetWindowPort(theWindow) ) ) {
                printf("Could not set drawable. Fatal.\n");
                exit(-1);
        };

        if ( !aglSetCurrentContext( aglCtx) ) {
                printf("Could not set current gl context. Fatal.\n");
                exit(-1);
        };

        aglEnable(aglCtx, AGL_SWAP_INTERVAL);
        
        long interval = 1;
        aglSetInteger(aglCtx, AGL_SWAP_INTERVAL, &interval);
        

	glEnable(GL_BLEND); 
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

        glColor4f(1.0f,1.0f,1.0f,0.1f);			// Full Brightness, 50% Alpha ( NEW )
	glBlendFunc(GL_ZERO,GL_ONE);	
	//glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	

        glViewport(0,0,winRect.right-winRect.left,winRect.bottom-winRect.top);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, winRect.right-winRect.left, winRect.bottom-winRect.top, 0, -1.0, 1.0);
	//glOrtho(0, IMAGE_WIDTH, IMAGE_HEIGHT, 0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

        curr_index=0;
        glDeleteTextures( 2, p_textures );
        glGenTextures( 2, p_textures );

        for( int i = 0; i < 1; i++ ) {
                glBindTexture( GL_TEXTURE_RECTANGLE_EXT, p_textures[i] );

                /* Set the texture parameters */
                glTexParameterf( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, 1.0 );

                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

                glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

#ifdef __APPLE__
                /* Tell the driver not to make a copy of the texture but to use
                   our buffer */
                glEnable( GL_UNPACK_CLIENT_STORAGE_APPLE );
                glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE );

#if 0
                /* Use VRAM texturing */
                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE,
                                GL_STORAGE_CACHED_APPLE );
#else
                /* Use AGP texturing */
                glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE,
                                GL_STORAGE_SHARED_APPLE );
#endif
#endif

                /* Call glTexImage2D only once, and use glTexSubImage2D later */
                glTexImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 3, image_width,
                                image_height, 0, GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE,
                                image_data[i] );
        }


        // init osd layer
        glDeleteTextures( 1, &osd_texture );
        glGenTextures( 1, &osd_texture );

        glBindTexture( GL_TEXTURE_RECTANGLE_EXT, osd_texture );

        /* Set the texture parameters */
        glTexParameterf( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_PRIORITY, 1.0 );

        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_MIN_FILTER, GL_LINEAR );

        glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

#ifdef __APPLE__
        /* Tell the driver not to make a copy of the texture but to use
           our buffer */
        glEnable( GL_UNPACK_CLIENT_STORAGE_APPLE );
        glPixelStorei( GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE );

#if 0
        /* Use VRAM texturing */
        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE,
                        GL_STORAGE_CACHED_APPLE );
#else
        /* Use AGP texturing */
        glTexParameteri( GL_TEXTURE_RECTANGLE_EXT, GL_TEXTURE_STORAGE_HINT_APPLE,
                        GL_STORAGE_SHARED_APPLE );
#endif
#endif

        /* Call glTexImage2D only once, and use glTexSubImage2D later */
        glTexImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 4, IMAGE_WIDTH,
                        IMAGE_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,//GL_UNSIGNED_INT_8_8_8_8_REV,
                        osd_image_data );

        glInitialized=true;
        glMutex.Unlock();
};

void cQuartzVideoOut::GetLockOsdSurface(uint8_t *&osd, int &stride, bool *&dirtyLines) {
        osd=osd_image_data; 
        stride=IMAGE_WIDTH*4; 
        dirtyLines=NULL;
};

void cQuartzVideoOut::CommitUnlockOsdSurface() { 
        cVideoOut::CommitUnlockOsdSurface();
        Render();
};

void cQuartzVideoOut::YUV(sPicBuffer *buf) {

        Render();
};


void cQuartzVideoOut::Render() {
        //QDEB("YUV\n");
        //        CopyPicBuf(&privBuffer,buf,buf->width,buf->height,
        //                0,0,0,0);

        if (!glInitialized)
                return;
#if 0
        if (buf->pixel[0]!=NULL) {
                yv12_to_yuy2_fr_a( buf->pixel[0], buf->pixel[2], buf->pixel[1],
                                image_data[curr_index], buf->width, buf->height,
                                buf->stride[0], buf->stride[1], 2*IMAGE_WIDTH);
        };
#endif
        glMutex.Lock();
        int i_new_index;
        i_new_index = 0;//( curr_index + 1 ) & 1;


        /* Update the texture */
        glBindTexture( GL_TEXTURE_RECTANGLE_EXT, p_textures[i_new_index] );
        glTexSubImage2D( GL_TEXTURE_RECTANGLE_EXT, 0, 0, 0,
                        image_width,
                        image_height,
                        GL_YCBCR_422_APPLE, GL_UNSIGNED_SHORT_8_8_APPLE, image_data[i_new_index] );

        /* Bind to the previous texture for drawing */
        glBindTexture( GL_TEXTURE_RECTANGLE_EXT, p_textures[curr_index] );

        /* Switch buffers */
        curr_index = i_new_index;

        /* glTexCoord works differently with GL_TEXTURE_2D and
           GL_TEXTURE_RECTANGLE_EXT */
        float f_x = (float)sxoff;
        float f_y = (float)syoff;
        float f_width = (float)swidth;
        float f_height = (float)sheight;

        /* Why drawing here and not in Render()? Because this way, the
           OpenGL providers can call pf_display to force redraw. Currently,
           the OS X provider uses it to get a smooth window resizing */

        glClear( GL_COLOR_BUFFER_BIT );
/*
        glEnable( GL_TEXTURE_RECTANGLE_EXT );
        glBegin( GL_POLYGON );
        glTexCoord2f( f_x, f_y ); glVertex2f( -1.0, 1.0 );
        glTexCoord2f( f_width, f_y ); glVertex2f( 1.0, 1.0 );
        glTexCoord2f( f_width, f_height ); glVertex2f( 1.0, -1.0 );
        glTexCoord2f( f_x, f_height ); glVertex2f( -1.0, -1.0 );
        glEnd();
        glDisable( GL_TEXTURE_RECTANGLE_EXT );
*/
        float winWidth=(float)(winRect.right-winRect.left);
        float winHeight=(float)(winRect.bottom-winRect.top);


        glColor4f(1.0f,1.0f,1.0f,0.0f);			
	glBlendFunc(GL_ONE,GL_ZERO);	
        glEnable( GL_TEXTURE_RECTANGLE_EXT );
        glBegin( GL_POLYGON );
        glTexCoord2f( f_x, f_y ); glVertex2f( (float)lxoff, (float)lyoff);
        glTexCoord2f( f_width, f_y ); glVertex2f( (float)lwidth , (float)lyoff );
        glTexCoord2f( f_width, f_height ); glVertex2f( (float)lwidth, (float)lheight );
        glTexCoord2f( f_x, f_height ); glVertex2f( (float)lxoff, (float)lheight );
        glEnd();
        glDisable( GL_TEXTURE_RECTANGLE_EXT );

#if 0
        // osd layer
        glColor4f(1.0f,1.0f,1.0f,1.0f);	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);	
        glBindTexture( GL_TEXTURE_RECTANGLE_EXT, osd_texture );

        glEnable( GL_TEXTURE_RECTANGLE_EXT );
        glBegin( GL_POLYGON );
        glTexCoord2f( 0.0, 0.0 ); glVertex2f( (float)0.0, (float)0.0);
        glTexCoord2f( (float) IMAGE_WIDTH, 0.0 ); glVertex2f( (float) lwidth , (float)0.0 );
        glTexCoord2f( (float) IMAGE_WIDTH, (float) IMAGE_HEIGHT ); glVertex2f( (float)lwidth, (float) lheight );
        glTexCoord2f( 0.0, (float) IMAGE_HEIGHT ); glVertex2f( (float)0.0, (float)lheight );
        glEnd();
        glDisable( GL_TEXTURE_RECTANGLE_EXT );
#endif

	//render resize box
	if ( !isFullscreen ) {
		
		glBegin(GL_LINES);
		glColor4f(0.2, 0.2, 0.2, 0.5);
		glVertex2f(winWidth-1, winHeight-1); glVertex2f(winWidth-1, winHeight-1);
		glVertex2f(winWidth-1, winHeight-5); glVertex2f(winWidth-5, winHeight-1);
		glVertex2f(winWidth-1, winHeight-9); glVertex2f(winWidth-9, winHeight-1);

		glColor4f(0.4, 0.4, 0.4, 0.5);
		glVertex2f(winWidth-1, winHeight-2); glVertex2f(winWidth-2, winHeight-1);
		glVertex2f(winWidth-1, winHeight-6); glVertex2f(winWidth-6, winHeight-1);
		glVertex2f(winWidth-1, winHeight-10); glVertex2f(winWidth-10, winHeight-1);
		
		glColor4f(0.6, 0.6, 0.6, 0.5);
		glVertex2f(winWidth-1, winHeight-3); glVertex2f(winWidth-3, winHeight-1);
		glVertex2f(winWidth-1, winHeight-7); glVertex2f(winWidth-7, winHeight-1);
		glVertex2f(winWidth-1, winHeight-11); glVertex2f(winWidth-11, winHeight-1);
		glEnd();
	}
	//glFlush();

        aglSwapBuffers(aglCtx);
        glMutex.Unlock();
 };


void cQuartzVideoOut::WindowResized()
{
	CGRect tmpBounds;

        if (!theWindow)
                return;

	GetPortBounds( GetWindowPort(theWindow), &winRect );

        if (glInitialized) {
                glMutex.Lock();
                aglUpdateContext(aglCtx);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, winRect.right-winRect.left, winRect.bottom-winRect.top, 0, -1.0, 1.0);
                glViewport(0,0,winRect.right-winRect.left,winRect.bottom-winRect.top);
                glMutex.Unlock();
        };

        dheight=winRect.bottom;
        dwidth=winRect.right;
        RecalculateAspect();
	
	//Clear Background
	tmpBounds = CGRectMake( 0, 0, winRect.right, winRect.bottom);
	CreateCGContextForPort(GetWindowPort(theWindow),&context);
	CGContextFillRect(context, tmpBounds);

}

void cQuartzVideoOut::ToggleFullscreen(){
        int device_width;
        int device_height;
        QDEB("ToggleFullscreen\n");

   
	//go fullscreen
	if(!isFullscreen) {
		//if(winLevel != 0)
		{
			//if(device_id == 0)
			{
				SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
				CGDisplayHideCursor(kCGDirectMainDisplay);
				//mouseHide = TRUE;
			}
			
			//if(fs_res_x != 0 || fs_res_y != 0)
			{
				//BeginFullScreen( &restoreState, deviceHdl, &fs_res_x, &fs_res_y, NULL, NULL, NULL);
				
				//Get Main device info///////////////////////////////////////////////////
				deviceRect = (*deviceHdl)->gdRect;
        
				device_width = deviceRect.right;
				device_height = deviceRect.bottom;
			}
		}

		//save old window size
 		//if (!vo_quartz_fs)
		{
			GetWindowPortBounds(theWindow, &oldWinRect);
			GetWindowBounds(theWindow, kWindowContentRgn, &oldWinBounds);
		}
		
		//go fullscreen
                QDEB("Going fullscreen pos: %d,%d size: %d,%d\n",
                        deviceRect.left,deviceRect.top,device_width,device_height);
		//panscan_calc();
		ChangeWindowAttributes(theWindow, kWindowNoShadowAttribute, 0);
		MoveWindow(theWindow, deviceRect.left, deviceRect.top, 1);
		SizeWindow(theWindow, device_width, device_height,1);

		isFullscreen=true;
	}
	else //go back to windowed mode
	{
		isFullscreen=false;
/*		if(restoreState != NULL)
		{
			//EndFullScreen(restoreState, NULL);
		
			//Get Main device info///////////////////////////////////////////////////
			//deviceRect = (*deviceHdl)->gdRect;
        
			device_width = deviceRect.right;
			device_height = deviceRect.bottom;
			restoreState = NULL;
		}*/
		SetSystemUIMode( kUIModeNormal, NULL);

		//show mouse cursor
		CGDisplayShowCursor(kCGDirectMainDisplay);
		//mouseHide = FALSE;
		
                QDEB("Going into windowed mode pos %d,%d size: %d,%d \n",
                        oldWinBounds.left,oldWinBounds.top,oldWinRect.right,oldWinRect.bottom);
		//revert window to previous setting
		ChangeWindowAttributes(theWindow, 0, kWindowNoShadowAttribute);
		MoveWindow(theWindow, oldWinBounds.left, oldWinBounds.top, 1);
		SizeWindow(theWindow, oldWinRect.right, oldWinRect.bottom,1);
	}
	WindowResized();
};


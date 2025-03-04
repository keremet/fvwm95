/****************************************************************************
 * This module is based on Twm, but has been siginificantly modified 
 * by Rob Nation 
 ****************************************************************************/
/*****************************************************************************/
/**       Copyright 1988 by Evans & Sutherland Computer Corporation,        **/
/**                          Salt Lake City, Utah                           **/
/**  Portions Copyright 1989 by the Massachusetts Institute of Technology   **/
/**                        Cambridge, Massachusetts                         **/
/**                                                                         **/
/**                           All Rights Reserved                           **/
/**                                                                         **/
/**    Permission to use, copy, modify, and distribute this software and    **/
/**    its documentation  for  any  purpose  and  without  fee is hereby    **/
/**    granted, provided that the above copyright notice appear  in  all    **/
/**    copies and that both  that  copyright  notice  and  this  permis-    **/
/**    sion  notice appear in supporting  documentation,  and  that  the    **/
/**    names of Evans & Sutherland and M.I.T. not be used in advertising    **/
/**    in publicity pertaining to distribution of the  software  without    **/
/**    specific, written prior permission.                                  **/
/**                                                                         **/
/**    EVANS & SUTHERLAND AND M.I.T. DISCLAIM ALL WARRANTIES WITH REGARD    **/
/**    TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES  OF  MERCHANT-    **/
/**    ABILITY  AND  FITNESS,  IN  NO  EVENT SHALL EVANS & SUTHERLAND OR    **/
/**    M.I.T. BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL  DAM-    **/
/**    AGES OR  ANY DAMAGES WHATSOEVER  RESULTING FROM LOSS OF USE, DATA    **/
/**    OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER    **/
/**    TORTIOUS ACTION, ARISING OUT OF OR IN  CONNECTION  WITH  THE  USE    **/
/**    OR PERFORMANCE OF THIS SOFTWARE.                                     **/
/*****************************************************************************/


/***********************************************************************
 *
 * fvwm event handling
 *
 ***********************************************************************/

#include <FVWMconfig.h>

#ifdef ISC
#include <sys/bsdtypes.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
/* Some people say that AIX and AIXV3 need 3 preceding underscores, other say
 * no. I'll do both */
#if defined ___AIX || defined _AIX || defined __QNX__ || defined ___AIXV3 || defined AIXV3 || defined _SEQUENT_
#include <sys/select.h>
#endif

#include "fvwm.h"
#include <X11/Xatom.h>
#include "menus.h"
#include "misc.h"
#include "parse.h"
#include "screen.h"
#ifdef SHAPE
#include <X11/extensions/shape.h>
#endif /* SHAPE */
#include "module.h"

unsigned int mods_used = (ShiftMask | ControlMask | Mod1Mask |
			  Mod2Mask| Mod3Mask| Mod4Mask| Mod5Mask);
extern int menuFromFrameOrWindowOrTitlebar;


int Context = C_NO_CONTEXT;	/* current button press context */
int Button = 0;
FvwmWindow *ButtonWindow;	/* button press window structure */
XEvent Event;			/* the current event */
FvwmWindow *Tmp_win;		/* the current fvwm window */

int last_event_type=0;
Window last_event_window=0;

#ifdef SHAPE
extern int ShapeEventBase;
void HandleShapeNotify(void);
#endif /* SHAPE */

Window PressedW;

/*
** LASTEvent is the number of X events defined - it should be defined
** in X.h (to be like 35), but since extension (eg SHAPE) events are
** numbered beyond LASTEvent, we need to use a bigger number than the
** default, so let's undefine the default and use 256 instead.
*/

#undef LASTEvent
#ifndef LASTEvent
#define LASTEvent 256
#endif /* !LASTEvent */
typedef void (*PFEH)();
PFEH EventHandlerJumpTable[LASTEvent];

/*
** Procedure:
**   InitEventHandlerJumpTable
*/
void InitEventHandlerJumpTable(void)
{
  int i;

  for (i=0; i<LASTEvent; i++)
  {
    EventHandlerJumpTable[i] = NULL;
  }
  EventHandlerJumpTable[Expose] =           HandleExpose;
  EventHandlerJumpTable[DestroyNotify] =    HandleDestroyNotify;
  EventHandlerJumpTable[MapRequest] =       HandleMapRequest;
  EventHandlerJumpTable[MapNotify] =        HandleMapNotify;
  EventHandlerJumpTable[UnmapNotify] =      HandleUnmapNotify;
  EventHandlerJumpTable[ButtonPress] =      HandleButtonPress;
  EventHandlerJumpTable[EnterNotify] =      HandleEnterNotify;
  EventHandlerJumpTable[LeaveNotify] =      HandleLeaveNotify;
  EventHandlerJumpTable[FocusIn] =          HandleFocusIn;
  EventHandlerJumpTable[ConfigureRequest] = HandleConfigureRequest;
  EventHandlerJumpTable[ClientMessage] =    HandleClientMessage;
  EventHandlerJumpTable[PropertyNotify] =   HandlePropertyNotify;
  EventHandlerJumpTable[KeyPress] =         HandleKeyPress;
  EventHandlerJumpTable[VisibilityNotify] = HandleVisibilityNotify;
  EventHandlerJumpTable[ColormapNotify] =   HandleColormapNotify;
#ifdef SHAPE
  if(ShapesSupported)
    EventHandlerJumpTable[ShapeEventBase+ShapeNotify] = HandleShapeNotify;
#endif /* SHAPE */
}

/***********************************************************************
 *
 *  Procedure:
 *	DispatchEvent - handle a single X event stored in global var Event
 *
 ************************************************************************/
void DispatchEvent()
{
  Window w = Event.xany.window;

  DBUG("DispatchEvent","Routine Entered");

  StashEventTime(&Event);

  if (XFindContext (dpy, w, FvwmContext, (caddr_t *) &Tmp_win) == XCNOENT)
    Tmp_win = NULL;
  last_event_type = Event.type;
  last_event_window = w;

#if 0
  switch(Event.type)
    {
    case Expose:
      DBUG("DispatchEvent","Event = Expose");
      HandleExpose();
      break;
    case DestroyNotify:
      DBUG("DispatchEvent","Event = DestroyNotify");
      HandleDestroyNotify();
      break;
    case MapRequest:
      DBUG("DispatchEvent","Event = MapRequest");
      HandleMapRequest(None);
      break;
    case MapNotify:
      DBUG("DispatchEvent","Event = MapNotify");
      HandleMapNotify();
      break;
    case UnmapNotify:
      DBUG("DispatchEvent","Event = UnmapNotify");
      HandleUnmapNotify();
      break;
    case ButtonPress:
      DBUG("DispatchEvent","Event = ButtonPress");
      HandleButtonPress();
      break;
    case EnterNotify:
      DBUG("DispatchEvent","Event = EnterNotify");
      HandleEnterNotify();
      break;
    case LeaveNotify:
      DBUG("DispatchEvent","Event = LeaveNotify");
      HandleLeaveNotify();
      break;
    case FocusIn:
      DBUG("DispatchEvent","Event = FocusIn");
      HandleFocusIn();
      break;
    case ConfigureRequest:
      DBUG("DispatchEvent","Event = ConfigureRequest");
      HandleConfigureRequest();
      break;
    case ClientMessage:
      DBUG("DispatchEvent","Event = ClientMessage");
      HandleClientMessage();
      break;
    case PropertyNotify:
      DBUG("DispatchEvent","Event = PropertyNotify");
      HandlePropertyNotify();
      break;
    case KeyPress:
      DBUG("DispatchEvent","Event = KeyPress");
      HandleKeyPress();
      break;
    case VisibilityNotify:
      DBUG("DispatchEvent","Event = VisibilityNotify");
      HandleVisibilityNotify();
      break;
    case ColormapNotify:
      DBUG("DispatchEvent","Event = ColormapNotify");
      HandleColormapNotify();
      break;
    default:
#ifdef SHAPE
      if(ShapesSupported)
        if(Event.type == (ShapeEventBase + ShapeNotify))
        {
          DBUG("DispatchEvent","Event = ShapeNotify");
          HandleShapeNotify();
        }
#endif /* SHAPE */
      break;
    }
#else
  if (EventHandlerJumpTable[Event.type])
    (*EventHandlerJumpTable[Event.type])();
#endif /* 0 */
  DBUG("DispatchEvent","Leaving Routine");
  return;
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleEvents - handle X events
 *
 ************************************************************************/
void HandleEvents()
{
  DBUG("HandleEvents","Routine Entered");
  while (TRUE)
    {
      last_event_type = 0;
      if(My_XNextEvent(dpy, &Event))
	{
	  DispatchEvent ();
	}
    }
}

/***********************************************************************
 *
 *  Procedure:
 *	Find the Fvwm context for the Event.
 *
 ************************************************************************/
int GetContext(FvwmWindow *t, XEvent *e, Window *w)
{
  int Context,i;
  
  if(!t)
    return C_ROOT; 
  
  Context = C_NO_CONTEXT;
  *w= e->xany.window;
  
  if(*w == Scr.NoFocusWin)
    return C_ROOT;
  
  /* Since key presses and button presses are grabbed in the frame
   * when we have re-parented windows, we need to find out the real
   * window where the event occured */
  if((e->type == KeyPress)&&(e->xkey.subwindow != None))
    *w = e->xkey.subwindow;
  
  if((e->type == ButtonPress)&&(e->xbutton.subwindow != None)&&
     ((e->xbutton.subwindow == t->w)||(e->xbutton.subwindow == t->Parent)))
    *w = e->xbutton.subwindow;
  
  if (*w == Scr.Root)
    Context = C_ROOT;
  if (t)
    {
      if (*w == t->title_w)
	Context = C_TITLE;
      if ((*w == t->w)||(*w == t->Parent))
	Context = C_WINDOW;
      if (*w == t->icon_w)
	Context = C_ICON;
      if (*w == t->icon_pixmap_w)
	Context = C_ICON;
      if (*w == t->frame)
	Context = C_SIDEBAR;
      for(i=0;i<4;i++)
	if(*w == t->corners[i])
	  {
	    Context = C_FRAME;
	    Button = i;
	  }
      for(i=0;i<4;i++)
	if(*w == t->sides[i])
	  {
	    Context = C_SIDEBAR;
	    Button = i;
	  }
      for(i=0;i<Scr.nr_left_buttons;i++)
	{
	  if(*w == t->left_w[i])
	    {
	      Context = (1<<i)*C_L1;
	      Button = i;
	    }
	}
      for(i=0;i<Scr.nr_right_buttons;i++)
	{
	  if(*w == t->right_w[i])
	    {
	      Context = (1<<i)*C_R1;
	      Button = i;
	    }
	}
    }
  return Context;
}

/***********************************************************************
 *
 *  Procedure:
 *	HandleFocusIn - handles focus in events
 *
 ************************************************************************/
void HandleFocusIn()
{
  XEvent d;
  Window w;
  
  w= Event.xany.window;
  while(XCheckTypedEvent(dpy,FocusIn,&d))
    {
      w = d.xany.window;
    }
  if (XFindContext (dpy, w, FvwmContext, (caddr_t *) &Tmp_win) == XCNOENT)
    {
      Tmp_win = NULL;
    }
  
  if(!Tmp_win)
    {
      if(w != Scr.NoFocusWin)
	{
	  Scr.UnknownWinFocused = w;
	}
      else
	{
	  SetBorder(Scr.Hilite,False,True,True,None);
	  Broadcast(M_FOCUS_CHANGE,5,0,0,0,Scr.ActiveTitleColors.fore,
		    Scr.ActiveTitleColors.back,0,0);
	  if (Scr.ColormapFocus == COLORMAP_FOLLOWS_FOCUS)
	    {
	      if((Scr.Hilite)&&(!(Scr.Hilite->flags & ICONIFIED)))
		{
		  InstallWindowColormaps(Scr.Hilite);
		}
	      else
		{
		  InstallWindowColormaps(NULL);
		}
	    }
	  
	}
    }
  else if(Tmp_win != Scr.Hilite)
    {
      SetBorder(Tmp_win,True,True,True,None);
      Broadcast(M_FOCUS_CHANGE,5,Tmp_win->w,
		Tmp_win->frame,(unsigned long)Tmp_win,Scr.ActiveTitleColors.fore,
		Scr.ActiveTitleColors.back,0,0);
      if (Scr.ColormapFocus == COLORMAP_FOLLOWS_FOCUS)
	{
	  if((Scr.Hilite)&&(!(Scr.Hilite->flags & ICONIFIED)))
	    {
	      InstallWindowColormaps(Scr.Hilite);
	    }
	  else
	    {
	      InstallWindowColormaps(NULL);
	    }
	}
    }
}

/***********************************************************************
 *
 *  Procedure:
 *	HandleKeyPress - key press event handler
 *
 ************************************************************************/
void HandleKeyPress()
{
  Binding *key;
  unsigned int modifier;
  modifier = (Event.xkey.state & mods_used);
  ButtonWindow = Tmp_win;
 
  Context = GetContext(Tmp_win,&Event, &PressedW);
  PressedW = None;

  /* Here's a real hack - some systems have two keys with the
   * same keysym and different keycodes. This converts all
   * the cases to one keycode. */
  Event.xkey.keycode = 
    XKeysymToKeycode(dpy,XKeycodeToKeysym(dpy,Event.xkey.keycode,0));
  
  
  for (key = Scr.AllBindings; key != NULL; key = key->NextBinding)
    {
      if ((key->Button_Key == Event.xkey.keycode) &&
	  ((key->Modifier == (modifier&(~LockMask)))||
	   (key->Modifier == AnyModifier)) &&
	  (key->Context & Context)&&
	  (key->IsMouse == 0))
	{
	  ExecuteFunction(key->Action,Tmp_win, &Event,Context,-1);
	  return;
	}
    }
  
  /* if we get here, no function key was bound to the key.  Send it
   * to the client if it was in a window we know about.
   */
  if (Tmp_win)
    {
      if(Event.xkey.window != Tmp_win->w)
	{
	  Event.xkey.window = Tmp_win->w;
	  XSendEvent(dpy, Tmp_win->w, False, KeyPressMask, &Event);
	}
    }

  ButtonWindow = NULL;
}


/***********************************************************************
 *
 *  Procedure:
 *	HandlePropertyNotify - property notify event handler
 *
 ***********************************************************************/
#define MAX_NAME_LEN 200L		/* truncate to this many */
#define MAX_ICON_NAME_LEN 200L		/* ditto */

void HandlePropertyNotify()
{
  XTextProperty text_prop;
#ifdef I18N
  Atom actual = None;
  int actual_format;
  unsigned long nitems, bytesafter;
  char *prop = NULL;
  char **list;
  int num;
#endif
  
  if ((!Tmp_win)||(XGetGeometry(dpy, Tmp_win->w, &JunkRoot, &JunkX, &JunkY,
				&JunkWidth, &JunkHeight, &JunkBW, &JunkDepth) == 0))
    return;
  
  switch (Event.xproperty.atom) 
    {
    case XA_WM_NAME:
      UpdateTitle(Tmp_win);
      BroadcastName(M_WINDOW_NAME,Tmp_win->w,Tmp_win->frame,
		    (unsigned long)Tmp_win,Tmp_win->name);
      
      /* fix the name in the title bar */
      if(!(Tmp_win->flags & ICONIFIED))
	SetTitleBar(Tmp_win,(Scr.Hilite==Tmp_win),True);
      
      /*
       * if the icon name is NoName, set the name of the icon to be
       * the same as the window 
       */
      if (Tmp_win->icon_name == NoName) 
	{
	  Tmp_win->icon_name = Tmp_win->name;
	  BroadcastName(M_ICON_NAME,Tmp_win->w,Tmp_win->frame,
			(unsigned long)Tmp_win,Tmp_win->icon_name);
	  RedoIconName(Tmp_win);
	}
      break;
      
    case XA_WM_ICON_NAME:
#ifdef I18N
     if (XGetWindowProperty (dpy, Tmp_win->w, Event.xproperty.atom, 0L,
                           MAX_NAME_LEN, False, AnyPropertyType, &actual,
                           &actual_format, &nitems, &bytesafter,
                           (unsigned char **) &prop) != Success ||
	 actual == None)
	return;
     text_prop.value = prop;
     text_prop.encoding = actual;
     text_prop.format = actual_format;
     text_prop.nitems = nitems;
     if (XmbTextPropertyToTextList(dpy, &text_prop, &list, &num) < Success)
	return;
#ifdef EVIL
     if (!(num > 0 && *list)) {
       if (!XGetWMIconName (dpy, Tmp_win->w, &text_prop))
	return;
       free_icon_name (Tmp_win);
       Tmp_win->icon_name = (char *) text_prop.value;
     } else {
       prop = *list;
       Tmp_win->icon_name = prop;
       if((Tmp_win->icon_name != NULL) && (strcmp(Tmp_win->icon_name, "") == 0)) {
	 if (!XGetWMIconName (dpy, Tmp_win->w, &text_prop))
           return;
         free_icon_name (Tmp_win);
         Tmp_win->icon_name = (char *) text_prop.value;
       }
     }
#else /* EVIL */
     if (!(num > 0 && *list))
	prop = NoName;
     else
	prop = *list;
     free_icon_name (Tmp_win);
     Tmp_win->icon_name = prop;
#endif /* EVIL */
#else /* I18N */
      if (!XGetWMIconName (dpy, Tmp_win->w, &text_prop))
	return;
      free_icon_name (Tmp_win);
      Tmp_win->icon_name = (char *) text_prop.value;
#endif /* I18N */
      if (Tmp_win->icon_name == NULL)
        Tmp_win->icon_name = NoName;
      BroadcastName(M_ICON_NAME,Tmp_win->w,Tmp_win->frame,
		    (unsigned long)Tmp_win,Tmp_win->icon_name);
      RedoIconName(Tmp_win);
      break;
      
    case XA_WM_HINTS:
      if (Tmp_win->wmhints) 
	XFree ((char *) Tmp_win->wmhints);
      Tmp_win->wmhints = XGetWMHints(dpy, Event.xany.window);

      if(Tmp_win->wmhints == NULL)
	return;

      if((Tmp_win->wmhints->flags & IconPixmapHint)||
	 (Tmp_win->wmhints->flags & IconWindowHint))
	if(Tmp_win->icon_bitmap_file == Scr.DefaultIcon)
	  Tmp_win->icon_bitmap_file = (char *)0;
      
      if((Tmp_win->wmhints->flags & IconPixmapHint)||
         (Tmp_win->wmhints->flags & IconWindowHint))
	{
	  if (!(Tmp_win->flags & SUPPRESSICON))
	    {
              if (Tmp_win->icon_w)
                XDestroyWindow(dpy,Tmp_win->icon_w);
	      XDeleteContext(dpy, Tmp_win->icon_w, FvwmContext);
	      if(Tmp_win->flags & ICON_OURS)
		{
		  if(Tmp_win->icon_pixmap_w != None)
		    {
		      XDestroyWindow(dpy,Tmp_win->icon_pixmap_w);
		      XDeleteContext(dpy, Tmp_win->icon_pixmap_w, FvwmContext);
		    }
		}
	      else 
		XUnmapWindow(dpy,Tmp_win->icon_pixmap_w);
	    }
          Tmp_win->icon_w = None;
          Tmp_win->icon_pixmap_w = None;
	  Tmp_win->iconPixmap = (Window)NULL;
	  if(Tmp_win->flags & ICONIFIED)
	    {
	      Tmp_win->flags &= ~ICONIFIED;
	      Tmp_win->flags &= ~ICON_UNMAPPED;
	      CreateIconWindow(Tmp_win,Tmp_win->icon_x_loc,Tmp_win->icon_y_loc);
	      Broadcast(M_ICONIFY,7,Tmp_win->w,Tmp_win->frame,
			(unsigned long)Tmp_win,
			Tmp_win->icon_x_loc,
			Tmp_win->icon_y_loc,
			Tmp_win->icon_w_width,
			Tmp_win->icon_w_height);
	      BroadcastConfig(M_CONFIGURE_WINDOW,Tmp_win);
	      
	      if (!(Tmp_win->flags & SUPPRESSICON))
		{
		  LowerWindow(Tmp_win);
		  AutoPlace(Tmp_win);
		  if(Tmp_win->Desk == Scr.CurrentDesk)
		    {
                      if(Tmp_win->icon_w)
                        XMapWindow(dpy, Tmp_win->icon_w);
		      if(Tmp_win->icon_pixmap_w != None)
			XMapWindow(dpy, Tmp_win->icon_pixmap_w);
		    }
		}
	      Tmp_win->flags |= ICONIFIED;
	      DrawIconWindow(Tmp_win);
	    }
	}
      break;

    case XA_WM_NORMAL_HINTS:
      {
	int new_width, new_height;
	
	GetWindowSizeHints(Tmp_win);
	new_width = Tmp_win->frame_width;
	new_height = Tmp_win->frame_height;
	ConstrainSize(Tmp_win, &new_width, &new_height);
	if((new_width != Tmp_win->frame_width)||
	   (new_height != Tmp_win->frame_height))
	  SetupFrame(Tmp_win,Tmp_win->frame_x, Tmp_win->frame_y,
		     new_width,new_height,False);

	BroadcastConfig(M_CONFIGURE_WINDOW,Tmp_win);
      }
      break;

    default:
      if (Event.xproperty.atom == _XA_WM_PROTOCOLS)
        FetchWmProtocols(Tmp_win);
      else if (Event.xproperty.atom == _XA_WM_COLORMAP_WINDOWS)
        {
          FetchWmColormapWindows(Tmp_win);   /* frees old data */
          ReInstallActiveColormap();
        }
      else if (Event.xproperty.atom == _XA_WM_STATE)
        {
          if ((Tmp_win != NULL) && (Tmp_win->flags & ClickToFocus)
            &&(Tmp_win == Scr.Focus))
            {
	      Scr.Focus = NULL;
	      SetFocus(Tmp_win->w, Tmp_win, 0);
	    }
        }
      break;
    }
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleClientMessage - client message event handler
 *
 ************************************************************************/
void HandleClientMessage()
{
  XEvent button;
  
  if ((Event.xclient.message_type == _XA_WM_CHANGE_STATE)&&
      (Tmp_win)&&(Event.xclient.data.l[0]==IconicState)&&
      !(Tmp_win->flags & ICONIFIED))
  {
    XQueryPointer( dpy, Scr.Root, &JunkRoot, &JunkChild,
                   &(button.xmotion.x_root),
                   &(button.xmotion.y_root),
                   &JunkX, &JunkY, &JunkMask);
    button.type = 0;
    ExecuteFunction("Iconify",Tmp_win, &button,C_FRAME,-1);
    return;
  }

  /*
  ** CKH - if we get here, it was an unknown client message, so send
  ** it to the client if it was in a window we know about.  I'm not so
  ** sure this should be done or not, since every other window manager
  ** I've looked at doesn't.  But it might be handy for a free drag and
  ** drop setup being developed for Linux.
  */
  if (Tmp_win)
  {
    if(Event.xclient.window != Tmp_win->w)
    {
      Event.xclient.window = Tmp_win->w;
      XSendEvent(dpy, Tmp_win->w, False, NoEventMask, &Event);
    }
  }
}

/***********************************************************************
 *
 *  Procedure:
 *	HandleExpose - expose event handler
 *
 ***********************************************************************/
void HandleExpose()
{
  if (Event.xexpose.count != 0)
    return;
  
  if (Tmp_win)
    {
      if ((Event.xany.window == Tmp_win->title_w))
	{
	  SetTitleBar(Tmp_win,(Scr.Hilite == Tmp_win),False);
	}
      else
	{
	  SetBorder(Tmp_win,(Scr.Hilite == Tmp_win),True,True,Event.xany.window);
	}
    }
  return;
}



/***********************************************************************
 *
 *  Procedure:
 *	HandleDestroyNotify - DestroyNotify event handler
 *
 ***********************************************************************/
void HandleDestroyNotify()
{
  Destroy(Tmp_win);
}




/***********************************************************************
 *
 *  Procedure:
 *	HandleMapRequest - MapRequest event handler
 *
 ************************************************************************/
void HandleMapRequest()
{
  HandleMapRequestKeepRaised(None);
}
void HandleMapRequestKeepRaised(Window KeepRaised)
{
  extern long isIconicState;
  extern Bool PPosOverride;
  
  if(XFindContext(dpy, Event.xmaprequest.window, FvwmContext, 
		  (caddr_t *)&Tmp_win)==XCNOENT)
    Tmp_win = NULL;
  
  if(!PPosOverride)
    XFlush(dpy);
  
  /* If the window has never been mapped before ... */
  if(!Tmp_win)
    {
      /* Add decorations. */
      Tmp_win = AddWindow(Event.xmaprequest.window);
      if (Tmp_win == NULL)
	return;
    }

  if(KeepRaised != None)
    XRaiseWindow(dpy, KeepRaised);

  /* If it's not merely iconified, and we have hints, use them. */
  if (!(Tmp_win->flags & ICONIFIED))
    {
      int state;
      
      if(Tmp_win->wmhints && (Tmp_win->wmhints->flags & StateHint))
	state = Tmp_win->wmhints->initial_state;
      else
	state = NormalState;
      
      if(Tmp_win->flags & STARTICONIC)
	state = IconicState;

      if(isIconicState != DontCareState) 
	state = isIconicState;

      XGrabServer (dpy);
      switch (state) 
	{
	case DontCareState:
	case NormalState:
	case InactiveState:
	default:
	  if (Tmp_win->Desk == Scr.CurrentDesk)
	    {
	      XMapWindow(dpy, Tmp_win->w);
	      XMapWindow(dpy, Tmp_win->frame);
	      Tmp_win->flags |= MAP_PENDING;
	      SetMapStateProp(Tmp_win, NormalState);
	      if((Tmp_win->flags & ClickToFocus)&&
		 ((!Scr.Focus)||(Scr.Focus->flags & ClickToFocus)))
		{
		  SetFocus(Tmp_win->w,Tmp_win,1);
		}
	    }
	  else
	    {
	      XMapWindow(dpy, Tmp_win->w);
	      SetMapStateProp(Tmp_win, NormalState);
	    }
	  break;
	  
	case IconicState:
	  Iconify(Tmp_win, 0, 0);
	  break;
	}
      if(!PPosOverride)
	XSync(dpy,0);
      XUngrabServer(dpy);
    }
  /* If no hints, or currently an icon, just "deiconify" */
  else
    {
      DeIconify(Tmp_win);
    }
  if(!PPosOverride)
    KeepOnTop();
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleMapNotify - MapNotify event handler
 *
 ***********************************************************************/
void HandleMapNotify()
{
  if (!Tmp_win)
    {
      if((Event.xmap.override_redirect == True)&&
	 (Event.xmap.window != Scr.NoFocusWin))
	{
	  XSelectInput(dpy,Event.xmap.window,FocusChangeMask);
	  Scr.UnknownWinFocused = Event.xmap.window;
	}      
      return;
    }

  /* Except for identifying over-ride redirect window mappings, we
   * don't need or want windows associated with the sunstructurenotifymask */
  if(Event.xmap.event != Event.xmap.window)
    return;
   
  /*
   * Need to do the grab to avoid race condition of having server send
   * MapNotify to client before the frame gets mapped; this is bad because
   * the client would think that the window has a chance of being viewable
   * when it really isn't.
   */
  XGrabServer (dpy);
  if (Tmp_win->icon_w)
    XUnmapWindow(dpy, Tmp_win->icon_w);
  if(Tmp_win->icon_pixmap_w != None)
    XUnmapWindow(dpy, Tmp_win->icon_pixmap_w);
  XMapSubwindows(dpy, Tmp_win->frame);

  if(Tmp_win->Desk == Scr.CurrentDesk)
    {
      XMapWindow(dpy, Tmp_win->frame);
    }

  if(Tmp_win->flags & ICONIFIED)
    Broadcast(M_DEICONIFY,3,Tmp_win->w,Tmp_win->frame,
	      (unsigned long)Tmp_win,0,0,0,0);
  else
    {
      Broadcast(M_MAP,3,Tmp_win->w,Tmp_win->frame,
		(unsigned long)Tmp_win,0,0,0,0);
    }
  
  if((Tmp_win->flags & ClickToFocus)&&(Scr.Focus)&&
     ((!Scr.Focus)||(Scr.Focus->flags & ClickToFocus)))
    {
      SetFocus(Tmp_win->w,Tmp_win,1);
    }
  if((!(Tmp_win->flags &(BORDER|TITLE)))&&(Tmp_win->boundary_width <2))
    {
      SetBorder(Tmp_win,False,True,True,Tmp_win->frame);
    }
  XSync(dpy,0);
  XUngrabServer (dpy);
  XFlush (dpy);
  Tmp_win->flags |= MAPPED;
  Tmp_win->flags &= ~MAP_PENDING;
  Tmp_win->flags &= ~ICONIFIED;
  Tmp_win->flags &= ~ICON_UNMAPPED;
  KeepOnTop();
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleUnmapNotify - UnmapNotify event handler
 *
 ************************************************************************/
void HandleUnmapNotify()
{
  int dstx, dsty;
  Window dumwin;
  extern FvwmWindow *colormap_win;

  if(Event.xunmap.event != Event.xunmap.window)
    return;
  /*
   * The July 27, 1988 ICCCM spec states that a client wishing to switch
   * to WithdrawnState should send a synthetic UnmapNotify with the
   * event field set to (pseudo-)root, in case the window is already
   * unmapped (which is the case for fvwm for IconicState).  Unfortunately,
   * we looked for the FvwmContext using that field, so try the window
   * field also.
   */
  if (!Tmp_win)
    {
      Event.xany.window = Event.xunmap.window;
      if (XFindContext(dpy, Event.xany.window,
		       FvwmContext, (caddr_t *)&Tmp_win) == XCNOENT)
	Tmp_win = NULL;
    }
  
  if(!Tmp_win)
    return;

  if(Tmp_win ==  Scr.Hilite)
    Scr.Hilite = NULL;

  if(Scr.PreviousFocus == Tmp_win)
    Scr.PreviousFocus = NULL;

  if((Tmp_win == Scr.Focus)&&(Tmp_win->flags & ClickToFocus))
    {
      if(Tmp_win->next)
	{
	  HandleHardFocus(Tmp_win->next);
	}
      else
	SetFocus(Scr.NoFocusWin,NULL,1);
    }
  
  if(Scr.Focus == Tmp_win)
    SetFocus(Scr.NoFocusWin,NULL,1);

  if(Tmp_win == Scr.pushed_window)
    Scr.pushed_window = NULL;

  if(Tmp_win == colormap_win)
    colormap_win = NULL;

  if ((!(Tmp_win->flags & MAPPED)&&!(Tmp_win->flags&ICONIFIED)))
    {
      return;
    }
  
  XGrabServer(dpy);

/*
  if(XCheckTypedWindowEvent (dpy, Event.xunmap.window, DestroyNotify,&dummy)) 
    {
      Destroy(Tmp_win);
      XUngrabServer (dpy);
      return;
    } 
*/

  /*
   * The program may have unmapped the client window, from either
   * NormalState or IconicState.  Handle the transition to WithdrawnState.
   *
   * We need to reparent the window back to the root (so that fvwm exiting 
   * won't cause it to get mapped) and then throw away all state (pretend 
   * that we've received a DestroyNotify).
   */
  if (XTranslateCoordinates (dpy, Event.xunmap.window, Scr.Root,
			     0, 0, &dstx, &dsty, &dumwin)) 
    {
      XEvent ev;
      Bool reparented;
      
      reparented = XCheckTypedWindowEvent (dpy, Event.xunmap.window, 
					   ReparentNotify, &ev);
      SetMapStateProp (Tmp_win, WithdrawnState);
      if (reparented) 
	{
	  if (Tmp_win->old_bw)
	    XSetWindowBorderWidth (dpy, Event.xunmap.window, Tmp_win->old_bw);
	  if((!(Tmp_win->flags & SUPPRESSICON))&&
	     (Tmp_win->wmhints && (Tmp_win->wmhints->flags & IconWindowHint)))
	    XUnmapWindow (dpy, Tmp_win->wmhints->icon_window);
	} 
      else
	{
	  RestoreWithdrawnLocation (Tmp_win,False);
	}
      XRemoveFromSaveSet (dpy, Event.xunmap.window);
      XSelectInput (dpy, Event.xunmap.window, NoEventMask);
      Destroy(Tmp_win);		/* do not need to mash event before */
      /*
       * Flush any pending events for the window.
       */
      /* Bzzt! it could be about to re-map */
/*      while(XCheckWindowEvent(dpy, Event.xunmap.window,
			      StructureNotifyMask | PropertyChangeMask |
			      ColormapChangeMask | VisibilityChangeMask |
			      EnterWindowMask | LeaveWindowMask, &dummy));
      */
    } /* else window no longer exists and we'll get a destroy notify */
  XUngrabServer (dpy);
  
  XFlush (dpy);
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleButtonPress - ButtonPress event handler
 *
 ***********************************************************************/
void HandleButtonPress()
{
  unsigned int modifier;
  Binding *MouseEntry;
  Window x;
  int LocalContext;
  
  /* click to focus stuff goes here */
  if((Tmp_win)&&(Tmp_win->flags & ClickToFocus)&&(Tmp_win != Scr.Ungrabbed)&&
     ((Event.xbutton.state&
       (ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask)) == 0))
    {
      if(Tmp_win)
	{
	  SetFocus(Tmp_win->w,Tmp_win,1);
#ifdef CLICKY_MODE_1
	  if((Event.xany.window != Tmp_win->w)&&
	     (Event.xbutton.subwindow != Tmp_win->w)&&
	     (Event.xany.window != Tmp_win->Parent)&&
	     (Event.xbutton.subwindow != Tmp_win->Parent))
#endif
	    {
	      RaiseWindow(Tmp_win);
	    }
	  
	  KeepOnTop();

	  /* Why is this here? Seems to cause breakage with
	   * non-focusing windows! */
	  if(!(Tmp_win->flags & ICONIFIED))
	    {
	      XSync(dpy,0);
#if 1 /* pass click event to just clicked to focus window */
	      XAllowEvents(dpy,ReplayPointer,CurrentTime);
#else /* don't pass click to just focused window */
	      XAllowEvents(dpy,AsyncPointer,CurrentTime);
#endif /* 0 */
	      XSync(dpy,0);
	      return;
	    }

	}
    }
  XSync(dpy,0);
  XAllowEvents(dpy,ReplayPointer,CurrentTime);
  XSync(dpy,0);
  
  Context = GetContext(Tmp_win,&Event, &PressedW);
  LocalContext = Context;
  x= PressedW;
  if(Context == C_TITLE)
    SetTitleBar(Tmp_win,(Scr.Hilite == Tmp_win),False);
  else
    SetBorder(Tmp_win,(Scr.Hilite == Tmp_win),True,True,PressedW);	
  
  ButtonWindow = Tmp_win;
  
  /* we have to execute a function or pop up a menu
   */
  
  modifier = (Event.xbutton.state & mods_used);
  /* need to search for an appropriate mouse binding */
  for (MouseEntry = Scr.AllBindings; MouseEntry != NULL;
       MouseEntry= MouseEntry->NextBinding)
    {
      if(((MouseEntry->Button_Key == Event.xbutton.button)||
	  (MouseEntry->Button_Key == 0))&&
	 (MouseEntry->Context & Context)&&
	 ((MouseEntry->Modifier == AnyModifier)||
	  (MouseEntry->Modifier == (modifier& (~LockMask))))&&
	 (MouseEntry->IsMouse == 1))
	{
	  /* got a match, now process it */
	  ExecuteFunction(MouseEntry->Action,Tmp_win, &Event,Context,-1);
	  break;
	}
    }
  PressedW = None;
  if(LocalContext!=C_TITLE)
    SetBorder(ButtonWindow,(Scr.Hilite == ButtonWindow),True,True,x);
  else
    SetTitleBar(ButtonWindow,(Scr.Hilite==ButtonWindow),False);
  ButtonWindow = NULL;
}

/***********************************************************************
 *
 *  Procedure:
 *	HandleEnterNotify - EnterNotify event handler
 *
 ************************************************************************/
void HandleEnterNotify()
{
  XEnterWindowEvent *ewp = &Event.xcrossing;
  XEvent d;
  
  /* look for a matching leaveNotify which would nullify this enterNotify */
  if(XCheckTypedWindowEvent (dpy, ewp->window, LeaveNotify, &d))
    {
      StashEventTime(&d);
      if((d.xcrossing.mode==NotifyNormal)&&
	 (d.xcrossing.detail!=NotifyInferior))
	return;
    }

/* an EnterEvent in one of the PanFrameWindows activates the Paging */
#ifndef NON_VIRTUAL
  if (ewp->window==Scr.PanFrameTop.win 
      || ewp->window==Scr.PanFrameLeft.win
      || ewp->window==Scr.PanFrameRight.win 
      || ewp->window==Scr.PanFrameBottom.win ) 
    {
      int delta_x=0, delta_y=0;
      /* this was in the HandleMotionNotify before, HEDU */
      HandlePaging(Scr.EdgeScrollX,Scr.EdgeScrollY,
                   &Event.xcrossing.x_root,&Event.xcrossing.y_root,
                   &delta_x,&delta_y,True);
      return;
    }
#endif /* NON_VIRTUAL */
 
  if(Event.xany.window == Scr.Root)
    {
      if((Scr.Focus)&&(!(Scr.Focus->flags&ClickToFocus))&&
	 (!(Scr.Focus->flags&SloppyFocus)))
	{
	  SetFocus(Scr.NoFocusWin,NULL,1);
	}
      if (Scr.ColormapFocus == COLORMAP_FOLLOWS_MOUSE)
	{
	  InstallWindowColormaps(NULL);
	}
      return;
    }
  
  /* make sure its for one of our windows */
  if (!Tmp_win) 
    return;
  
  if(!(Tmp_win->flags & ClickToFocus))
    SetFocus(Tmp_win->w,Tmp_win,1);

  if (Scr.ColormapFocus == COLORMAP_FOLLOWS_MOUSE)
  {
    if((!(Tmp_win->flags & ICONIFIED))&&(Event.xany.window == Tmp_win->w))
      InstallWindowColormaps(Tmp_win);
    else
      InstallWindowColormaps(NULL);
  }
  return;
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleLeaveNotify - LeaveNotify event handler
 *
 ************************************************************************/
void HandleLeaveNotify()
{
  /* If we leave the root window, then we're really moving
   * another screen on a multiple screen display, and we
   * need to de-focus and unhighlight to make sure that we
   * don't end up with more than one highlighted window at a time */
  if(Event.xcrossing.window == Scr.Root)
    {
      if(Event.xcrossing.mode == NotifyNormal)
	{
	  if (Event.xcrossing.detail != NotifyInferior) 
	    {
	      if(Scr.Focus != NULL)
		{
		  SetFocus(Scr.NoFocusWin,NULL,1);
		}
	      if(Scr.Hilite != NULL)
		SetBorder(Scr.Hilite,False,True,True,None);
	    }
	}
    }
}


/***********************************************************************
 *
 *  Procedure:
 *	HandleConfigureRequest - ConfigureRequest event handler
 *
 ************************************************************************/
void HandleConfigureRequest()
{
  XWindowChanges xwc;
  unsigned long xwcm;
  int x, y, width, height;
  XConfigureRequestEvent *cre = &Event.xconfigurerequest;
  
  /*
   * Event.xany.window is Event.xconfigurerequest.parent, so Tmp_win will
   * be wrong
   */
  Event.xany.window = cre->window;	/* mash parent field */
  if (XFindContext (dpy, cre->window, FvwmContext, (caddr_t *) &Tmp_win) ==
      XCNOENT)
    Tmp_win = NULL;
  
  /*
   * According to the July 27, 1988 ICCCM draft, we should ignore size and
   * position fields in the WM_NORMAL_HINTS property when we map a window.
   * Instead, we'll read the current geometry.  Therefore, we should respond
   * to configuration requests for windows which have never been mapped.
   */
  if (!Tmp_win || (Tmp_win->icon_w == cre->window))
    {
      xwcm = cre->value_mask & 
	(CWX | CWY | CWWidth | CWHeight | CWBorderWidth);
      xwc.x = cre->x;
      xwc.y = cre->y;
      if((Tmp_win)&&((Tmp_win->icon_w == cre->window)))
	{
	  Tmp_win->icon_xl_loc = cre->x;
	  Tmp_win->icon_x_loc = cre->x + 
	    (Tmp_win->icon_w_width - Tmp_win->icon_p_width)/2;
	  Tmp_win->icon_y_loc = cre->y - Tmp_win->icon_p_height;
	  if(!(Tmp_win->flags & ICON_UNMAPPED))
	    Broadcast(M_ICON_LOCATION,7,Tmp_win->w,Tmp_win->frame,
		      (unsigned long)Tmp_win,
		      Tmp_win->icon_x_loc,Tmp_win->icon_y_loc,
		      Tmp_win->icon_w_width, 
		      Tmp_win->icon_w_height + Tmp_win->icon_p_height);
	}
      xwc.width = cre->width;
      xwc.height = cre->height;
      xwc.border_width = cre->border_width;
      XConfigureWindow(dpy, Event.xany.window, xwcm, &xwc);
      
      if(Tmp_win)
	{
	  xwc.x = Tmp_win->icon_x_loc;
	  xwc.y = Tmp_win->icon_y_loc - Tmp_win->icon_p_height;
	  xwcm = cre->value_mask & (CWX | CWY);
	  if(Tmp_win->icon_pixmap_w != None)
	    XConfigureWindow(dpy, Tmp_win->icon_pixmap_w, xwcm, &xwc);
	  xwc.x = Tmp_win->icon_x_loc;
	  xwc.y = Tmp_win->icon_y_loc;
	  xwcm = cre->value_mask & (CWX | CWY);
	  if(Tmp_win->icon_w != None)
	    XConfigureWindow(dpy, Tmp_win->icon_w, xwcm, &xwc);
	}
      return;
    }
  
  if (cre->value_mask & CWStackMode) 
    {
      FvwmWindow *otherwin;
      
      xwc.sibling = (((cre->value_mask & CWSibling) &&
		      (XFindContext (dpy, cre->above, FvwmContext,
				     (caddr_t *) &otherwin) == XCSUCCESS))
		     ? otherwin->frame : cre->above);
      xwc.stack_mode = cre->detail;
      XConfigureWindow (dpy, Tmp_win->frame,
			cre->value_mask & (CWSibling | CWStackMode), &xwc);
    }
  
#ifdef SHAPE
  if (ShapesSupported)
  {
    int xws, yws, xbs, ybs;
    unsigned wws, hws, wbs, hbs;
    int boundingShaped, clipShaped;
    
    XShapeQueryExtents (dpy, Tmp_win->w,&boundingShaped, &xws, &yws, &wws,
			&hws,&clipShaped, &xbs, &ybs, &wbs, &hbs);
    Tmp_win->wShaped = boundingShaped;
  }
#endif /* SHAPE */
  
  /* Don't modify frame_XXX fields before calling SetupWindow! */
  x = Tmp_win->frame_x;
  y = Tmp_win->frame_y;
  width = Tmp_win->frame_width;
  height = Tmp_win->frame_height;
  
  /* for restoring */  
  if (cre->value_mask & CWBorderWidth) 
    {
      Tmp_win->old_bw = cre->border_width; 
    }
  /* override even if border change */
  
  if (cre->value_mask & CWX)
    x = cre->x - Tmp_win->boundary_width - Tmp_win->bw;
  if (cre->value_mask & CWY) 
    y = cre->y - Tmp_win->boundary_width - Tmp_win->title_height - Tmp_win->bw;
  if (cre->value_mask & CWWidth)
    width = cre->width + 2*Tmp_win->boundary_width;
  if (cre->value_mask & CWHeight) 
    height = cre->height+Tmp_win->title_height+2*Tmp_win->boundary_width;
  
  /*
   * SetupWindow (x,y) are the location of the upper-left outer corner and
   * are passed directly to XMoveResizeWindow (frame).  The (width,height)
   * are the inner size of the frame.  The inner width is the same as the 
   * requested client window width; the inner height is the same as the
   * requested client window height plus any title bar slop.
   */
  SetupFrame (Tmp_win, x, y, width, height,FALSE);
  KeepOnTop();
  
}

/***********************************************************************
 *
 *  Procedure:
 *      HandleShapeNotify - shape notification event handler
 *
 ***********************************************************************/
#ifdef SHAPE
void HandleShapeNotify (void)
{
  if (ShapesSupported)
  {
    XShapeEvent *sev = (XShapeEvent *) &Event;
  
    if (!Tmp_win)
      return;
    if (sev->kind != ShapeBounding)
      return;
    Tmp_win->wShaped = sev->shaped;
    SetShape(Tmp_win,Tmp_win->frame_width);
  }
}
#endif  /* SHAPE*/

/***********************************************************************
 *
 *  Procedure:
 *	HandleVisibilityNotify - record fully visible windows for
 *      use in the RaiseLower function and the OnTop type windows.
 *
 ************************************************************************/
void HandleVisibilityNotify()
{
  XVisibilityEvent *vevent = (XVisibilityEvent *) &Event;
  
  if(Tmp_win)
    {
      if(vevent->state == VisibilityUnobscured)
	Tmp_win->flags |= VISIBLE;
      else
	Tmp_win->flags &= ~VISIBLE;
      
      /* For the most part, we'll raised partially obscured ONTOP windows
       * here. The exception is ONTOP windows that are obscured by
       * other ONTOP windows, which are raised in KeepOnTop(). This
       * complicated set-up saves us from continually re-raising
       * every on top window */
      if(((vevent->state == VisibilityPartiallyObscured)||
	  (vevent->state == VisibilityFullyObscured))&&
	 (Tmp_win->flags&ONTOP)&&(Tmp_win->flags & RAISED))
	{
/*	  RaiseWindow(Tmp_win);
	  Tmp_win->flags &= ~RAISED;
*/	}
    }
}


/***************************************************************************
 *
 * Waits for next X event, or for an auto-raise timeout.
 *
 ****************************************************************************/
int My_XNextEvent(Display *dpy, XEvent *event)
{
  extern int fd_width, x_fd;
  fd_set in_fdset, out_fdset;
  Window targetWindow;
  int i, count, retval; 

  int *rPipePtr = readPipes;
  int *wPipePtr = writePipes;
  struct queue_buff_struct **queuePtr = pipeQueue; 

  DBUG ("My_XNextEvent", "Routine Entered");

  /*
   *  Do this IMMEDIATELY prior to select, to prevent any nasty
   *  queued up X events from just hanging around waiting to be
   *  flushed
   */

  if (XPending (dpy))
    {
    DBUG ("My_XNextEvent", "taking care of queued up events & returning");
    XNextEvent (dpy, event);
    StashEventTime (event);
    return 1;
    }

  FD_ZERO (&in_fdset);
  FD_SET (x_fd, &in_fdset); 
  FD_ZERO (&out_fdset);

  for (i=0; i<npipes; i++, wPipePtr++, rPipePtr++, queuePtr++) 
    {
    if (*rPipePtr >= 0) FD_SET (*rPipePtr, &in_fdset);
    if (*queuePtr) FD_SET (*wPipePtr, &out_fdset);
    } 


  DBUG ("My_XNextEvent", "no X events waiting - about to reap children");

  /*  
   * Zap all those zombies!
   */
  /*
   * If we get to here, then there are no X events waiting to be processed.
   * * Just take a moment to check for dead children.
   */

  ReapChildren ();

  DBUG ("My_XNextEvent", "waiting for module input/output");

#ifdef __hpux
  retval = select (fd_width, (int *) &in_fdset, (int *) &out_fdset, 0, NULL);
#else
  retval = select (fd_width, &in_fdset, &out_fdset, 0, NULL);
#endif

  if (retval <= 0) return 0;

  rPipePtr = readPipes;
  wPipePtr = writePipes;

  /* Check for module input */
  for (i=0; i<npipes; i++, rPipePtr++, wPipePtr++)
    {
    if (*rPipePtr >= 0)
      {
      if (FD_ISSET (*rPipePtr, &in_fdset))
        {
        count = read (*rPipePtr, &targetWindow, sizeof (Window));
        if (count > 0)
          {
          DBUG ("My_XNextEvent", "calling HandleModuleInput");
          HandleModuleInput (targetWindow, i);
          }
        else
          {
          DBUG ("My_XNextEvent", "calling KillModule");
          KillModule (i, 10);
          }
        }
      }
    if (*wPipePtr >= 0)
      {
      if (FD_ISSET (*wPipePtr, &out_fdset))
        {
        DBUG ("My_XNextEvent", "calling FlushQueue");
        FlushQueue (i);
        }
      }
    }

  DBUG ("My_XNextEvent", "leaving My_XNextEvent");
  return 0;
}

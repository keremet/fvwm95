/* FvwmBacker Module for Fvwm. 
 *
 *  Copyright 1994,  Mike Finger (mfinger@mermaid.micro.umn.edu or
 *                               Mike_Finger@atk.com)
 *
 * The author makes not guarantees or warantees, either express or
 * implied.  Feel free to use any contained here for any purpose, as long
 * and this and any other applicible copyrights are kept intact.

 * The functions in this source file that are based on part of the FvwmIdent
 * module for Fvwm are noted by a small copyright atop that function, all others
 * are copyrighted by Mike Finger.  For those functions modified/used, here is
 *  the full, origonal copyright:
 *
 * Copyright 1994, Robert Nation and Nobutaka Suzuki.
 * No guarantees or warantees or anything
 * are provided or implied in any way whatsoever. Use this program at your
 * own risk. Permission to use this program for any purpose is given,
 * as long as the copyright is kept intact. */

/* Modified to directly manipulate the X server if a solid color
 * background is requested. To use this, usr "-solid <color_name>"
 * as the command to be executed.
 *
 * A. Davison
 * Septmber 1994.
 */

#include <FVWMconfig.h>

#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/time.h>
#if defined ___AIX || defined _AIX || defined __QNX__ || defined ___AIXV3 || defined AIXV3 || defined _SEQUENT_
#include <sys/select.h>
#endif
#include <unistd.h>
#include <ctype.h>
#ifdef ISC /* Saul */
#include <sys/bsdtypes.h> /* Saul */
#endif /* Saul */
#include <stdlib.h>

#include "../../fvwm/module.h"
#include <fvwm/version.h>
#include "FvwmBacker.h"
#include "Mallocs.h"

#include <X11/Xlib.h>

unsigned long NameToPixel(char*, unsigned long);

typedef struct 
{
	int		type;		/* The command type. 			 */
						/* -1 = no command.              */
						/*  0 = command to be spawned 	 */
						/*  1 = a solid color to be set  */
	char*	cmdStr;		/* The command string (Type 0)   */
	unsigned long solidColor;
						/* A solid color after X parsing (Type 1) */
} Command;

Command *commands;
int DeskCount=0;

int Fvwm_fd[2];
int fd_width;

char *Module;

/* X Display information. */

Display* 	dpy;
Window		root;
int			screen;

FILE*	logFile;

/* Comment this out if you don't want a logfile. */

/* #define LOGFILE "/tmp/FvwmBacker.log"*/


void main(int argc, char **argv)
{
char *temp, *s;
	char*	displayName = NULL;

  commands=NULL;

  /* Save the program name for error messages and config parsing */
  temp = argv[0];
  s=strrchr(argv[0], '/');
  if (s != NULL)
    temp = s + 1;

  Module=temp;
  
  if((argc != 6)&&(argc != 7)) {
    fprintf(stderr,"%s Version %s should only be executed by fvwm!\n",Module,
      VERSION);
   exit(1);
  }

  Fvwm_fd[0] = atoi(argv[1]);
  Fvwm_fd[1] = atoi(argv[2]);

  /* Grab the X display information now. */

	dpy = XOpenDisplay(displayName);
	if (!dpy) 
	{
		fprintf(stderr, "%s:  unable to open display '%s'\n",
			Module, XDisplayName (displayName));
		exit (2);
	}
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	/* Open a log file if necessary */
#	ifdef LOGFILE
		logFile = fopen(LOGFILE,"a");
		fprintf(logFile,"Initialising FvwmBacker\n");
#	endif

  signal (SIGPIPE, DeadPipe);  

  /* Parse the config file */
  ParseConfig();

  fd_width = GetFdWidth();

  SetMessageMask(Fvwm_fd,M_NEW_DESK|M_CONFIG_INFO|M_END_CONFIG_INFO);


  /* Recieve all messages from Fvwm */
  EndLessLoop();
}

/******************************************************************************
  EndLessLoop -  Read until we get killed, blocking when can't read
******************************************************************************/
void EndLessLoop()
{
fd_set readset;
struct timeval tv;

  while(1) {
    FD_ZERO(&readset);
    FD_SET(Fvwm_fd[1],&readset);
    tv.tv_sec=0;
    tv.tv_usec=0;
#ifdef __hpux
    if (!select(fd_width,(int *)&readset,NULL,NULL,&tv)) {
      FD_ZERO(&readset);
      FD_SET(Fvwm_fd[1],&readset);
      select(fd_width,(int *)&readset,NULL,NULL,NULL);
    }
#else
    if (!select(fd_width,&readset,NULL,NULL,&tv)) {
      FD_ZERO(&readset);
      FD_SET(Fvwm_fd[1],&readset);
      select(fd_width,&readset,NULL,NULL,NULL);
    }
#endif

    if (!FD_ISSET(Fvwm_fd[1],&readset)) continue;
    ReadFvwmPipe();
  }
}

/******************************************************************************
  ReadFvwmPipe - Read a single message from the pipe from Fvwm
    Originally Loop() from FvwmIdent:
      Copyright 1994, Robert Nation and Nobutaka Suzuki.
******************************************************************************/
void ReadFvwmPipe()
{
  int count,total,count2=0,body_length;
  unsigned long header[HEADER_SIZE],*body;
  char *cbody;

  body = NULL;
  if((count = ReadFvwmPacket(Fvwm_fd[1],header,&body)) > 0)
    {
      ProcessMessage(header[1],body);
      free(body);
    }
}


/******************************************************************************
  ProcessMessage - Process the message coming from Fvwm
    Skeleton based on processmessage() from FvwmIdent:
      Copyright 1994, Robert Nation and Nobutaka Suzuki.
******************************************************************************/
void ProcessMessage(unsigned long type,unsigned long *body)
{
  char* color;
  char* tmp;

  if (type==M_NEW_DESK) 
    {
      if (body[0]>DeskCount || commands[body[0]].type == -1) 
	{
	  return;
	}
#ifdef LOGFILE
      fprintf(logFile,"Desk: %d\n",body[0]);
      fprintf(logFile,"Command type: %d\n",commands[body[0]].type);
      if (commands[body[0]].type == 0)
	fprintf(logFile,"Command String: %s\n",commands[body[0]].cmdStr);
      else if (commands[body[0]].type == 1)
	fprintf(logFile,"Color Number: %d\n",commands[body[0]].solidColor);
      else if (commands[body[0]].type == -1)
	fprintf(logFile,"No Command\n");
      else
	{
	  fprintf(logFile,"Illegal command type !\n");
	  exit(1);
	}
      fflush(logFile);
#	endif

      
      if (commands[body[0]].type == 1)
	{
	  /* Process a solid color request */
	  
	  XSetWindowBackground(dpy, root, commands[body[0]].solidColor);
	  XClearWindow(dpy, root);
	  XFlush(dpy);
	  /*	XSetWindowBackground(dpy, root, commands[body[0]].solidColor);
	   */
	  
#		ifdef LOGFILE
	  fprintf(logFile,"Color set.\n");
	  fflush(logFile);
#		endif
	}
      else if(commands[body[0]].cmdStr != NULL)
	{
	  system(commands[body[0]].cmdStr);
	}
    }
}


/***********************************************************************
  Detected a broken pipe - time to exit 
    Based on DeadPipe() from FvwmIdent:
      Copyright 1994, Robert Nation and Nobutaka Suzuki.
 **********************************************************************/
void DeadPipe(int nonsense)
{
  exit(1);
}

/******************************************************************************
  ParseConfig - Parse the configuration file fvwm to us to use
    Based on part of main() from FvwmIdent:
      Copyright 1994, Robert Nation and Nobutaka Suzuki.
******************************************************************************/
void ParseConfig()
{
  char line2[40];
  char *tline;
  
  sprintf(line2,"*%sDesk",Module);
  
  GetConfigLine(Fvwm_fd,&tline);

    while(tline != (char *)0) 
      {
	if(strlen(tline)>1) 
	  {
	    if(strncasecmp(tline,line2,strlen(line2))==0)
	      AddCommand(&tline[strlen(line2)]);
	  }
	GetConfigLine(Fvwm_fd,&tline);
      }
}

/******************************************************************************
AddCommand - Add a command to the correct spot on the dynamic array.
******************************************************************************/
void AddCommand(char *string)
{
char *temp;
int num;
  temp=string;
  while(isspace((unsigned char)*temp)) temp++;
  num=atoi(temp);
  while(!isspace((unsigned char)*temp)) temp++;
  while(isspace((unsigned char)*temp)) temp++;
  if (DeskCount<1) {
    commands=(Command*)safemalloc((num+1)*sizeof(Command));
    while(DeskCount<num+1) commands[DeskCount++].type= -1;
  }
  else {
    if (num+1>DeskCount) {
      commands=(Command*)realloc(commands,(num+1)*sizeof(Command));
      while(DeskCount<num+1) commands[DeskCount++].type= -1;
    }
  }
/*  commands[num]=(Command*)safemalloc(sizeof(Command));
*/

    /* Now check the type of command... */
    /* strcpy(commands[num],temp);*/

	if (strncmp(temp,"-solid",6)==0)
	{
		char* color;
		char* tmp;
		/* Process a solid color request */

		color = &temp[7];
		while (isspace((unsigned char)*color))
			color++;
		tmp= color;
		while (!isspace((unsigned char)*tmp))
			tmp++;
		*tmp = 0;
		commands[num].type = 1;
		commands[num].solidColor = NameToPixel(color, BlackPixel(dpy, screen));
#ifdef LOGFILE
		fprintf(logFile,"Adding color: %s as number %d to desk %d\n",
			color,commands[num].solidColor, num);
		fflush(logFile);
#endif
	}
	else
	{
#ifdef LOGFILE
		fprintf(logFile,"Adding command: %s to desk %d\n",temp, num);
		fflush(logFile);
#endif
		commands[num].type = 0;
		commands[num].cmdStr = (char *)safemalloc(strlen(temp)+1);
		strcpy(commands[num].cmdStr,temp);
	}

}

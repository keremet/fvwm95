/* 
 * Copyright (C) 1994 Mark Boyns (boyns@sdsu.edu) and
 *                    Mark Scott (mscott@mcd.mot.com)
 *
 * FvwmAudio version 1.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* ChangeLog

 * Fixed FvwmAudio to reflect the changes made to the module protocol.

 * Szijarto Szabolcs <saby@sch.bme.hu> provided FvwmSound code that used
 $HOSTDISPLAY for the rplay host.  The code has been added to FvwmAudio.

 * Fixed bugs reported by beta testers, thanks!

 * Builtin rplay support has been added to FvwmAudio.  This support is
 enabled when FvwmAudio is compiled with HAVE_RPLAY defined and when
 FvwmAudioPlayCmd is set to builtin-rplay.  I guess it's safe to say
 that FvwmSound is now obsolete. -- Mark Boyns 5/7/94
 
 * FvwmAudio is based heavily on an Fvwm module "FvwmSound" by Mark Boyns
 and the real credit for this really goes to him for the concept.
 I just stripped out the "rplay" library dependencies to allow generic
 audio play command support.

 */

/*
 * This module is based on FvwmModuleDebugger which has the following
 * copyright:
 *
 * This module, and the entire ModuleDebugger program, and the concept for
 * interfacing this module to the Window Manager, are all original work
 * by Robert Nation
 *
 * Copyright 1994, Robert Nation. No guarantees or warantees or anything
 * are provided or implied in any way whatsoever. Use this program at your
 * own risk. Permission to use this program for any purpose is given,
 * as long as the copyright is kept intact.
 */

/*
 * fvwm includes:
 */
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <FVWMconfig.h>
#include "../../fvwm/module.h"
#include <fvwm/version.h>
#include <fvwm/fvwmlib.h>     

/*
 * rplay includes:
 */
#ifdef HAVE_RPLAY
#include <rplay.h>
#endif

#define BUILTIN_STARTUP		MAX_MESSAGES
#define BUILTIN_SHUTDOWN	MAX_MESSAGES+1
#define BUILTIN_UNKNOWN		MAX_MESSAGES+2
#define MAX_BUILTIN		3

#define BUFSIZE			512

/* globals */
char	*MyName;
int	fd_width;
int	fd[2];
char	audio_play_cmd_line[BUFSIZE], audio_play_dir[BUFSIZE];
time_t	audio_delay = 0;	/* seconds */
#ifdef HAVE_RPLAY
int	rplay_fd = -1;
#endif

/* prototypes */
void	Loop(int *);
void	process_message(unsigned long,unsigned long *);
void	DeadPipe(int);
void	config(void);
void	done(int);
int     audio_play(short); 

/* define the message table */
char	*messages[MAX_MESSAGES+MAX_BUILTIN] =
{
	"new_page",
	"new_desk",
	"add_window",
	"raise_window",
	"lower_window",
	"configure_window",
	"focus_change",
	"destroy_window",
	"iconify",
	"deiconify",
	"window_name",
	"icon_name",
	"res_class",
	"res_name",
	"end_windowlist",
	"icon_location",
	"map",
	"error",
	"config_info",
	"end_config_info",
	"icon_file",
	"default_icon",
/* add builtins here */
	"startup",
	"shutdown",
	"unknown",
};

/* define the sound table  */
char	*sound_table[MAX_MESSAGES+MAX_BUILTIN];

#ifdef HAVE_RPLAY
/* define the rplay table */
RPLAY	*rplay_table[MAX_MESSAGES+MAX_BUILTIN];
#endif

void main(int argc, char **argv)
{
	char *temp, *s;

	if ((argc != 6)&&(argc != 7))
	{
		fprintf(stderr,"%s Version %s should only be executed by fvwm!\n",
			MyName, VERSION);
		exit(1);
	}

	/* Save our program  name - for error messages */
	temp = argv[0];
	s=strrchr(argv[0], '/');
	if (s != NULL)
		temp = s + 1;

	MyName = safemalloc(strlen(temp)+2);
	strcpy(MyName,"*");
	strcat(MyName, temp);

	/* Dead pipe == Fvwm died */
	signal (SIGPIPE, DeadPipe);  
  
	fd[0] = atoi(argv[1]);
	fd[1] = atoi(argv[2]);

	audio_play_dir[0] = '\0';
	audio_play_cmd_line[0] = '\0';

	/*
	 * Read the sound configuration.
	 */
	config();

	/*
	 * Play the startup sound.
	 */
	audio_play(BUILTIN_STARTUP);
	SendInfo(fd[0], "Nop", 0);
	Loop(fd);
}

/***********************************************************************
 *
 *  Procedure:
 *	config - read the sound configuration file.
 *
 ***********************************************************************/
void	config(void)
{
  char	*buf;
  char	*message;
  char	*sound;
  char	*p;
  int	i, found;
#ifdef HAVE_RPLAY
  char	host[128];
  int	volume = RPLAY_DEFAULT_VOLUME;
  int	priority = RPLAY_DEFAULT_PRIORITY;
#endif
  
  /*
   * Intialize all the sounds.
   */
  for (i = 0; i < MAX_MESSAGES+MAX_BUILTIN; i++) {
    sound_table[i] = NULL;
#ifdef HAVE_RPLAY
    rplay_table[i] = NULL;
#endif
  }
  
#ifdef HAVE_RPLAY
  strcpy(host, rplay_default_host());
#endif
  
  GetConfigLine(fd,&buf);
  while (buf != NULL)
    {
      buf[strlen(buf)-1] = '\0';
      if (buf[0] == '*')
	{
	  /*
	   * Search for *FvwmAudio.
	   */
	  if (strncasecmp(buf, MyName, strlen(MyName)) == 0)
	    {
	      p = strtok(buf, " \t");
	  
	      if (strcasecmp(p, "*FvwmAudioPlayCmd") == 0) {
		p = strtok(NULL, "\n"); /* allow parameters */
		if (p && *p) {
		  strcpy(audio_play_cmd_line, p);
		}
	      }
	      else if (strcasecmp(p, "*FvwmAudioDir") == 0) {
		p = strtok(NULL, " \t");
		if (p && *p) {
		  strcpy(audio_play_dir, p);
		}
	      }
	      else if (strcasecmp(p, "*FvwmAudioDelay") == 0) {
		p = strtok(NULL, " \t");
		if (p && *p) {
		  audio_delay = atoi(p);
		}
	      }
	  
#ifdef HAVE_RPLAY
	      /*
	       * Check for rplay configuration options.
	       */
	      else if (strcasecmp(p, "*FvwmAudioRplayHost") == 0)
		{
		  p = strtok(NULL, " \t");
		  if (p && *p)
		    {
		      /*
		       * Check for environment variables like $HOSTDISPLAY.
		       */
		      if (*p == '$')
			{
			  char	*c1, *c2;
			  c2 = host;
			  for (c1 = (char *)getenv(p+1); *c1 && (*c1 != ':'); c1++)
			    {
			      *c2++ = *c1;
			    }
			  *c2 = '\0';
			}
		      else
			{
			  strcpy(host, p);
			}
		    }
		}
	      else if (strcasecmp(p, "*FvwmAudioRplayVolume") == 0)
		{
		  p = strtok(NULL, " \t");
		  if (p && *p) {
		    volume = atoi(p);
		  }
		}
	      else if (strcasecmp(p, "*FvwmAudioRplayPriority") == 0)
		{
		  p = strtok(NULL, " \t");
		  if (p && *p) {
		    priority = atoi(p);
		  }
		}
#endif
	      /*
	       * *FvwmAudio <message_type> <audio_file>
	       */
	      else  {
		message = strtok(NULL, " \t");
		sound = strtok(NULL, " \t");
	    
		if (!message || !*message || !sound || !*sound)
		  {
		    continue;
		  }
	    
		found = 0;
	    
		for (i = 0; !found && i < MAX_MESSAGES+MAX_BUILTIN; i++)
		  {
		    if (strcasecmp(message, messages[i]) == 0) {
#ifdef HAVE_RPLAY
		      rplay_table[i] = rplay_create(RPLAY_PLAY);
		      rplay_set(rplay_table[i], RPLAY_APPEND,
				RPLAY_SOUND,	sound,
				RPLAY_PRIORITY,	priority,
				RPLAY_VOLUME,	volume,
				NULL);
#endif
		      sound_table[i]=safemalloc(strlen(sound)+1);
		      strcpy(sound_table[i],sound);
		      found++;
		    }
		  }
	      }
	    }
	}
      GetConfigLine(fd,&buf);
    }
  
#ifdef HAVE_RPLAY
  /*
   * Builtin rplay support is enabled when FvwmAudioPlayCmd == builtin-rplay.
   */
  if (strcasecmp(audio_play_cmd_line, "builtin-rplay") == 0)
    {
      rplay_fd = rplay_open(host);
      if (rplay_fd < 0)
	{
	  rplay_perror("rplay_open");
	  done(1);
	}
    }
#endif
}

/***********************************************************************
 *
 *  Procedure:
 *	Loop - wait for data to process
 *
 ***********************************************************************/
void Loop(int *fd)
{
	unsigned long	header[HEADER_SIZE], *body;
	char		*cbody;
	int		body_length,count,count2=0, total;
	time_t 		now, last_time = 0;
	unsigned long	code;
	
	while (1)
	{
		if ((count = read(fd[1],header,
				  HEADER_SIZE*sizeof(unsigned long))) > 0)
		{
			/*
			 * Ignore messages that occur during the delay
			 * period.
			 */
			now = time(0);
			if (now < (last_time + audio_delay))
			{
				continue;
			}
			last_time = now;
			
			if(header[0] == START_FLAG)
			{
				body_length = header[2]-HEADER_SIZE;
				body = (unsigned long *)
					safemalloc(body_length * sizeof(unsigned long));
				cbody = (char *)body;
				total = 0;
				while(total < body_length*sizeof(unsigned long))
				{
					if((count2=read(fd[1],&cbody[total],
						body_length*sizeof(unsigned long)-total)) >0)
						total += count2;
					else if(count2 < 0)
						DeadPipe(0);
				}

				/*
				 * code will equal the number of shifts in the
				 * base-2 header[1] number.  Could use log here
				 * but this should be fast enough.
				 */
				code = -1;
				while (header[1])
				{
					code++;
					header[1] >>= 1;
				}
				
				/*
				 * Play the sound.
				 */
				if (code >= 0 && code < MAX_MESSAGES)
				{
					audio_play(code);
				}
				else
				{
					audio_play(BUILTIN_UNKNOWN);
				}
				
				free(body);
			}
		}
		if(count <= 0)
			DeadPipe(1);
	}
}


/***********************************************************************
 *
 *  Procedure:
 *	SIGPIPE handler - SIGPIPE means fvwm is dying
 *
 ***********************************************************************/
void DeadPipe(int nonsense)
{
	done(0);
}

/***********************************************************************
 *
 *  Procedure:
 *	done - common exit point for FvwmAudio.
 *
 ***********************************************************************/
void	done(int n)
{
	audio_play(BUILTIN_SHUTDOWN);
	exit(n);
}

/***********************************************************************
 *
 * Procedure:
 *
 *    audio_play - actually plays sound from lookup table
 *
 **********************************************************************/
int audio_play(short sound) 
{
	static char buf[BUFSIZE];

#ifdef HAVE_RPLAY
	if (rplay_fd != -1)
	{
		if (rplay_table[sound])
		{
			if (rplay(rplay_fd, rplay_table[sound]) < 0)
			{
				rplay_perror("rplay");
			}
		}
		return 0;
	}
#endif
	if (sound_table[sound])
	{
		memset(buf,0,BUFSIZE);

		/*
		 * Don't use audio_play_dir if it's NULL or if the sound file
		 * is an absolute pathname.
		 */
		if (audio_play_dir[0] == '\0' || sound_table[sound][0] == '/')
		{
			sprintf(buf,"%s %s", audio_play_cmd_line, sound_table[sound]);
		}
		else
		{
			sprintf(buf,"%s %s/%s &", audio_play_cmd_line, audio_play_dir,
				sound_table[sound]);
		}
		return system(buf);
	}  

	return 1;
}





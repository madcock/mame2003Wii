#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <sys/stat.h>
#include <assert.h>

#include "libretro.h"
#include "osdepend.h"

#include "fileio.h"
#include "palette.h"
#include "common.h"
#include "mame.h"
#include "driver.h"

extern int16_t XsoundBuffer[2048];
extern char* systemDir;
extern char* romDir;
extern retro_log_printf_t log_cb;

#if defined(__CELLOS_LV2__) && !defined(__PSL1GHT__)
#include <unistd.h> //stat() is defined here
#define S_ISDIR(x) (x & CELL_FS_S_IFDIR)
#endif

#if 0
struct GameOptions
{
	mame_file *	record;			/* handle to file to record input to */
	mame_file *	playback;		/* handle to file to playback input from */
	mame_file *	language_file;	/* handle to file for localization */

	int		mame_debug;		/* 1 to enable debugging */
	int		cheat;			/* 1 to enable cheating */
	int 	gui_host;		/* 1 to tweak some UI-related things for better GUI integration */
	int 	skip_disclaimer;	/* 1 to skip the disclaimer screen at startup */
	int 	skip_gameinfo;		/* 1 to skip the game info screen at startup */

	int		samplerate;		/* sound sample playback rate, in Hz */
	int		use_samples;	/* 1 to enable external .wav samples */
	int		use_filter;		/* 1 to enable FIR filter on final mixer output */

	float	brightness;		/* brightness of the display */
	float	pause_bright;		/* additional brightness when in pause */
	float	gamma;			/* gamma correction of the display */
	int		color_depth;	/* 15, 16, or 32, any other value means auto */
	int		vector_width;	/* requested width for vector games; 0 means default (640) */
	int		vector_height;	/* requested height for vector games; 0 means default (480) */
	int		ui_orientation;	/* orientation of the UI relative to the video */

	int		beam;			/* vector beam width */
	float	vector_flicker;	/* vector beam flicker effect control */
	float	vector_intensity;/* vector beam intensity */
	int		translucency;	/* 1 to enable translucency on vectors */
	int 	antialias;		/* 1 to enable antialiasing on vectors */

	int		use_artwork;	/* bitfield indicating which artwork pieces to use */
	int		artwork_res;	/* 1 for 1x game scaling, 2 for 2x */
	int		artwork_crop;	/* 1 to crop artwork to the game screen */

	char	savegame;		/* character representing a savegame to load */
	int     crc_only;       /* specify if only CRC should be used as checksum */
	char *	bios;			/* specify system bios (if used), 0 is default */

	int		debug_width;	/* requested width of debugger bitmap */
	int		debug_height;	/* requested height of debugger bitmap */
	int		debug_depth;	/* requested depth of debugger bitmap */

	#ifdef MESS
	UINT32 ram;
	struct ImageFile image_files[MAX_IMAGES];
	int		image_count;
	int		(*mess_printf_output)(const char *fmt, va_list arg);
	int disable_normal_ui;

	int		min_width;		/* minimum width for the display */
	int		min_height;		/* minimum height for the display */
	#endif
};
#endif

int osd_init(void)
{
    return 0;
}

void osd_exit(void)
{

}


/******************************************************************************

	Sound

******************************************************************************/

static bool stereo;

int osd_start_audio_stream(int aStereo)
{
    stereo = (aStereo != 0);
    return (Machine->sample_rate / Machine->drv->frames_per_second);
}

int osd_update_audio_stream(INT16 *buffer)
{
   int i, samplerate_buffer_size;

   samplerate_buffer_size = (Machine->sample_rate / Machine->drv->frames_per_second);

   if(stereo)
      memcpy(XsoundBuffer, buffer, samplerate_buffer_size * 4);
   else
   {
      for (i = 0; i < samplerate_buffer_size; i ++)
      {
         XsoundBuffer[i * 2 + 0] = buffer[i];
         XsoundBuffer[i * 2 + 1] = buffer[i];
      }    
   }

   return (Machine->sample_rate / Machine->drv->frames_per_second);
}

void osd_stop_audio_stream(void)
{
}

void osd_set_mastervolume(int attenuation)
{
}

int osd_get_mastervolume(void)
{
    return 0;
}

void osd_sound_enable(int enable)
{
    memset(XsoundBuffer, 0, sizeof(XsoundBuffer));
}



/******************************************************************************

	File I/O

******************************************************************************/
static const char* const paths[] = {"raw", "rom", "image", "image_diff", "sample", "artwork", "nvram", "hs", "hsdb", "config", "inputlog", "memcard", "ss", "history", "cheat", "lang", "ctrlr", "ini"};

struct _osd_file
{
    FILE* file;
};

int osd_get_path_count(int pathtype)
{
    return 1;
}

int osd_get_path_info(int pathtype, int pathindex, const char *filename)
{
    char buffer[1024];
    struct stat statbuf;
#if defined(_WIN32)
   char slash = '\\';
#else
   char slash = '/';
#endif

    switch(pathtype)
    {
       case FILETYPE_ROM: /* ROM */
       case FILETYPE_IMAGE:
          /* removes the stupid restriction where we need to have roms in a 'rom' folder */
          snprintf(buffer, 1024, "%s%c%s", romDir, slash, filename);
          break;
       default:
          snprintf(buffer, 1024, "%s%c%s%c%s", systemDir, slash, paths[pathtype], slash, filename);
    }
 
#ifdef DEBUG_LOG
    fprintf(stderr, "osd_get_path_info (buffer = [%s]), (systemDir: [%s]), (path type dir: [%s]), (path type: [%d]), (filename: [%s]) \n", buffer, systemDir, paths[pathtype], pathtype, filename);
#endif

    if(stat(buffer, &statbuf) == 0)
        return (S_ISDIR(statbuf.st_mode)) ? PATH_IS_DIRECTORY : PATH_IS_FILE;
    
    return PATH_NOT_FOUND;
}

osd_file *osd_fopen(int pathtype, int pathindex, const char *filename, const char *mode)
{
   char buffer[1024];
   osd_file *out;
#if defined(_WIN32)
   char slash = '\\';
#else
   char slash = '/';
#endif

   switch(pathtype)
   {
      case 1: /* ROM */
         /* removes the stupid restriction where we need to have roms in a 'rom' folder */
         snprintf(buffer, 1024, "%s%c%s", romDir, slash, filename);
         break;
      default:
         snprintf(buffer, 1024, "%s%c%s%c%s", systemDir, slash, paths[pathtype], slash, filename);
   }

   if (log_cb)
      log_cb(RETRO_LOG_INFO, "osd_fopen (buffer = [%s]), (systemDir: [%s]), (path type dir: [%s]), (path: [%d]), (filename: [%s]) \n", buffer, systemDir, paths[pathtype], pathtype, filename);

   out = (osd_file*)malloc(sizeof(osd_file));
    
   if (osd_get_path_info(pathtype, pathindex, filename) == PATH_NOT_FOUND)
   {
       /* if path not found, create */
       char newPath[1024];
       snprintf(newPath, sizeof(newPath), "%s%c%s", systemDir, slash, paths[pathtype]);
       mkdir(newPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
   }
    
   out->file = fopen(buffer, mode);

   if(out->file == 0)
   {
      free(out);
      return 0;
   }
   return out;
}

int osd_fseek(osd_file *file, INT64 offset, int whence)
{
    return fseek(file->file, offset, whence);
}

UINT64 osd_ftell(osd_file *file)
{
    return ftell(file->file);
}

int osd_feof(osd_file *file)
{
    return feof(file->file);
}

UINT32 osd_fread(osd_file *file, void *buffer, UINT32 length)
{
    return fread(buffer, 1, length, file->file);
}

UINT32 osd_fwrite(osd_file *file, const void *buffer, UINT32 length)
{
    return fwrite(buffer, 1, length, file->file);
}

void osd_fclose(osd_file *file)
{
    fclose(file->file);
    free(file);
}


/******************************************************************************

	Miscellaneous

******************************************************************************/

int osd_display_loading_rom_message(const char *name,struct rom_load_data *romdata){return 0;}
void osd_pause(int paused){}

void CLIB_DECL osd_die(const char *text,...)
{
   if (log_cb)
      log_cb(RETRO_LOG_INFO, text); 

    // TODO: Don't abort, switch back to main thread and exit cleanly: This is only used if a malloc fails in src/cpu/z80/z80.c so not too high a priority
    abort();
}

#include "cpuidh.h"
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

char    configdata[2][1024];
char    timeday[30];
char    idString1[100] = " ";
char    idString2[100] = " ";
double  theseSecs = 0.0;
double  startSecs = 0.0;
double  secs;
double  ramGB;

int CPUconf;
int CPUavail;

unsigned int millisecs  = 0;

#include <sys/sysinfo.h> 
#include <sys/utsname.h> 

struct timespec tp1;

void getSecs()
{
  clock_gettime(CLOCK_REALTIME, &tp1);

  theseSecs =  tp1.tv_sec + tp1.tv_nsec / 1e9;           
  return;
}

void start_time()
{
  getSecs();
  startSecs = theseSecs;
  return;
}

void end_time()
{
  getSecs();
  secs = theseSecs - startSecs;
  millisecs = (int)(1000.0 * secs);
  return;
}    


/****************************************************************************
**			TAU Portable Profiling Package			   **
**			http://www.acl.lanl.gov/tau		           **
*****************************************************************************
**    Copyright 1997  						   	   **
**    Department of Computer and Information Science, University of Oregon **
**    Advanced Computing Laboratory, Los Alamos National Laboratory        **
****************************************************************************/
/*********************************************************************/
/*                  pC++/Sage++  Copyright (C) 1993,1995             */
/*  Indiana University  University of Oregon  University of Rennes   */
/*********************************************************************/


/*
 * pprof.c : parallel profile data files printer
 * Modified by Sameer Shende (sameer@cs.uoregon.edu)
 * (c) 1993 Jerry Manic Saftware
 *
 * Version 1.0
 * Version 2.0  added collection profiling
 * Version 2.1  added summary printing
 * Version 2.2  added collection name and types
 * Version 2.3  machine-independent profile data file format
 * Version 2.4  racy support (dump format)
 * Version 2.5  switched to ASCII files to be really portable
 * Version 2.6  Added support for HPC+++ (n,c,t) , aggregates
 * Version 2.7  Added support for Dynamic profiling 
 * Version 2.8  Added no. of subroutines data 
 * Version 2.9  Added no. of subrs and stddev to dump mode
 * Version 2.91 Separate column heading for stddev in dump mode, 
 *              delete stddev column for mean
 */

#ifdef TAU_WINDOWS
#pragma warning( disable : 4786 )
#endif


# include <stdio.h>
# include <stdlib.h>

# include <errno.h>
# include <string.h>
# ifdef SOL2CC
#define TAUERRNO ::errno
#define qsort(a, b, c, d) std::qsort(a, (unsigned) b, (unsigned) c, d)
# else // SOL2CC
#define TAUERRNO errno
# endif // SOL2CC

#ifdef TAU_DOT_H_LESS_HEADERS 
# include <iostream>
# include <map>
using namespace std;
#else 
# include <iostream.h>
# include <map.h>
#endif /* TAU_DOT_H_LESS_HEADERS */
# include <stdlib.h>
#if (!defined(TAU_WINDOWS))
# include <unistd.h>
#endif //TAU_WINDOWS
# include <string.h>
# include <fcntl.h>
# include <limits.h>
# include <math.h>
#ifdef TAU_WINDOWS
# include <float.h>
#endif //TAU_WINDOWS

#ifdef COMPAQ_ALPHA 
# include <float.h>
#endif /* COMPAQ_ALPHA */
#ifdef KAI
# include <algobase>
using namespace std;
#endif
#ifdef POOMA_TFLOP 
extern "C" int getopt(int, char *const *, const char *);
extern char *optarg;
extern int optind, opterr, optopt;
#endif

#ifdef FUJITSU
#include <Profile/fujitsu.h>
#endif /* FUJITSU */

#ifndef DBL_MIN
#include <float.h>
#endif 


/* Macros for min/max. To avoid <algobase> and gcc-3.0 problems, we define: */
#define TAU_MIN(a,b) (((a)<(b))?(a):(b))
#define TAU_MAX(a,b) (((a)>(b))?(a):(b))


# ifndef TRUE
#   define FALSE 0
#   define TRUE  1
# endif

# if defined(ultrix) || defined(sequent) || defined(butterfly) || defined(GNU)
double fmod (double x, double y)
{
  return ( x - floor(x/y) * y );
}
# endif

static struct p_func_descr {
#ifdef USE_LONG
    long     numcalls;
    long     numsubrs;
#else // DEFAULT double 
    double     numcalls;
    double     numsubrs;
#endif // USE_LONG
    double  usec;
    double  cumusec;
    double  stddeviation;
} *p_func_list = 0;    /* -- function profile data -- */

static struct p_prof_elem {
    char   *name;
    int     tag;
#ifdef USE_LONG
    long     numcalls;
    long     numsubrs;
#else // DEFAULT double 
    double     numcalls;
    double     numsubrs;
#endif //USE_LONG 
    double  usec;
    double  cumusec;
    double  stddeviation;
} *p_prof_tbl  = 0,    /* -- function profile data table -- */
  *p_min_tbl = 0,      /* -- minimal summary function profile data table -- */
  *p_max_tbl = 0,      /* -- maximal summary function profile data table -- */
  *p_total_tbl = 0;    /* -- summary function profile data table -- */

/* MIMD extension to identify top level function in each thread */
int top_level_function; /* id for the top level function */
double max_thread_cumusec = 0.0; 
static struct p_coll_descr {
    int numelem;
    int dim;
    int size;
    int localacs;
    int remoteacs;
    char *collname;
    char *elemname;
    char *varname;
} *p_coll_list = 0,    /* -- collection profile data table -- */
  *p_coll_tbl  = 0;    /* -- summary collection profile data table -- */

#define MAX_COUNTERS  1024  	/* -- HPC++ object profiling */
#define SIZE_OF_LINE  64*1024   /* Big line to accomodate long function names */
#define SIZE_OF_FILENAME 1024   /* Include full path. */

static struct p_aggr_descr {
    int numelem;
    int dim;
    int size;
    long counters[MAX_COUNTERS];
    long total_events; /* sum of counters */
    char *container_name;
    char *container_type;
    char *var_name;
} *p_aggr_list = 0,    /* -- aggregate profile data table -- */
  *p_aggr_tbl  = 0;    /* -- summary aggregate profile data table -- */

static int numfunc;          /* -- number of functions   -- */
static int filledDBThr, filledDBCtx = 0;     /* flag  for iterating over no,ctx,thr */
static char *depfile;        /* -- corresponding .dep file -- */
static char proffile[256];   /* -- profile data file     -- */
static char **funcnamebuf;   /* -- function name table   -- */
static int *functagbuf;      /* -- function tag table    -- */
static int numcoll;          /* -- number of collections -- */
static char **eventnamebuf = 0;  /* -- event name table     -- */
static int numevents;	      /* -- total counters used  -- */
static int numaggr;          /* -- number of aggregates  -- */
static double total_total;   /* -- total time overall    -- */
static double min_total;
static double max_total;
static int nodeprint = TRUE;
static int dump = FALSE;
static int dumpminmax = FALSE;
static int list = FALSE;
static char lbuf[256];        /* -- temporary line buffer for reads -- */
static char sbuf[128];        /* -- temporary string buffer -- */
static int  hpcxx_flag = FALSE;
static int  hwcounters = false;
static int  userevents = false;
static int  profilestats = false; /* for SumExclSqr */
static int  files_processed = 0; /* -- used for printing summary -- */

/************** Function Declarations *********************************/

int FunctionSummaryInfo(int no, int ctx, int thr, int max); 
static char *strsave (const char *s); /* defined later */
static int MsecCmp (const void *left, const void *right);
static int CumMsecCmp (const void *left, const void *right);
static int StdDevCmp (const void *left, const void *right);
static int CallCmp (const void *left, const void *right);
static void DumpFuncTab (struct p_prof_elem *tab, char *id_str, double total,
                         int max, char *order); 
static void PrintFuncTab (struct p_prof_elem *tab, double total, int max);
static void ProcessUserEventData(FILE *fp, int no, int ctx, int thr, int max);
static void UserEventSummaryInfo(int node, int ctx, int thr);
/**************** static var ******************************************/
static int (* compar)(const void *, const void *) = CumMsecCmp;
/********************** Dynamic Profiling Data Structures **************/

struct ltstr
{
  bool operator()(const char* s1, const char* s2) const
  {
    return strcmp(s1, s2) < 0;
  }
};

class FunctionData {
  public :
#ifdef USE_LONG 
    long numcalls;
    long numsubrs;
#else // DEFAULT double 
    double     numcalls;
    double     numsubrs;
#endif // USE_LONG
    double excl;
    double incl;
    double stddeviation;
#ifdef USE_LONG 
  FunctionData(long nc, long ns, double ex, double in, double sigma) 
    : numcalls(nc), numsubrs(ns),  excl(ex), incl(in), stddeviation(sigma) { }
#else // DEFAULT double
  FunctionData(double nc, double ns, double ex, double in, double sigma) 
    : numcalls(nc), numsubrs(ns),  excl(ex), incl(in), stddeviation(sigma) { }
#endif // USE_LONG 
  FunctionData() {
    numcalls = numsubrs = 0;
    excl = incl = stddeviation = 0;
  }
  FunctionData(const FunctionData& X) {
    numcalls = X.numcalls;
    numsubrs = X.numsubrs;
    excl  = X.excl;
    incl  = X.incl;
    stddeviation = X.stddeviation; 
  }
  FunctionData& operator= (const FunctionData& X) {
    numcalls = X.numcalls;
    numsubrs = X.numsubrs;
    excl = X.excl;
    incl = X.incl;
    stddeviation = X.stddeviation;
    return *this;
  }
  FunctionData& operator+= (const FunctionData& X) {
    numcalls += X.numcalls;
    numsubrs += X.numsubrs;
    excl += X.excl;
    incl += X.incl;
    stddeviation += X.stddeviation;
    return *this;
  }
 ~FunctionData() { }
};
/* GLOBAL database of function names */
map<const char*, FunctionData, ltstr> funcDB;


class UserEventData {
  public :
    double     numevents;
    double     maxvalue;
    double     minvalue;
    double     meanvalue;
    double     sumsqr;
  UserEventData(double ne, double maxv, double minv, double meanv, 
	double sumsqrv) 
    : numevents(ne), maxvalue(maxv), minvalue(minv), meanvalue(meanv),
      sumsqr(sumsqrv) { }
  UserEventData() {
    numevents = meanvalue = sumsqr = 0;
    maxvalue  = DBL_MIN;
    minvalue  = DBL_MAX;
  }
  UserEventData(const UserEventData& X) 
    : numevents(X.numevents), maxvalue(X.maxvalue), minvalue(X.minvalue),
      meanvalue(X.meanvalue), sumsqr(X.sumsqr) { }
  UserEventData& operator= (const UserEventData& X)  
  {
    numevents 	= X.numevents;
    maxvalue 	= X.maxvalue;
    minvalue 	= X.minvalue;
    meanvalue	= X.meanvalue;
    sumsqr	= X.sumsqr;
    return *this;
  }
  UserEventData& operator+= (const UserEventData& X) {
  maxvalue	= TAU_MAX (maxvalue, X.maxvalue);
  minvalue	= TAU_MIN (minvalue, X.minvalue);
    if (numevents+X.numevents != 0) 
    {
      meanvalue 	= (meanvalue*numevents + X.meanvalue * X.numevents)/(numevents+X.numevents); 
    }
    else 
      meanvalue = 0;
    numevents 	+= X.numevents;
    sumsqr	+= X.sumsqr;

    return *this;
  }
 ~UserEventData() { }
};
/* GLOBAL database of user event names */
map<const char*, UserEventData, ltstr> userEventDB;


bool IsDynamicProfiling(char *filename) 
{
  FILE *fp;
  char error_msg[SIZE_OF_FILENAME], version[64];
  int numberOfFunctions;
 
  if ((fp = fopen(filename, "r")) == NULL) {
    sprintf(error_msg,"Error: Could not open %s",filename);
    perror(error_msg);
    return false;
  }
  if (fscanf(fp, "%d %s",&numberOfFunctions, version) == EOF) {
    printf("Error: fscanf returns EOF file %s", filename);
    return false;
  }
  fclose(fp); // thats all we wanted to read 

  if (strcmp(version,"templated_functions") == 0) { // correct version
    hwcounters = false; // Timing data is in the profile files  
    return true;
  }
  else  { 
    if (strcmp(version,"templated_functions_hw_counters") == 0) {
      hwcounters  = true; // Counters - do not use time string formatting   
      return true; // It is dynamic profiling 	
    } 
    else // Neither  - static profiling 
      return false;
  }
}

  
int InitFuncNameBuf(void)
{
  int i;
  map<const char*, FunctionData, ltstr>::iterator it;

  funcnamebuf = (char **) malloc (numfunc * sizeof(char *));
  if (funcnamebuf == NULL) 
  {  
    perror("Error: Out of Memory : malloc returns NULL ");
    exit (1);
  }
  functagbuf  = (int *) malloc (numfunc * sizeof(int));
  if (functagbuf == NULL) 
  {  
    perror("Error: Out of Memory : malloc returns NULL ");
    exit (1);
  }

  for(it=funcDB.begin(), i = 0; it!=funcDB.end(); it++, i++)
  {
    funcnamebuf[i] = strsave((*it).first);
    functagbuf[i]  = i; /* Default - give tags that are 0 to numfunc - 1 */
    /* This is because we don't have a dep file in dynamic profiling */
  }

  return TRUE;
}
  


int FillFunctionDB(int node, int ctx, int thr, char *prefix)
{
  char line[SIZE_OF_LINE]; // In case function name is *really* long - templ. args
  char func[SIZE_OF_LINE]; // - do - 
  char version[64],filename[SIZE_OF_FILENAME]; // double check?
  int numberOfFunctions, i, j, k;
  char header[256], trailer[256]; // Format string 
  int hlen, tlen; 
#ifdef USE_LONG 
  long numcalls,numsubrs,numinvocations;
#else // DEFAULT double 
    double     numcalls;
    double     numsubrs;
    double     numinvocations; 
#endif // USE_LONG
  double excl, incl, exclthiscall, inclthiscall, sumexclsqr;
  bool dontread = false;
  int numberOfUserEvents;
  FILE *fp;
  char *functionName; //need a separate string otherwise it stores only one ptr.
  char *userEventName; //need a separate string otherwise it stores only one ptr
  map<const char*, FunctionData, ltstr>::iterator it;
  map<const char*, UserEventData, ltstr>::iterator uit;


  
  sprintf(filename,"%s.%d.%d.%d",prefix, node, ctx, thr);

#ifdef DEBUG
  printf("Inside FillFunctionDB : Filename %s\n",filename);
#endif /* DEBUG */

  if ((fp = fopen(filename, "r")) == NULL) {
    /* comment this line later */
#ifdef DEBUG /* In sweeping through n,c,t tree its ok if a file is not found */
    sprintf(line,"Error: Could not open file %s", filename);
    perror(line);
#endif /* DEBUG */
    return 0;
  }
  
#ifdef DEBUG 
  cout << "Inside FillFunctionDB  n " << node << " c " << ctx << " thr " << thr << endl;
#endif /* DEBUG */
  filledDBThr++; /* Set flag to indicate that some work was done */
  filledDBCtx++; /* Set flag to indicate that some work was done */
  
  if (fgets(line, sizeof(line), fp) == NULL) { 
    perror("Error: fgets returns NULL ");
    return 0;
  }
  sscanf(line,"%d %s", &numberOfFunctions, version);
  // double check - just to be sure 
  if (strncmp(version,"templated_functions",strlen("templated_functions")) != 0 ) { 
   // Neither templated_functions nor templated_functions_hw_counters
    printf("Incorrect version in file %s : %s", filename, version);
    return 0;
  }

  // New Data format contains a string like 
  // "# Name Calls Subrs Excl Incl SumExclSqr ProfileCalls"
  if (fgets(line, sizeof(line), fp) == NULL) {
    perror("Error: fgets returns NULL in format string ");
    return 0;
  }
  if (line[0] == '#') { // new data format 
    sprintf(header,"# Name Calls Subrs Excl Incl ");
    hlen = strlen(header);
    if (strncmp(line, header, hlen) != 0) {
      printf("Error in reading Format String : Expected %s Got %s\n", 
	header, line);
      exit(1);
    } else { // Format string parsed correctly. See if PROFILE_STATS is on
      sprintf(trailer,"SumExclSqr ProfileCalls");
      tlen = strlen(trailer);
      if (strncmp(line+hlen, trailer, tlen) == 0) {
	profilestats = true;
      } else { // doesn't contain SumExclSqr 
        sprintf(trailer, "ProfileCalls");
	tlen = strlen(trailer);
	if (strncmp(line+hlen, trailer, tlen) == 0) {
	  profilestats = false;
	} else { // neither matched! 
          printf("Error in reading Format String : Got %s\n", line);
	  exit(1);
	} // trailer matches 
      } // doesn't contain SumExclSqr 
#ifdef DEBUG
      printf("Format String correct ProfileStats is %d\n", profilestats);
#endif /* DEBUG */ 
    } // Format String available  
  } // First char is '#'
  else { // Old data format! # is not there! 
    profilestats = false;
    dontread = true; //already read a data line - process that first!
  } // line[0] = '#' 



  for (i =0; i < numberOfFunctions; i++) {
    if((i == 0) && (dontread == true)) { //skip 
    } else { 
      if (fgets(line, SIZE_OF_LINE, fp) == NULL) {
        perror("Error in fgets: Cannot read function table");
        return 0;
      }
    }
    // line[0] has '"' - start loop from 1 to get the entire function name
    for (j=1; line[j] != '"'; j++) {
        func[j-1] = line[j];
    }
    func[j-1] = '\0'; // null terminate the string
    // At this point line[j] is '"' and the has a blank after that, so
    // line[j+1] corresponds to the beginning of other data.
    if (!profilestats) { // SumExclSqr is not there 
#ifdef USE_LONG 
      sscanf(&line[j+1], "%ld %ld %lG %lG %ld", &numcalls, &numsubrs, &excl, &incl, &numinvocations);
#else // DEFAULT double
      sscanf(&line[j+1], "%lG %lG %lG %lG %lG", &numcalls, &numsubrs, &excl, &incl, &numinvocations);
#endif // USE_LONG 
    } else { // SumExclSqr is there.
#ifdef USE_LONG 
      sscanf(&line[j+1], "%ld %ld %lG %lG %lG %ld", &numcalls, &numsubrs, &excl, &incl, &sumexclsqr, &numinvocations);
#else // DEFAULT double
      sscanf(&line[j+1], "%lG %lG %lG %lG %lG %lG", &numcalls, &numsubrs, &excl, &incl, &sumexclsqr, &numinvocations);
#endif // USE_LONG 
    } // profilestats 
#ifdef DEBUG
    cout << "func = "<< func << endl;
    cout << "numcalls = "<< numcalls << " numsubrs = "<< numsubrs << " excl = "<< excl <<" incl = " << incl << " no profiled invocations " << numinvocations << endl;
#endif /* DEBUG */

    functionName = new char[strlen(func)+1]; // create a new storage - STL req.
    strcpy(functionName,func);

    if ((it = funcDB.find((const char *)functionName)) != funcDB.end()) { 
#ifdef DEBUG 
      cout << "Found the name " << functionName << endl;
#endif /* DEBUG */
      delete functionName; // don't need this if its already there.
    }
    else 
      funcDB[(const char *)functionName] = FunctionData(); 
      // adds  a null record and creates the name key in the map
      // Note: don't delete functionName - STL needs it
	/* PROCESS NO. OF Invocations Profiled */
    for(k = 0; k < numinvocations; k++) {
      if(fgets(line,SIZE_OF_LINE,fp) == NULL) {
	perror("Error in fgets: Cannot read invocation data ");
	return 0;
      }
      /* use this data */
      sscanf(line, "%lG %lG", &exclthiscall, &inclthiscall);
#ifdef DEBUG
      cout << "func = " << func << " ExclThisCall = " << exclthiscall << " InclThisCall = " << inclthiscall << endl;
#endif /* DEBUG */
    }
  }
/* Now look at filling the userEventDB */
  if ( fgets (line, 256, fp) == NULL ) {
    fprintf (stderr,
                  "invalid proftablefile: cannot read number of collections\n");
    exit (1);
  }
  sscanf(line, "%d %s", &numcoll, version);
   // WRITE CODE TO SUPPORT AGGREGATES HERE
  if ( fgets (line, 256, fp) != NULL) {
      // If userevent data is available, process it.
      sscanf(line, "%d %s", &numberOfUserEvents, version);
      if (strcmp(version, "userevents") == 0) /* User events */
      {
        userevents = true;
      }
      else
      { // Hey! What data did we read?
#ifdef DEBUG 
        printf("Unable to process data read: %s\n", line);
        printf("You're probably using an older version of this tool. \
          Please upgrade\n");
        fclose(fp);
#endif /* DEBUG */
        return 0;
      }
      // First read the comment line 
      // Read the user events
    if ( fgets (line, 256, fp) != NULL) {
      if (line[0] != '#')
      { // everything is fine  read # eventname numevents max min mean sumsqr
        // line contains the data for user events at this stage 
        printf("Possible error in data format read: %s\n", line);
        fclose(fp);
        return 0;
      }
  
      // Got the # line and now for the real user data 
      for (i =0; i < numberOfUserEvents; i++) {
        if (fgets(line, SIZE_OF_LINE, fp) == NULL) {
          perror("Error in fgets: Cannot read user event table");
          return 0;
        }
        // line[0] has '"' - start loop from 1 to get the entire function name
        for (j=1; line[j] != '"'; j++) {
          func[j-1] = line[j];
        }
        func[j-1] = '\0'; // null terminate the string
        // At this point line[j] is '"' and the has a blank after that, so
        // line[j+1] corresponds to the beginning of other data.
  
        userEventName = new char[strlen(func)+1]; // create a new storage - STL req.
        strcpy(userEventName,func);
  
        if ((uit = userEventDB.find((const char *)userEventName)) != userEventDB.end()) {
  #ifdef DEBUG
          cout << "FOUND the name " << userEventName << endl;
  #endif /* DEBUG */
          delete userEventName; // don't need this if its already there.
        }
        else
        {
          userEventDB[(const char *)userEventName] = UserEventData();
          // adds  a null record and creates the name key in the map
          // Note: don't delete userEventName - STL needs it
  #ifdef DEBUG
  	printf("ADDED UserEventName %s to the userEventDB\n", userEventName);
  #endif /* DEBUG */
        }
  
  #ifdef DEBUG
      printf("User Events read %s \n", line);
  #endif /* DEBUG */
  /* at this stage, the user event data should be read and userEventDB should 
  be filled in */    
      } /* All n user event data lines have been processed */
    } /* read the first line after n userevents. It contains # event... */ 
    else 
    { /* EOF encountered */
      fclose(fp);
      return 0;
    } /* data processed */ 
  } /* userevent data not found */


  fclose(fp);
  return 1;
}

/* to iterate over nodes, contexts and threads */
int FillFunctionDBInContext(int node, int ctx, int thr, char *prefix)
{
#ifdef DEBUG
  cout << "FillFunctionDBInContext n" << node << " c " << ctx << " t " << thr << endl;
#endif /* DEBUG */
  for(thr = 0,filledDBThr = 0; FillFunctionDB(node, ctx, thr, prefix); thr++);
  if (filledDBThr) /* did some work */
    return TRUE;
  else
    return FALSE;
}

int FillFunctionDBInNode (int node, int ctx, int thr, char *prefix) 
{
#ifdef DEBUG
  cout << "FillFunctionDBInNode n" << node << " c " << ctx << " t " << thr << endl;
#endif /* DEBUG */
  for(ctx = 0, filledDBCtx = 0; FillFunctionDBInContext(node, ctx, thr, prefix); ctx ++);
  if (filledDBCtx) 
    return TRUE;
  else
    return FALSE;
  /* This allows for iteration over 0.0.0, 0.0.1, 0.1.0. 1.0.0 etc. */
}

  
int DumpFunctionNamesInDB(void) 
{ /* For debugging */
  map<const char*, FunctionData, ltstr>::iterator it;
  
  for(it=funcDB.begin(); it != funcDB.end(); it++) {
    cout <<(*it).first << endl; /* Just print the name */
  }
  return 1;
}

int DumpFtabFile(char *prefix)
{ /* Racy needs a .ftab file - since one is not available in dynamic profiling,
     we need to generate it here */
  FILE *fp;
  char filename[SIZE_OF_FILENAME], error_msg[SIZE_OF_FILENAME];
  int i;
  /* Since funcnamebuf and functagbuf have already been initialized, use them */

  sprintf(filename, "%s.ftab",prefix); /* "profile.ftab"  */

  /* create this file only if its not there */
  if ((fp = fopen(filename, "r")) != NULL) { /* its already there */
    fclose (fp);
    return 1;
  }
  if (TAUERRNO == ENOENT) /* The file does not exist - create it! */
    fclose(fp); /* reopen in another mode */
 
  if ((fp = fopen(filename, "w+")) == NULL) {
    sprintf(error_msg, "Error : Could not create %s", filename);
    perror(error_msg);
    return 0;
  }

  fprintf(fp,"default.dep\n%d templated_functions\n",numfunc);
  for (i = 0; i < numfunc; i++) {
    fprintf(fp,"%d %s\n", functagbuf[i], funcnamebuf[i]);
  }
  fflush(fp);
  fclose (fp);
  return 1;
}

int ProcessFileDynamic(int node, int ctx, int thr, int max, char *prefix)
{
  char line[SIZE_OF_LINE]; // In case function name is *really* long - templ. args
  char func[SIZE_OF_LINE]; // - do - 
  char version[64],filename[SIZE_OF_FILENAME]; // double check?
  int numberOfFunctions, i, j, k;
#ifdef USE_LONG 
  long numcalls, numsubrs, numinvocations;
#else // DEFAULT double
  double numcalls, numsubrs, numinvocations;
#endif // USE_LONG 
  double excl, incl, exclthiscall, inclthiscall, sumexclsqr, stddev;
  bool dontread = false;
  FILE *fp;
  map<const char*, FunctionData, ltstr>::iterator it;
  int numberOfUserEvents;


  sprintf(filename,"%s.%d.%d.%d",prefix, node, ctx, thr);


  if ((fp = fopen(filename, "r")) == NULL) {
    /* comment this line later */
#ifdef DEBUG /* In sweeping through n,c,t tree its ok if a file is not found */
    sprintf(line,"Error: Could not open file %s", filename);
    perror(line);
#endif /* DEBUG */
    return 0;
  }
  
#ifdef DEBUG
  printf("Inside ProcessFileDynamic : Filename %s\n",filename);
#endif /* DEBUG */

  filledDBThr++; /* Set flag to indicate that some work was done */
  filledDBCtx++; /* Set flag to indicate that some work was done */

  if (fgets(line, sizeof(line), fp) == NULL) {
    perror("Error: fgets returns NULL ");
    return 0;
  }
  sscanf(line,"%d %s", &numberOfFunctions, version);
  // double check - just to be sure
  if (strncmp(version,"templated_functions",strlen("templated_functions")) != 0 ) { 
   // Neither templated_functions nor templated_functions_hw_counters
    printf("Incorrect version in file %s : %s", filename, version);
    return 0;
  }


  
  // New Data format contains a string like 
  // "# Name Calls Subrs Excl Incl SumExclSqr ProfileCalls"
  if (fgets(line, sizeof(line), fp) == NULL) {
    perror("Error: fgets returns NULL in format string ");
    return 0;
  }
  if (line[0] != '#') { // Old data format without '#' 
    profilestats = false;
    dontread = true; // have already read a valid data line
  } 
  // We've already parsed the options correctly in FillFunctionDB



  /* Before reading the data, initialize the map for the function */
  for(it = funcDB.begin(); it != funcDB.end(); it++) {
    (*it).second = FunctionData(); /* initialized to null values */
  } /* This ensures that data from two files doesn't interfere */
  
  /* Main loop of reading the function information, line by line */
  for (i =0; i < numberOfFunctions; i++) {
    if ( (i==0) && (dontread == true)) { //skip 
    } else {
      if (fgets(line, SIZE_OF_LINE, fp) == NULL) {
        perror("Error in fgets: Cannot read function table");
        return 0;
      }
    }
    // line[0] has '"' - start loop from 1 to get the entire function name
    for (j=1; line[j] != '"'; j++) {
        func[j-1] = line[j];
    }
    func[j-1] = '\0'; // null terminate the string
    // At this point line[j] is '"' and the has a blank after that, so
    // line[j+1] corresponds to the beginning of other data.

    if (!profilestats) { // SumExclSqr is not there 
#ifdef USE_LONG 
      sscanf(&line[j+1], "%ld %ld %lG %lG %ld", &numcalls, &numsubrs, &excl, &incl, &numinvocations);
#else // DEFAULT double
      sscanf(&line[j+1], "%lG %lG %lG %lG %lG", &numcalls, &numsubrs, &excl, &incl, &numinvocations);
#endif // USE_LONG 
      stddev = 0; // Not defined in this case 
    } else { // SumExclSqr is there.
#ifdef USE_LONG 
      sscanf(&line[j+1], "%ld %ld %lG %lG %lG %ld", &numcalls, &numsubrs, &excl, &incl, &sumexclsqr, &numinvocations);
#else // DEFAULT double
      sscanf(&line[j+1], "%lG %lG %lG %lG %lG %lG", &numcalls, &numsubrs, &excl, &incl, &sumexclsqr, &numinvocations);
#endif // USE_LONG 
      // Calculate the standard deviation = sqrt((sumt^2)/N - mean^2)
      stddev = sqrt(fabs( (sumexclsqr/numcalls) - ((excl/numcalls) * (excl/numcalls))) );
#ifdef DEBUG
      cout << "stddeviation = "<< stddev << " sumexclsqr = "<< sumexclsqr << " func : "<< " excl " << excl<< " calls " << numcalls << func<< endl; 
#endif /* DEBUG */
    } // profilestats 

#ifdef DEBUG
    cout << "func = "<< func << endl;
    cout << "numcalls = "<< numcalls <<" numsubrs = "<< numsubrs<< " excl = "<< excl <<" incl = " << incl << " num invocations profiled = " << numinvocations << endl;
#endif /* DEBUG */

    if ((it = funcDB.find((const char *)func)) == funcDB.end()) {
      cout << "ERROR : In second pass ProcessFileDynamic didn't find name " << func << " in file "<< filename << endl;
      return 0;
    }
    funcDB[func] += FunctionData(numcalls, numsubrs, excl, incl, stddev);
    /* In case a function appears twice in the same file (templated function
    the user didn't specify exactly unique type - then add the data. Defaults
    to assignment as initialization cleans it up. */ 
	/* PROCESS NO. OF Invocations Profiled */
    for(k = 0; k < numinvocations; k++) {
      if(fgets(line,SIZE_OF_LINE,fp) == NULL) {
	perror("Error in fgets: Cannot read invocation data ");
	return 0;
      }
      /* use this data */
      sscanf(line, "%lG %lG", &exclthiscall, &inclthiscall);
#ifdef DEBUG
      cout << "func = " << func << " ExclThisCall = " << exclthiscall << " InclThisCall = " << inclthiscall << endl;
#endif /* DEBUG */
    }
  }

  p_func_list = (struct p_func_descr *) malloc(numfunc * sizeof(struct p_func_descr));

  max_thread_cumusec = 0.0; /* initialize */
  /* fill up the p_func_list */
  for(it=funcDB.begin(), i = 0; it != funcDB.end(); it++, i++) {
    p_func_list[i].numcalls 	= (*it).second.numcalls;
    p_func_list[i].numsubrs 	= (*it).second.numsubrs;
    p_func_list[i].usec 	= (*it).second.excl;
    p_func_list[i].cumusec 	= (*it).second.incl;
    p_func_list[i].stddeviation = (*it).second.stddeviation;

    if (p_func_list[i].cumusec > max_thread_cumusec) {
        top_level_function = i; /* to find which is the top level function
        in this thread we find the function with the max cumusec */
        max_thread_cumusec = p_func_list[i].cumusec;
    }
#ifdef DEBUG
#ifdef USE_LONG 
    printf("Func Id %d name %s numcalls %ld numsubrs %ld usec %lG cumusec %lG\n",i, funcnamebuf[i],  p_func_list[i].numcalls, p_func_list[i].numsubrs, p_func_list[i].usec, p_func_list[i].cumusec);
#else // DEFAULT double
    printf("Func Id %d name %s numcalls %lG numsubrs %lG usec %lG cumusec %lG\n",i, funcnamebuf[i],  p_func_list[i].numcalls, p_func_list[i].numsubrs, p_func_list[i].usec, p_func_list[i].cumusec);
#endif // USE_LONG 
#endif /* DEBUG */
  }
  /* -- read number of collections ------------------------------------------ */
  if ( fgets (line, 256, fp) == NULL ) {
    fprintf (stderr,
                  "invalid proftablefile: cannot read number of collections\n");
    exit (1);
  }
  sscanf(line, "%d %s", &numcoll, version);
  if (strcmp(version, "aggregates") == 0) /* Aggregates in dynamic profiling */
  { // WRITE CODE TO SUPPORT AGGREGATES HERE 
    if(numcoll) { 
    } // numcoll > 0 
  } // "aggregates" 

  if ( fgets (line, 256, fp) != NULL) {
    // If userevent data is available, process it.
    sscanf(line, "%d %s", &numberOfUserEvents, version);
    if (strcmp(version, "userevents") == 0) /* User events */
    {
      userevents = true;
#ifdef DEBUG
      printf("User Events read %s \n", line);
#endif /* DEBUG */
      ProcessUserEventData(fp, node, ctx, thr, numberOfUserEvents);
    } 
    else
    { // Hey! What data did we read? 
#ifdef DEBUG
      printf("Unable to process data read: %s\n", line);
      printf("You're probably using an older version of this tool. \
	Please upgrade\n");
#endif /* DEBUG */
    }

  } /* user event data was there */
  else {
    userevents = false; /* no events defined for this file */
  }   
  fclose(fp);
#ifdef DEBUG
  cout << "Closing file " << filename << endl;
#endif /* DEBUG */

  // FUNCTION SUMMARY INFO 
  FunctionSummaryInfo(node, ctx, thr, max);
  if (userevents) 
  { /* user events were defined for this file */
    UserEventSummaryInfo(node, ctx, thr);
  }

  return 1;

}

int FunctionSummaryInfo(int no, int ctx, int thr, int max) 
{ // Continuation of ProcessFileDynamic - just breaking up the code 
  int i, j;
  int active_counters=0;
  int numf;
  int numc;
  int numa;
  double total, ct;
  char ident_str[32];


  /* Globals used to initialize locals */
  numf = numfunc;
  numc = numa = numcoll; 
  hpcxx_flag = TRUE; // for n,c,t display

  /* -- initialize summary function profile data table ---------------------- */
  if ( !p_total_tbl ) {
    p_total_tbl = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    p_min_tbl   = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    p_max_tbl   = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    for (i=0; i<numf; i++) {
      p_total_tbl[i].tag      		= functagbuf[i];
      p_total_tbl[i].name     		= funcnamebuf[i];
      p_total_tbl[i].usec     		= 0.0;
      p_total_tbl[i].cumusec  		= 0.0;
      p_total_tbl[i].numcalls 		= 0;
      p_total_tbl[i].numsubrs 		= 0;
      p_total_tbl[i].stddeviation 	= 0;

      p_max_tbl[i].tag      = p_min_tbl[i].tag      = p_total_tbl[i].tag;
      p_max_tbl[i].name     = p_min_tbl[i].name     = p_total_tbl[i].name;
      p_max_tbl[i].usec     = p_min_tbl[i].usec     = p_func_list[i].usec;
      p_max_tbl[i].cumusec  = p_min_tbl[i].cumusec  = p_func_list[i].cumusec; 
      p_max_tbl[i].numcalls = p_min_tbl[i].numcalls = p_func_list[i].numcalls;
      p_max_tbl[i].numsubrs = p_min_tbl[i].numsubrs = p_func_list[i].numsubrs;
      p_max_tbl[i].stddeviation = p_min_tbl[i].stddeviation = p_func_list[i].stddeviation;
#ifdef DEBUG
#ifdef USE_LONG 
    printf(" Func %d, min_tbl[i].numcalls %ld min_tbl[i].numsubrs %ld usec %lG, cumusec %lG\n",
        i, p_min_tbl[i].numcalls, p_min_tbl[i].numsubrs, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %ld max_tbl[i].numsubrs %ld usec %lG, cumusec %lG\n",
        i, p_max_tbl[i].numcalls, p_max_tbl[i].numsubrs, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
#else // DEFAULT double
    printf(" Func %d, min_tbl[i].numcalls %lG min_tbl[i].numsubrs %lG usec %lG, cumusec %lG\n",
        i, p_min_tbl[i].numcalls, p_min_tbl[i].numsubrs, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %lG max_tbl[i].numsubrs %lG usec %lG, cumusec %lG\n",
        i, p_max_tbl[i].numcalls, p_max_tbl[i].numsubrs, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
#endif // USE_LONG 
#endif /* DEBUG */
    }
    total_total = 0.0;
    /* old
    max_total = min_total = p_func_list[0].cumusec; 0 is not top level fn.
   */
    max_total = min_total = p_func_list[top_level_function].cumusec;
  }

  /* -- set function profile data table ------------------------------------- */
  /* -- and update summary function profile data table ---------------------- */
  p_prof_tbl = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
  for (i=0; i<numf; i++) {
    p_prof_tbl[i].tag   = p_total_tbl[i].tag;
    p_prof_tbl[i].name  = p_total_tbl[i].name;
    p_total_tbl[i].usec     += p_prof_tbl[i].usec     = p_func_list[i].usec;
    p_total_tbl[i].cumusec  += p_prof_tbl[i].cumusec  = p_func_list[i].cumusec;
    p_total_tbl[i].numcalls += p_prof_tbl[i].numcalls = p_func_list[i].numcalls;
    p_total_tbl[i].numsubrs += p_prof_tbl[i].numsubrs = p_func_list[i].numsubrs;
    p_total_tbl[i].stddeviation += p_prof_tbl[i].stddeviation = p_func_list[i].stddeviation;

    if ( p_min_tbl[i].usec     > p_func_list[i].usec )
      p_min_tbl[i].usec     = p_func_list[i].usec;
    if ( p_min_tbl[i].cumusec  > p_func_list[i].cumusec )
      p_min_tbl[i].cumusec  = p_func_list[i].cumusec;
    if ( p_min_tbl[i].numcalls > p_func_list[i].numcalls )
      p_min_tbl[i].numcalls = p_func_list[i].numcalls;
    if ( p_min_tbl[i].numsubrs > p_func_list[i].numsubrs )
      p_min_tbl[i].numsubrs = p_func_list[i].numsubrs;
    if ( p_min_tbl[i].stddeviation > p_func_list[i].stddeviation )
      p_min_tbl[i].stddeviation = p_func_list[i].stddeviation;

    if ( p_max_tbl[i].usec     < p_func_list[i].usec )
      p_max_tbl[i].usec     = p_func_list[i].usec;

    if ( p_max_tbl[i].cumusec  < p_func_list[i].cumusec )
      p_max_tbl[i].cumusec  = p_func_list[i].cumusec;
    if ( p_max_tbl[i].numcalls < p_func_list[i].numcalls )
      p_max_tbl[i].numcalls = p_func_list[i].numcalls;
    if ( p_max_tbl[i].numsubrs < p_func_list[i].numsubrs )
      p_max_tbl[i].numsubrs = p_func_list[i].numsubrs;
    if ( p_max_tbl[i].stddeviation < p_func_list[i].stddeviation )
      p_max_tbl[i].stddeviation = p_func_list[i].stddeviation;
  }
#ifdef DEBUG
  for (i=0; i<numf; i++) {
#ifdef USE_LONG 
    printf(" Func %d, min_tbl[i].numcalls %ld  numsubrs %ld usec %lG, cumusec %lG\n",
        i, p_min_tbl[i].numcalls, p_min_tbl[i].numsubrs, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %ld numsubrs %ld usec %lG, cumusec %lG\n",
        i, p_max_tbl[i].numcalls, p_max_tbl[i].numsubrs, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
#else // DEFAULT double
    printf(" Func %d, min_tbl[i].numcalls %lG  numsubrs %lG usec %lG, cumusec %lG\n",
        i, p_min_tbl[i].numcalls, p_min_tbl[i].numsubrs, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %lG numsubrs %lG usec %lG, cumusec %lG\n",
        i, p_max_tbl[i].numcalls, p_max_tbl[i].numsubrs, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
#endif // USE_LONG 
  }
#endif /* DEBUG */


#ifdef DEBUG
  printf("Top level function = %d in file %s\n", top_level_function, proffile);
#endif /* DEBUG */
  /* -- get total runtime (time of "main" function (always function 0)) ----- */
  /* This is not true for all programs; new method -
     The top level function has max cumusec time for that thread (support
     for MIMD programs where top level fn is different for different threads
*/
  total_total += total = p_func_list[top_level_function].cumusec;
  if ( min_total > p_func_list[top_level_function].cumusec ) min_total = p_func_list[top_level_function].cumusec;
  if ( max_total < p_func_list[top_level_function].cumusec ) max_total = p_func_list[top_level_function].cumusec;

#ifdef DEBUG
  printf("%s : total = %5.1f top level = %5.1f\n", proffile, total, max_thread_cumusec);
#endif
  if (hpcxx_flag == FALSE)
  {
    sprintf(ident_str,"%d", no);
  }
  else  /* hpc++ */
  {
    sprintf(ident_str,"%d,%d,%d", no, ctx, thr);
  }


  /* -- print function profile data table ----------------------------------- */
  if ( nodeprint ) {
    if ( dump ) {
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), MsecCmp);
      DumpFuncTab (p_prof_tbl, ident_str, total, max, "excl");
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), CumMsecCmp);
      DumpFuncTab (p_prof_tbl, ident_str, total, max, "incl");
    }
    else {
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), compar);
      if (hpcxx_flag == FALSE)
        printf ("\nNODE %d: \n", no);
      else
      {
        printf ("\nNODE %d;", no);
        printf ("CONTEXT %d;", ctx);
        printf ("THREAD %d:\n", thr);
      }

      PrintFuncTab (p_prof_tbl, total, max);
    }
  }

  if ( numc ) {
    if ( hpcxx_flag == FALSE) { /* pc++ data format */
      if ( nodeprint ) {
        if ( ! dump ) {
          printf ("\n   local    remote  collection\n");
          printf (  "accesses  accesses  num   name\n");
        }
      }
      for (i=0; i<numc; i++) {
        if ( nodeprint ) {
          /* -- print collection profile data table --------------------------- */
          if ( dump ) {
            if ( ct = p_coll_list[i].localacs + p_coll_list[i].remoteacs ) {
              printf ("coll %d %d %d %4.2f %d %4.2f\n", no, i,
                     p_coll_list[i].localacs, p_coll_list[i].localacs/ct*100.0,
                     p_coll_list[i].remoteacs, p_coll_list[i].remoteacs/ct*100.0);
            }
            else {
              printf ("coll %d %d 0 0.0 0 0.0\n", no, i);
            }
          }
          else {
            printf ("%8d  %8d  %3d   %s\n",
              p_coll_list[i].localacs, p_coll_list[i].remoteacs,
              i, p_coll_list[i].varname);
          }
        }
 
        /* -- compute collection profile summary ------------------------------ */
        p_coll_tbl[i].localacs  += p_coll_list[i].localacs;
        p_coll_tbl[i].remoteacs += p_coll_list[i].remoteacs;
      }
    } /* hpcxx_flag == FALSE */
    else {
        /* print aggregate info */
/*
      if ( nodeprint ) {
        if ( ! dump ) {
          printf ("\n   local    remote  collection\n");
          printf (  "accesses  accesses  num   name\n");
        }
      }
*/
      for (i=0; i<numa; i++) {
        if ( nodeprint ) {
          /* -- print aggregate profile data table --------------------------- */
          if ( dump ) {
            if ( p_aggr_list[i].total_events ) {
              active_counters = 0;
              for(j=0; j < MAX_COUNTERS; j++)
              {
                if (p_aggr_list[i].counters[j]) active_counters++ ;
              }
              printf("aggregates %d,%d,%d %d %d ",no, ctx, thr, i, active_counters);
              /* and then the quads <eid, name, no, %val> */
              for (j = 0; j < MAX_COUNTERS; j++)
              {
                /* print only those events that took place */
                if ( p_aggr_list[i].counters[j])
                {
                  if (eventnamebuf == NULL)
                  { /* print NULL */
                    printf("%d NULL %d %4.2f ", j,
                        p_aggr_list[i].counters[j],
        p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
                  }
                  else
                  {
                    printf("%d %s %d %4.2f ", j, eventnamebuf[j],
                        p_aggr_list[i].counters[j],
        p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
                  }
                }
              } /* printed the j counters */
              printf("%s %s %s\n", p_aggr_list[i].container_name,
                p_aggr_list[i].container_type, p_aggr_list[i].var_name);
            }
            else /* all counters are 0 */
            {
              printf("aggregates %d,%d,%d %d 0 NULL 0 0.0 %s %s %s\n",
                no, ctx, thr, i, p_aggr_list[i].container_name,
                p_aggr_list[i].container_type, p_aggr_list[i].var_name);
              /* node no ctx thr , eid 0 name NULL events 0 % 0.0 */
            }
          }
          else
          { /* not dump */
            printf("aggregates %s <%s> %s\n",p_aggr_list[i].container_name,
                        p_aggr_list[i].container_type, p_aggr_list[i].var_name);
            for(j = 0; j < MAX_COUNTERS; j++)
            {
              if(p_aggr_list[i].counters[j])
              {
                if(eventnamebuf == NULL)
                {
                  printf("Event id %d\t: %d %4.2f       percent\n",
                        j, p_aggr_list[i].counters[j],
        p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
                }
                else
                {
                  printf("Event id %d name %s\t: %d %4.2f percent\n",
                        j, eventnamebuf[j], p_aggr_list[i].counters[j],
        p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
                }
              }
            } /* printed all events */
          } /* not dump */
        } /* nodeprint */
        /* -- compute collection profile summary ------------------------------ */
        for (j = 0; j < MAX_COUNTERS; j++)
        {
          p_aggr_tbl[i].counters[j] += p_aggr_list[i].counters[j];
        }
        p_aggr_tbl[i].total_events += p_aggr_list[i].total_events;
      } /* for i */
    } /* hpcxx_flag == TRUE */
  } /* numc > 0 */

  free (p_coll_list);
  free (p_func_list);
  free (p_prof_tbl);
  free (p_aggr_list);
  files_processed ++; /* increment counter for hpc++ */
  return (TRUE);
}

int ProcessFileDynamicInContext(int node, int ctx, int thr, int maxfuncs, char *prefix)
{
#ifdef DEBUG
  cout << "ProcessFileDynamicInContext n " << node << " c " << ctx << " t " << thr << endl;
#endif /* DEBUG */
  for (thr =0, filledDBThr = 0; ProcessFileDynamic(node, ctx, thr, maxfuncs, prefix); thr++);
  if (filledDBThr)
    return TRUE;
  else
    return FALSE; 

}

int ProcessFileDynamicInNode (int node, int ctx, int thr, int maxfuncs, char *prefix)
{
#ifdef DEBUG 
  cout << "ProcessFileDynamicInNode n "<< node << " c "<< ctx << " t "<< thr<< endl;
#endif /* DEBUG */
  for(ctx=0, filledDBCtx = 0; ProcessFileDynamicInContext(node, ctx, thr, maxfuncs, prefix); ctx++);
  if (filledDBCtx)
    return TRUE;
  else
    return FALSE;
  /* Iterations over 0.0.0 0.0.1 0.1.0 1.0.0 etc. */
}

/******************* user events profiling code ***************************/
void  ProcessUserEventData(FILE *fp, int node, int ctx, int thr, 
	int numberOfUserEvents)
{
  char line[SIZE_OF_LINE]; // In case function name is *really* long - templ. args
  char func[SIZE_OF_LINE]; // - do - 
  double userNumEvents, userMax, userMin, userMean, userSumSqr;
  map<const char*, UserEventData, ltstr>::iterator it;
  int i, j;

  
  // New Data format contains a string like
  // "# Name Calls Subrs Excl Incl SumExclSqr ProfileCalls"
  // "# eventname numevents max min mean sumsqr
  if (fgets(line, sizeof(line), fp) == NULL) {
    perror("Error: User Event fgets returns NULL in format string ");
    return ;
  }
  if (strncmp (line, "# eventname numevents max min mean sumsqr", 
	strlen("# eventname numevents max min mean sumsqr")) == 0)
  { 
#ifdef DEBUG
    cout << "ProcessUserEventData: Read line :" << line << " AS EXPECTED " << endl;
#endif /* DEBUG */
  } 
  else 
  {
    cout << "Unexpected format string :"<< line <<": Currently not supported"
      << endl;
    return;
  }
  

  /* Before reading the data, initialize the map for the function */
  for(it = userEventDB.begin(); it != userEventDB.end(); it++) {
    (*it).second = UserEventData(); /* initialized to null values */
#ifdef DEBUG 
     cout << "userEventDB entries name :"<< (*it).first <<endl;
#endif /* DEBUG */
  } /* This ensures that data from two files doesn't interfere */

  /* Main loop of reading the function information, line by line */
  for (i =0; i < numberOfUserEvents; i++) {
    if (fgets(line, SIZE_OF_LINE, fp) == NULL) {
      perror("Error in fgets: Cannot read event table");
      return ;
    }
    // line[0] has '"' - start loop from 1 to get the entire function name
    for (j=1; line[j] != '"'; j++) {
        func[j-1] = line[j];
    }
    func[j-1] = '\0'; // null terminate the string
    // At this point line[j] is '"' and the has a blank after that, so
    // line[j+1] corresponds to the beginning of other data.
    sscanf(&line[j+1], "%lG %lG %lG %lG %lG", &userNumEvents, &userMax, 
	&userMin, &userMean, &userSumSqr);

    if ((it = userEventDB.find((const char *)func)) == userEventDB.end()) {
      cout << "ERROR : In second pass ProcessUserEventData didn't find name " 
	<< func << " on node " << node << " context " << ctx << " thread " 
	<< thr << endl;
      return;
    }
    userEventDB[func] += UserEventData(userNumEvents, userMax, userMin, 
	userMean, userSumSqr);
    /* In case a function appears twice in the same file (templated function
    the user didn't specify exactly unique type - then add the data. Defaults
    to assignment as initialization cleans it up. */
#ifdef DEBUG 
    cout << "Added userEvent entry to DB " << func << " : num " 
      << userNumEvents << " max " << userMax << " mean " << userMean 
      << " sumsqr " << userSumSqr << endl;
#endif /* DEBUG */
  } /* processed all userevent lines */
  return;
} 



void UserEventSummaryInfo(int node, int ctx, int thr)
{
  // Generate a report of user events 
  double stddev;
  int i;
  map<const char*, UserEventData, ltstr>::iterator it;
 
  /* -- print user event profile data table ------------------------------ */
  if ( nodeprint ) {
    if ( dump ) 
    {
    //Code for racy 
      printf("%d userevents\n", userEventDB.size());
      printf("NumSamples   MaxValue   MinValue  MeanValue  Std. Dev.  Event Name\n");
      for(it = userEventDB.begin(), i=0; it != userEventDB.end(); it++, i++ ) 
      {
        // Calculate the standard deviation = sqrt((sumt^2)/N - mean^2)
	if ((*it).second.numevents == 0) 
	{ 
	  stddev = 0;
	  (*it).second.maxvalue = 0;
	  (*it).second.minvalue = 0;
	  (*it).second.meanvalue = 0;
	} else
 	{
          stddev = sqrt(fabs( ((*it).second.sumsqr/(*it).second.numevents)
                 - ( (*it).second.meanvalue * (*it).second.meanvalue )));
	}
	printf("userevent %d,%d,%d %d \"%s\" %#.16G %#.16G %#.16G %#.16G %#.16G\n", 
	  node, ctx, thr, i,
	  (*it).first, 
	  (*it).second.numevents, 
	  (*it).second.maxvalue, 
	  (*it).second.minvalue, 
	  (*it).second.meanvalue, 
	  stddev);

        printf("%10.4G %10.4G %10.4G %10.4G %10.4G  %s\n",
          (*it).second.numevents,
          (*it).second.maxvalue,
          (*it).second.minvalue,
          (*it).second.meanvalue,
          stddev,
          (*it).first);
      }
 
    }
    else 
    {
      printf("---------------------------------------------------------------------------------------\n");
      printf("\nUSER EVENTS Profile :NODE %d, CONTEXT %d, THREAD %d\n", 
	node, ctx, thr);
      printf("---------------------------------------------------------------------------------------\n");
      printf("NumSamples   MaxValue   MinValue  MeanValue  Std. Dev.  Event Name\n");
      printf("---------------------------------------------------------------------------------------\n");
      for(it = userEventDB.begin(); it != userEventDB.end(); it++ ) {
        // Calculate the standard deviation = sqrt((sumt^2)/N - mean^2)
	if ((*it).second.numevents == 0) 
	{ 
	  stddev = 0;
	  (*it).second.maxvalue = 0;
	  (*it).second.minvalue = 0;
	  (*it).second.meanvalue = 0;
	} else
 	{
          stddev = sqrt(fabs( ((*it).second.sumsqr/(*it).second.numevents) 
		 - ( (*it).second.meanvalue * (*it).second.meanvalue )));
        }
	printf("%10.4G %10.4G %10.4G %10.4G %10.4G  %s\n",
	  (*it).second.numevents,
	  (*it).second.maxvalue,
	  (*it).second.minvalue,
	  (*it).second.meanvalue,
	  stddev,
	  (*it).first);
      } /* for loop */
      printf("---------------------------------------------------------------------------------------\n");
	
    } /* not dump */
 
  } /* node print */	  
   
} 



/******************** pC++ /HPC++ profiling code **************************/
static char *strsave (const char *s)
{
  char *r;

  if ( (r = (char *) malloc (strlen(s)+1)) == NULL ) {
    fprintf (stderr, "error: no more memory\n");
    exit (1);
  }
  strcpy (r, s);
  return r;
}

/*
 * ToTimeStr: convert usec to hh:mm:ss.mmm
 */

static char *ToTimeStr (double ti, char timbuf[])
{
  long msec, sec, min, hour;

  if (hwcounters == false) { /* time */
    msec = (long) fmod (ti / 1.0e3, 1.0e3);
    sec  = (long) fmod (ti / 1.0e6, 60.0);
    min  = (long) fmod (ti / 60.0e6, 60.0);
    hour = (long) (ti / 36.0e8);
  
    if ( hour )
      sprintf (timbuf, "%2d:%02d:%02d.%03d", hour, min, sec, msec);
    else if ( min )
      sprintf (timbuf, "   %2d:%02d.%03d", min, sec, msec);
    else if ( sec )
      sprintf (timbuf, "      %2d,%03d", sec, msec);
    else
      sprintf (timbuf, "         %3d", msec);
    if (ti < 1.0e3) 
      sprintf (timbuf, "   %9.3G",ti/1.0e3);
  } else { /* counters */
    sprintf(timbuf,"%12.4G", ti);
  }

  return (timbuf);
}

/*
 * MsecCmp   : compare usec field
 * CumMsecCmp: compare cumusec field
 * CallCmp   : compare numcalls field
 *
 * functions return (unsually) -1 for greater than, 0 for equal, 1 for less than
 * in order to reverse the order of sort (descending instead of ascending)
 * can be changed through sign
 */

static int sign = 1;

static int MsecCmp (const void *left, const void *right)
{
  double l = ((struct p_prof_elem *) left)->usec;
  double r = ((struct p_prof_elem *) right)->usec;

  if ( l < r )
    return sign;
  else if ( l > r )
    return -sign;
  else
    return 0;
}

static int CumMsecCmp (const void *left, const void *right)
{
  double l = ((struct p_prof_elem *) left)->cumusec;
  double r = ((struct p_prof_elem *) right)->cumusec;

  if ( l < r )
    return sign;
  else if ( l > r )
    return -sign;
  else
    return 0;
}

static int CallCmp (const void *left, const void *right)
{
  double l = ((struct p_prof_elem *) left)->numcalls;
  double r = ((struct p_prof_elem *) right)->numcalls;

  if ( l < r )
    return sign;
  else if ( l > r )
    return -sign;
  else
    return 0;
}

static int MsecPerCallCmp (const void *left, const void *right)
{
  if (((struct p_prof_elem *) left) ->numcalls == 0) return sign;
  if (((struct p_prof_elem *) right) ->numcalls == 0) return -sign;
  double l = ((struct p_prof_elem *) left) ->usec/ ((struct p_prof_elem *) left) ->numcalls;
  double r = ((struct p_prof_elem *) right) ->usec/ ((struct p_prof_elem *) right)->numcalls;

  if ( l < r )
    return sign;
  else if (l > r)
    return -sign;
  else
    return 0;
}

static int CumMsecPerCallCmp (const void *left, const void *right)
{
  if (((struct p_prof_elem *) left) ->numcalls == 0) return sign;
  if (((struct p_prof_elem *) right) ->numcalls == 0) return -sign;
  double l = ((struct p_prof_elem *) left) ->cumusec/ ((struct p_prof_elem *) left) ->numcalls;
  double r = ((struct p_prof_elem *) right) ->cumusec/ ((struct p_prof_elem *) right)->numcalls;

  if ( l < r )
    return sign;
  else if (l > r)
    return -sign;
  else
    return 0;
}

static int StdDevCmp (const void *left, const void *right)
{
  double l = ((struct p_prof_elem *) left ) -> stddeviation;
  double r = ((struct p_prof_elem *) right) -> stddeviation;

  if ( l < r )
    return sign;
  else if (l > r)
    return -sign;
  else
    return 0;
}

static int SubrCmp (const void *left, const void *right)
{
  double l = ((struct p_prof_elem *) left)->numsubrs;
  double r = ((struct p_prof_elem *) right)->numsubrs;

  if ( l < r )
    return sign;
  else if ( l > r )
    return -sign;
  else
    return 0;
}

/*
 * PrintFuncTab : Print plain function profile data
 * DumpFuncTab  : Dump plain function profile data
 */


static void PrintFuncTab (struct p_prof_elem *tab, double total, int max)
{
  int i;
#ifdef USE_LONG 
  int o_numcalls = 0;
  int o_numsubrs = 0;
#else // DEFAULT double 
  double o_numcalls = 0.0;
  double o_numsubrs = 0.0;
#endif // USE_LONG 
  double o_usec = 0.0;
  double o_cumusec = 0.0;
  double o_stddeviation = 0.0;
  char buf1[20], buf2[20];

  for (i=max; i<numfunc; i++) {
    o_numcalls 	   += tab[i].numcalls;
    o_numsubrs     += tab[i].numsubrs;
    o_usec         += tab[i].usec;
    o_cumusec      += tab[i].cumusec;
    o_stddeviation += tab[i].stddeviation;
  }

  if (hwcounters == false) {
    printf ("---------------------------------------------------------------------------------------\n");
    printf ("%%Time    Exclusive    Inclusive       #Call      #Subrs  Inclusive ");
    if (profilestats) printf("  Standard ");
    printf("Name\n");
    printf ("              msec   total msec                          usec/call ");
    if (profilestats) printf(" deviation ");
    printf("\n");
    printf ("---------------------------------------------------------------------------------------\n");
  } // timing data
  else {
    printf ("---------------------------------------------------------------------------------------\n");
    printf ("%%Time   Exclusive   Inclusive       #Call      #Subrs Count/Call ");
    if (profilestats) printf("  Standard ");
    printf("Name\n");
    printf ("           counts total counts                            ");
    if (profilestats) printf("  deviation ");
    printf("\n");
    printf ("---------------------------------------------------------------------------------------\n");

  } // Counters data 

  for (i=0; i<numfunc && i<max; i++) {
#ifdef DEBUG
#ifdef USE_LONG 
    printf(" PrintFuncTab name = %s, numcalls %ld, numsubrs %ld usec %lG, cumusec %lG \n",
	tab[i].name, tab[i].numcalls, tab[i].numsubrs, tab[i].usec, tab[i].cumusec);
#else // DEFAULT double
    printf(" PrintFuncTab name = %s, numcalls %lG, numsubrs %lG usec %lG, cumusec %lG \n",
	tab[i].name, tab[i].numcalls, tab[i].numsubrs, tab[i].usec, tab[i].cumusec);
#endif // USE_LONG 
#endif 
/* DO WE NEED if tab[i].numcalls > 0 ? Yes - otherwise usec/call is inf */
    if ( tab[i].numcalls > 0 )  {
      if ( tab[i].cumusec > 0.0 ) { /*changed from usec > 0.0 to cumusec >0.0 */
        if ( hwcounters == false) { /* timing data use strings conversion */
#ifdef USE_LONG
          printf ("%5.1f %s %s %8ld %8ld %10.0f ",
#else // DEFAULT double 
          printf ("%5.1f %s %s %11G %11G %10.0f ",
#endif // USE_LONG
            tab[i].cumusec / total * 100.0,
            ToTimeStr (tab[i].usec, buf1),
            ToTimeStr (tab[i].cumusec, buf2),
            tab[i].numcalls,
            tab[i].numsubrs,
            tab[i].cumusec / tab[i].numcalls);
	  if (profilestats) printf("%10.4G ", tab[i].stddeviation); 
 	  printf("%s\n", tab[i].name);
 	} 
        else { /* Counters  - do not use hr:mn:sec.msec format */
#ifdef USE_LONG 
          printf ("%5.1f  %10.4G  %10.4G %8ld %8ld %10.0f ",
#else // DEFAULT double
          printf ("%5.1f  %10.4G  %10.4G %11G %11G %10.0f ",
#endif // USE_LONG 
            tab[i].cumusec / total * 100.0,
            tab[i].usec,
            tab[i].cumusec,
            tab[i].numcalls,
            tab[i].numsubrs,
            tab[i].cumusec / tab[i].numcalls);
 	  if(profilestats) printf("%10.4G ", tab[i].stddeviation);
          printf("%s\n", tab[i].name);
        } /* counters */
      } /* cumusec > 0 */
      else {
#ifdef USE_LONG
        printf ("  0.0            0            0 %8ld %8ld          0 ",
#else // DEFAULT double 
        printf ("  0.0            0            0 %11G %11G          0 ",
#endif // USE_LONG 
          tab[i].numcalls,
          tab[i].numsubrs);
	if (profilestats) printf("%10.4G ", tab[i].stddeviation);
	printf("%s\n", tab[i].name);

      }
    } 
  }

  if ( o_numcalls > 0 ) {
    if ( o_cumusec > 0.0 ) {
      if (hwcounters == false) { /* time */
#ifdef USE_LONG
        printf ("%5.1f %s %s %8ld %8ld %10.0f ",
#else // DEFAULT double 
        printf ("%5.1f %s %s %11G %11G %10.0f ",
#endif // USE_LONG 
          o_cumusec / total * 100.0,
          ToTimeStr (o_usec, buf1), ToTimeStr (o_cumusec, buf2),
          o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
	if (profilestats) printf("%10.4G ",o_stddeviation);
	printf("-others-\n");
      }
      else { /* counters */
#ifdef USE_LONG
        printf ("%5.1f  %10.4G  %10.4G %8ld %8ld %10.0f ", 
#else // DEFAULT double
        printf ("%5.1f  %10.4G  %10.4G %11G %11G %10.0f ", 
#endif // USE_LONG 
          o_cumusec / total * 100.0,
          o_usec, o_cumusec,
          o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
	if (profilestats) printf("%10.4G ",o_stddeviation);
	printf("-others-\n");
      } /* counters */
    } /* o_cumusec > 0 */
    else {
#ifdef USE_LONG
      printf ("  0.0            0            0 %8ld %8ld          0 ",
#else // DEFAULT double 
      printf ("  0.0            0            0 %11G %11G          0 ",
#endif // USE_LONG 
        o_numcalls, o_numsubrs);
      if (profilestats) printf("%10.4G ", o_stddeviation);
      printf("-others-\n");
    }
  }
}

/////////////////////////////////////////////////////////////////////
static void PrintFuncTabOrig (struct p_prof_elem *tab, double total, int max)
{
  int i;
#ifdef USE_LONG
  int o_numcalls = 0;
  int o_numsubrs = 0;
#else // DEFAULT double 
  double o_numcalls = 0.0;
  double o_numsubrs = 0.0;
#endif // USE_LONG 

  double o_usec = 0.0;
  double o_cumusec = 0.0;
  char buf1[20], buf2[20];

  for (i=max; i<numfunc; i++) {
    o_numcalls += tab[i].numcalls;
    o_numsubrs += tab[i].numsubrs;
    o_usec     += tab[i].usec;
    o_cumusec  += tab[i].cumusec;
  }

  if (hwcounters == false) {
    printf ("---------------------------------------------------------------------------\n");
    printf ("%%time         msec   total msec       #call      #subrs  usec/call name\n");
    printf ("---------------------------------------------------------------------------\n");
  } // timing data
  else {
    printf ("---------------------------------------------------------------------------\n");
    printf ("%%time   exclusive   inclusive       #call      #subrs count/call name\n");
    printf ("---------------------------------------------------------------------------\n");
  } // Counters data 

  for (i=0; i<numfunc && i<max; i++) {
#ifdef DEBUG
#ifdef USE_LONG 
    printf(" PrintFuncTab name = %s, numcalls %ld, numsubrs %ld usec %lG, cumusec %lG \n",
	tab[i].name, tab[i].numcalls, tab[i].numsubrs, tab[i].usec, tab[i].cumusec);
#else // DEFAULT double
    printf(" PrintFuncTab name = %s, numcalls %lG, numsubrs %lG usec %lG, cumusec %lG \n",
	tab[i].name, tab[i].numcalls, tab[i].numsubrs, tab[i].usec, tab[i].cumusec);
#endif // USE_LONG 
#endif 
/* DO WE NEED if tab[i].numcalls > 0 ? Yes - otherwise usec/call is inf */
    if ( tab[i].numcalls > 0 )  {
      if ( tab[i].cumusec > 0.0 ) { /*changed from usec > 0.0 to cumusec >0.0 */
        if ( hwcounters == false) { /* timing data use strings conversion */
#ifdef USE_LONG 
          printf ("%5.1f %s %s %8ld %8ld %10.0f %s\n",
#else // DEFAULT double 
          printf ("%5.1f %s %s %11G %11G %10.0f %s\n",
#endif // USE_LONG
            tab[i].cumusec / total * 100.0,
            ToTimeStr (tab[i].usec, buf1),
            ToTimeStr (tab[i].cumusec, buf2),
            tab[i].numcalls,
            tab[i].numsubrs,
            tab[i].cumusec / tab[i].numcalls,
            tab[i].name);
 	} 
        else { /* Counters  - do not use hr:mn:sec.msec format */
#ifdef USE_LONG
          printf ("%5.1f  %10.4G  %10.4G %8ld %8ld %10.0f %s\n",
#else // DEFAULT double 
          printf ("%5.1f  %10.4G  %10.4G %11G %11G %10.0f %s\n",
#endif // USE_LONG 
            tab[i].cumusec / total * 100.0,
            tab[i].usec,
            tab[i].cumusec,
            tab[i].numcalls,
            tab[i].numsubrs,
            tab[i].cumusec / tab[i].numcalls,
            tab[i].name);
        } /* counters */
      } /* cumusec > 0 */
      else {
#ifdef USE_LONG 
        printf ("  0.0           0           0 %8ld %8ld          0 %s\n",
#else // DEFAULT double
        printf ("  0.0           0           0 %11G %11G          0 %s\n",
#endif // USE_LONG 
          tab[i].numcalls,
          tab[i].numsubrs,
          tab[i].name);

      }
    } 
  }

  if ( o_numcalls > 0 ) {
    if ( o_cumusec > 0.0 ) {
      if (hwcounters == false) { /* time */
#ifdef USE_LONG
        printf ("%5.1f %s %s %8ld %8ld %10.0f -others-\n",
#else // DEFAULT double 
        printf ("%5.1f %s %s %11G %11G %10.0f -others-\n",
#endif // USE_LONG 
          o_cumusec / total * 100.0,
          ToTimeStr (o_usec, buf1), ToTimeStr (o_cumusec, buf2),
          o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
      }
      else { /* counters */
#ifdef USE_LONG
        printf ("%5.1f  %10.4G  %10.4G %8ld %8ld %10.0f -others-\n",
#else // DEFAULT double 
        printf ("%5.1f  %10.4G  %10.4G %11G %11G %10.0f -others-\n",
#endif // USE_LONG 
          o_cumusec / total * 100.0,
          o_usec, o_cumusec,
          o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
      } /* counters */
    } /* o_cumusec > 0 */
    else {
//#ifdef USE_LONG 
//      printf ("  0.0           0           0 %8ld %8ld          0 -others-\n",
//#else // DEFAULT double 
//      printf ("  0.0           0           0 %11G %11G          0 -others-\n",
//#endif // USE_LONG
//        o_numcalls, o_numsubrs);
#ifdef USE_LONG
        printf ("%5.1f  %10.4G  %10.4G %8ld %8ld %10.0f -others-\n",
#else // DEFAULT double
        printf ("%5.1f  %10.4G  %10.4G %11G %11G %10.0f -others-\n",
#endif // USE_LONG 
          o_cumusec / total * 100.0,
          o_usec, o_cumusec,
          o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
    }
  }
}
//////////////////////////////////////////////////////////////////////


static void DumpFuncTab (struct p_prof_elem *tab, char *id_str, double total,
                         int max, char *order) 
{
  int i;
  int printed_anything = 0;
#ifdef USE_LONG 
  long o_numcalls = 0;
  long o_numsubrs = 0;
#else // DEFAULT double
  double o_numcalls = 0.0;
  double o_numsubrs = 0.0;
#endif // USE_LONG 
  double t = 0.0;
  double o_usec = 0.0;
  double o_cumusec = 0.0;
  double o_stddeviation = 0.0;
  char buf1[20], buf2[20];

  for (i=0; i<numfunc; i++) {
    if ( tab[i].numcalls ) t += tab[i].usec;
    if ( i >= max ) {
      o_numcalls += tab[i].numcalls;
      o_numsubrs += tab[i].numsubrs;
      o_usec     += tab[i].usec;
      o_cumusec  += tab[i].cumusec;
      o_stddeviation += tab[i].stddeviation;
    }
  }

  
/* SINCE RACY DOESN'T SUPPORT NO OF SUBROUTINES, WE DON'T SEND IT THIS YET! */
  for (i=0; i<numfunc && i<max; i++) {
    if ( tab[i].numcalls) {
      printf("%s ",id_str);
      /* old 
	 if ( id < 0 ) printf ("%c ", -id); else printf ("%d ", id);
      */
      printf ("%d \"%s\" %s ", tab[i].tag, tab[i].name, order);
      if ( order[0] == 'e' )
        printf ("%.16G %4.2f\n", tab[i].usec, tab[i].usec / t * 100.0);
      else if ( order[0] == 'i' )
        printf ("%.16G %4.2f\n", tab[i].cumusec, tab[i].cumusec/total*100.0);
      if ( tab[i].cumusec > 0.0 ) {
#ifdef USE_LONG 
        printf ("%5.1f %s %s %8d %8d %10.0f ",
#else // DEFAULT double 
        printf ("%5.1f %s %s %11G %11G %10.0f ",
#endif // USE_LONG
          tab[i].cumusec / total * 100.0,
          ToTimeStr (tab[i].usec, buf1), ToTimeStr (tab[i].cumusec, buf2),
	  tab[i].numcalls, tab[i].numsubrs,
          tab[i].cumusec / tab[i].numcalls);
	  if(profilestats) printf("%10.4G ", tab[i].stddeviation);
	  //if(profilestats && (strcmp(id_str, "m")!=0)) printf("%10.4G ", tab[i].stddeviation);
          printf("%s\n", tab[i].name);
      }
      else {
//#ifdef USE_LONG
//        printf ("  0.0         0         0 %8d          0 %s\n",
//#else // DEFAULT double 
//        printf ("  0.0         0         0 %11G          0 %s\n",
//#endif // USE_LONG 
//          tab[i].numcalls,
//          tab[i].name);
#ifdef USE_LONG
	printf ("%5.1f %s %s %8d %8d %10.0f ",
#else // DEFAULT double 
        printf ("%5.1f %s %s %11G %11G %10.0f ",
#endif // USE_LONG
          tab[i].cumusec / total * 100.0,
          ToTimeStr (tab[i].usec, buf1), ToTimeStr (tab[i].cumusec, buf2),
	  tab[i].numcalls, tab[i].numsubrs,
          tab[i].cumusec / tab[i].numcalls);
	  if(profilestats) printf("%10.4G ", tab[i].stddeviation);
          //if(profilestats && (strcmp(id_str, "m")!=0)) printf("%10.4G ", tab[i].stddeviation);
          printf("%s\n", tab[i].name);
      }
    printed_anything = 1; /* set flag */
    }
  }

  if ( o_numcalls > 0) {
    printf("%s ",id_str);
/* old 
    if ( id < 0 ) printf ("%c ", -id); else printf ("%d ", id);
*/
    printf (" -1 -others- %s ", order);
    if ( order[0] == 'e' )
      printf ("%.16G %4.2f\n", o_usec, o_usec / t * 100.0);
    else if ( order[0] == 'i' )
      printf ("%.16G %4.2f\n", o_cumusec, o_cumusec / total * 100.0);
    if ( o_cumusec > 0.0 ) {
#ifdef USE_LONG
      printf ("%5.1f %s %s %8d %8d %10.0f ",
#else // DEFAULT double 
      printf ("%5.1f %s %s %11G %11G %10.0f ",
#endif // USE_LONG 
        o_cumusec / total * 100.0,
        ToTimeStr (o_usec, buf1), ToTimeStr (o_cumusec, buf2),
        o_numcalls, o_numsubrs, o_cumusec / o_numcalls);
	if(profilestats) printf("%10.4G ", o_stddeviation);
        //if(profilestats && (strcmp(id_str, "m")!=0)) printf("%10.4G ", o_stddeviation);
        printf("-other-\n");
    }
    else {
#ifdef USE_LONG
      //printf ("  0.0            0            0 %8d %8d          0 -others-\n",
      printf ("  0.0            0            0 %8d %8d          0 ",
#else // DEFAULT double 
      //printf ("  0.0            0            0 %11G %11G          0 -others-\n",
      printf ("  0.0            0            0 %11G %11G          0 ",
#endif // USE_LONG 
	      o_numcalls, o_numsubrs);
      if(profilestats) printf("%10.4G ", o_stddeviation);
      printf("-other-\n");
    }
    printed_anything = 1; /* set flag */
  }
  /* if we don't print anything, racy sees an error in getting incomplete data */
  if (!printed_anything) { /* then print a dummy record */
    printf("%s ", id_str);
    printf (" -1 -others- %s ", order);
 
    if ( order[0] == 'e' )
      if (t > 0.0) {
        printf ("%.16G %4.2f\n", o_usec, o_usec / t * 100.0);
      }
      else { 
        printf ("%.16G %4.2f\n", o_usec, o_usec ); /* will print 0 0 */
      }
    else if ( order[0] == 'i' )
      printf ("%.16G %4.2f\n", o_cumusec, o_cumusec / total * 100.0);
#ifdef USE_LONG
    //printf ("  0.0            0            0 %8d %8d          0 -others-\n",
    printf ("  0.0            0            0 %8d %8d          0 ",
#else // DEFAULT double 
    //printf ("  0.0            0            0 %11G %11G          0 -others-\n",
    printf ("  0.0            0            0 %11G %11G          0 ",
#endif // USE_LONG 
	    o_numcalls, o_numsubrs);
      if(profilestats) printf("%10.4G ", o_stddeviation);
      printf("-other-\n");
  }
}

/*
 * ReadNameTable: read function name table file
 * ProcessFile  : read profile data node file and print results
 */


static void ReadNameTable (char file[])
{
  int i, tag;
  FILE *in;

  sprintf (proffile, "%s.ftab", file);

  if ( (in = fopen (proffile, "r")) == NULL ) {
    perror (proffile);
    exit (1);
  }

  /* -- read depfile -- */
  if ( fgets (lbuf, 256, in) == NULL ) {
    fprintf (stderr, "%s: cannot read function table\n", proffile);
    exit (1);
  }
  sscanf (lbuf, "%s", sbuf);
  depfile = strsave (sbuf);

  /* -- read number of functions -- */
  fgets (lbuf, 256, in);
  sscanf (lbuf, "%d", &numfunc);
  
  /* -- read function table -- */
  funcnamebuf = (char **) malloc (numfunc * sizeof(char *));
  functagbuf  = (int *) malloc (numfunc * sizeof(int));
  for (i=0; i<numfunc; i++) {
    fgets (lbuf, 256, in);
    sscanf (lbuf, "%d %s", &tag, sbuf);
    if ( tag < 0 )
      funcnamebuf[i] = NULL;
    else
      funcnamebuf[i] = strsave (sbuf);
    functagbuf[i] = tag;
  }
  fclose (in);
}

static void ReadEventTable (char file[])
{ /* reads from profile.ctab the event tables <eid, eventname> */
  int i, j, eventid;
  FILE *in;

  sprintf (proffile, "%s.ctab", file);

  if ( (in = fopen (proffile, "r")) == NULL ) {
	return ; /* there's no .ctab file - no need to fill table */
	/* its legal not to have .ctab - for pc++ e.g. */
  }

  /* -- read ctab -- */
  if ( fgets (lbuf, 256, in) == NULL ) {
    fprintf (stderr, "%s: cannot read event table\n", proffile);
    exit (1);
  }
  sscanf (lbuf, "%d", &numevents);

  /* -- read event table -- */
  eventnamebuf = (char **) malloc (numevents * sizeof(char *));
  for (i=0; i<numevents; i++) {
    fgets (lbuf, 256, in);
    sscanf (lbuf, "%d", &eventid);
    j = 0;
    while((lbuf[j] != '"') && (j < strlen(lbuf))) j++; /* skip */
    if ( j != strlen (lbuf))
    {
    	strcpy(sbuf, &lbuf[j]);
	sbuf[strlen(sbuf) - 1] = '\0' ; /* remove \n at the end */
    }
    else 
    {
	sscanf(lbuf, "%d %s",&eventid, sbuf);
	fprintf(stderr,"Warning : event id %d name %s should be in quotes in %s\n", eventid, sbuf, proffile);
    }
    
    if ( eventid < 0 )
      eventnamebuf[i] = NULL;
    else
      eventnamebuf[eventid] = strsave (sbuf);
  }
  fclose (in);
  /* we know the number of counters */
  if (numevents > MAX_COUNTERS) /* realloc the counters array */
  {
    fprintf(stderr,"Number of events in %s exceeds system limit \n",proffile);
  }
#ifdef DEBUG
  for(i=0; i<numevents; i++)
  {
    printf("Event id %d name=%s\n",i,eventnamebuf[i]);
  }
#endif
  return;
}

static int ProcessFile (int no, int ctx, int thr, int longname, int max, char prefix[], int ignore)
{
  int i, j, e, n, r, s, d, l, eid;
  int active_counters=0;
  int ret;
  long count;
  int numf;
  int numc;
  int numa;
  FILE *in;
  double t1, t2;
  double total, ct;
//  extern int errno;
  char s1[128], s2[128];
  char aggr_str[32],ident_str[32];

  if (longname == FALSE) /* pc++ */
  {
    sprintf (proffile, "%s.%d", prefix, no);
  }
  else
  { /* hpc++ profile.0.0.0 etc. */
    sprintf (proffile, "%s.%d.%d.%d", prefix, no, ctx, thr);
  }
    

  /* -- read profile data file and set profile data tables ------------------ */
  /* ------------------------------------------------------------------------ */
  if ( (in = fopen (proffile, "r")) == NULL ) {
    if ( TAUERRNO == ENOENT && ignore)
      return (FALSE);
    else {
      perror (proffile);
      exit (1);
    }
  }

#ifdef DEBUG
  printf("Reading %s\n", proffile);
#endif
  /* reset top level function finding data */
  top_level_function = 0 ; /* by default - works for pC++ */
  max_thread_cumusec = 0.0 ; /* initialize */

  /* -- read number of functions -------------------------------------------- */  if ( fgets (lbuf, 256, in) == NULL ) {
    fprintf (stderr,"invalid proftablefile: cannot read number of functions\n");    exit (1);
  }
  sscanf (lbuf, "%d", &numf);

  if ( numf != numfunc ) {
    fprintf (stderr, "%s: number of functions does not match\n", proffile);
    exit (1);
  }

  /* -- read and setup function profile data -------------------------------- */  p_func_list = (struct p_func_descr *) malloc (numf * sizeof(struct p_func_descr));
  for (i=0; i<numf; i++) {
    fgets (lbuf, 256, in);

#ifdef __bsdi__
    /* Wierd bogus HACK because bsdi is funky (PHB) */
    sscanf (lbuf, "%d %lf %lf", &n, &t1, &t2);
#else
    sscanf (lbuf, "%d %lG %lG", &n, &t1, &t2);
#endif

    p_func_list[i].numcalls = n;
    p_func_list[i].usec     = t1;
    p_func_list[i].cumusec  = t2;
    if (p_func_list[i].cumusec > max_thread_cumusec) {
        top_level_function = i; /* to find which is the top level function
        in this thread we find the function with the max cumusec */
        max_thread_cumusec = p_func_list[i].cumusec;
    }
#ifdef DEBUG
    printf("Func Id %d numcalls %d usec %lG cumusec %lG\n",i, p_func_list[i].numcalls, p_func_list[i].usec, p_func_list[i].cumusec);
#endif
  }

  /* -- read number of collections ------------------------------------------ */
  if ( fgets (lbuf, 256, in) == NULL ) {
    fprintf (stderr,
                  "invalid proftablefile: cannot read number of collections\n");
    exit (1);
  }
  sscanf (lbuf, "%d %s", &numc, aggr_str);

  if(strcmp(aggr_str,"coll") == 0) /* pc++ file, hpc++ has aggregates */
  {
    /* -- setup and read collection profile data ------------------------------ */
    if ( numc ) {
      p_coll_list = (struct p_coll_descr *) malloc (numc * sizeof(struct p_coll_descr));
  
      for (i=0; i<numc; i++) {
        fgets (lbuf, 256, in);
        sscanf (lbuf,"%d %d %d %d %d %s %s %s", &n, &d, &s, &l, &r, sbuf, s1, s2);
  
        p_coll_list[i].numelem   = n;
        p_coll_list[i].dim       = d;
        p_coll_list[i].size      = s;
        p_coll_list[i].localacs  = l;
        p_coll_list[i].remoteacs = r;
        p_coll_list[i].collname = strsave(sbuf);
        p_coll_list[i].elemname = strsave(s1);
        p_coll_list[i].varname  = strsave(s2);
      }
  
      if ( !p_coll_tbl ) {
        p_coll_tbl = (struct p_coll_descr *) malloc (numc * sizeof(struct p_coll_descr));
        for (i=0; i<numc; i++) {
          p_coll_tbl[i].numelem   = p_coll_list[i].numelem;
          p_coll_tbl[i].dim       = p_coll_list[i].dim;
          p_coll_tbl[i].size      = p_coll_list[i].size;
          p_coll_tbl[i].localacs  = 0;
          p_coll_tbl[i].remoteacs = 0;
          p_coll_tbl[i].collname = p_coll_list[i].collname;
          p_coll_tbl[i].elemname = p_coll_list[i].elemname;
          p_coll_tbl[i].varname  = p_coll_list[i].varname;
        }
        numcoll = numc;
      }
      else if ( numc != numcoll ) {
        fprintf (stderr, "%s: number of collections does not match\n", proffile);
        exit (1);
      }
    }
  } /* if aggr_str == "coll"*/
  else if (strcmp(aggr_str,"aggregates") == 0)
  { /* hpc++ aggregate info */
    numa = numc;
    hpcxx_flag = TRUE;

    if ( numa ) {
      p_aggr_list = (struct p_aggr_descr *) malloc (numa * sizeof(struct p_aggr_descr));
  
      for (i=0; i<numa; i++) {
/*
	if (((ret = fscanf(in, "%d", &e)) < 0) ||
	    ((ret = fscanf(in, "%d", &n)) < 0) ||
	    ((ret = fscanf(in, "%d", &d)) < 0) ||
	    ((ret = fscanf(in, "%d", &s)) < 0) )
*/
	ret = fscanf(in, "%d %d %d %d ", &e, &n, &d, &s);
        if (ret < 0)
        {
	  perror("fscanf error:");
	  exit(1);
	}
#ifdef DEBUG
	printf("Got events %d no %d dim %d size %d\n", e,n,d,s);
#endif
	/* clean the n counters */
	for(j =0; j < MAX_COUNTERS; j++)
	{
	  p_aggr_list[i].counters[j] = 0L;
 	}
	p_aggr_list[i].total_events = 0L;
  
        p_aggr_list[i].numelem   = n;
        p_aggr_list[i].dim       = d;
        p_aggr_list[i].size      = s;
	for(j = 0; j < e; j++)
	{ /* read the tuples <eventid, count > */
	  fscanf(in,"%d %ld ", &eid, &count);
#ifdef DEBUG
	  printf(" <e %d c %ld> ", eid, count);
#endif
	  if ((eid < 0) || (eid > MAX_COUNTERS))
 	  {
	     printf("Illegal event id %d \n", eid);
	     exit(1);
	  }
	  p_aggr_list[i].counters[eid] = count;
	  p_aggr_list[i].total_events += count;
 	}
	fscanf(in, "%s %s %s", sbuf, s1, s2);
        p_aggr_list[i].container_name = strsave(sbuf);
        p_aggr_list[i].container_type = strsave(s1);
        p_aggr_list[i].var_name  = strsave(s2);
#ifdef DEBUG
	printf("\nReading %s %s %s\n", sbuf,s1, s2);
#endif
      }
  
      if ( !p_aggr_tbl ) { /* this is done only once */
        p_aggr_tbl = (struct p_aggr_descr *) malloc (numa * sizeof(struct p_aggr_descr));
        for (i=0; i<numa; i++) {
          p_aggr_tbl[i].numelem   = p_aggr_list[i].numelem;
          p_aggr_tbl[i].dim       = p_aggr_list[i].dim;
          p_aggr_tbl[i].size      = p_aggr_list[i].size;
          p_aggr_tbl[i].container_name = p_aggr_list[i].container_name;
          p_aggr_tbl[i].container_type = p_aggr_list[i].container_type;
          p_aggr_tbl[i].var_name  = p_aggr_list[i].var_name;
	  for(j = 0; j < MAX_COUNTERS; j++)
	  { /* initialize */
	    p_aggr_tbl[i].counters[j]  = 0L;
	    p_aggr_tbl[i].total_events = 0L;
	  }
        }
        numaggr = numa; 
      }
      else if ( numa != numaggr ) {
        fprintf (stderr, "%s: number of aggregates does not match\n", proffile);
        exit (1);
      }
    } /* numa > 0 */
  }  /* aggr_str == "aggregates" */

  
  fclose (in);

  /* -- initialize summary function profile data table ---------------------- */
  if ( !p_total_tbl ) {
    p_total_tbl = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    p_min_tbl   = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    p_max_tbl   = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
    for (i=0; i<numf; i++) {
      p_total_tbl[i].tag      = functagbuf[i];
      p_total_tbl[i].name     = funcnamebuf[i];
      p_total_tbl[i].usec     = 0.0;
      p_total_tbl[i].cumusec  = 0.0;
      p_total_tbl[i].numcalls = 0;

      p_max_tbl[i].tag      = p_min_tbl[i].tag      = p_total_tbl[i].tag;
      p_max_tbl[i].name     = p_min_tbl[i].name     = p_total_tbl[i].name;
      p_max_tbl[i].usec     = p_min_tbl[i].usec     = p_func_list[i].usec;
      p_max_tbl[i].cumusec  = p_min_tbl[i].cumusec  = p_func_list[i].cumusec;
      p_max_tbl[i].numcalls = p_min_tbl[i].numcalls = p_func_list[i].numcalls;
#ifdef DEBUG
    printf(" Func %d, min_tbl[i].numcalls %d usec %lG, cumusec %lG\n",
 	i, p_min_tbl[i].numcalls, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %d usec %lG, cumusec %lG\n",
 	i, p_max_tbl[i].numcalls, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
#endif /* DEBUG */
    }
    total_total = 0.0;
    /* old
    max_total = min_total = p_func_list[0].cumusec; 0 is not top level fn.
   */
    max_total = min_total = p_func_list[top_level_function].cumusec;
  }

  /* -- set function profile data table ------------------------------------- */
  /* -- and update summary function profile data table ---------------------- */
  p_prof_tbl = (struct p_prof_elem *) malloc (numf * sizeof(struct p_prof_elem));
  for (i=0; i<numf; i++) {
    p_prof_tbl[i].tag   = p_total_tbl[i].tag;
    p_prof_tbl[i].name  = p_total_tbl[i].name;
    p_total_tbl[i].usec     += p_prof_tbl[i].usec     = p_func_list[i].usec;
    p_total_tbl[i].cumusec  += p_prof_tbl[i].cumusec  = p_func_list[i].cumusec;
    p_total_tbl[i].numcalls += p_prof_tbl[i].numcalls = p_func_list[i].numcalls;

    if ( p_min_tbl[i].usec     > p_func_list[i].usec )
      p_min_tbl[i].usec     = p_func_list[i].usec;
    if ( p_min_tbl[i].cumusec  > p_func_list[i].cumusec )
      p_min_tbl[i].cumusec  = p_func_list[i].cumusec;
    if ( p_min_tbl[i].numcalls > p_func_list[i].numcalls )
      p_min_tbl[i].numcalls = p_func_list[i].numcalls;

    if ( p_max_tbl[i].usec     < p_func_list[i].usec )
      p_max_tbl[i].usec     = p_func_list[i].usec;
    if ( p_max_tbl[i].cumusec  < p_func_list[i].cumusec )
      p_max_tbl[i].cumusec  = p_func_list[i].cumusec;
    if ( p_max_tbl[i].numcalls < p_func_list[i].numcalls )
      p_max_tbl[i].numcalls = p_func_list[i].numcalls;
  }
#ifdef DEBUG
  for (i=0; i<numf; i++) {
    printf(" Func %d, min_tbl[i].numcalls %d usec %lG, cumusec %lG\n",
 	i, p_min_tbl[i].numcalls, p_min_tbl[i].usec, p_min_tbl[i].cumusec);

    printf(" Func %d, max_tbl[i].numcalls %d usec %lG, cumusec %lG\n",
 	i, p_max_tbl[i].numcalls, p_max_tbl[i].usec, p_max_tbl[i].cumusec);
  }
#endif /* DEBUG */


#ifdef DEBUG
  printf("Top level function = %d in file %s\n", top_level_function, proffile);
#endif /* DEBUG */
  /* -- get total runtime (time of "main" function (always function 0)) ----- */
  /* This is not true for all programs; new method - 
     The top level function has max cumusec time for that thread (support 
     for MIMD programs where top level fn is different for different threads */
  total_total += total = p_func_list[top_level_function].cumusec;
  if ( min_total > p_func_list[top_level_function].cumusec ) min_total = p_func_list[top_level_function].cumusec;
  if ( max_total < p_func_list[top_level_function].cumusec ) max_total = p_func_list[top_level_function].cumusec;

#ifdef DEBUG
  printf("%s : total = %5.1f top level = %5.1f\n", proffile, total, max_thread_cumusec);
#endif
  if (hpcxx_flag == FALSE) 
  { 
    sprintf(ident_str,"%d", no);
  }
  else  /* hpc++ */
  {
    sprintf(ident_str,"%d,%d,%d", no, ctx, thr);
  }

 
  /* -- print function profile data table ----------------------------------- */
  if ( nodeprint ) {
    if ( dump ) {
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), MsecCmp);
      DumpFuncTab (p_prof_tbl, ident_str, total, max, "excl");
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), CumMsecCmp);
      DumpFuncTab (p_prof_tbl, ident_str, total, max, "incl");
    }
    else {
      qsort ((void *) p_prof_tbl, numf, sizeof(struct p_prof_elem), compar);
      if (hpcxx_flag == FALSE)
      	printf ("\nNODE %d: \n", no);
      else
      {
	printf ("\nNODE %d;", no);
        printf ("CONTEXT %d;", ctx);
        printf ("THREAD %d:\n", thr);
      }

      PrintFuncTab (p_prof_tbl, total, max);
    }
  }

  if ( numc ) {
    if ( hpcxx_flag == FALSE) { /* pc++ data format */
      if ( nodeprint ) {
        if ( ! dump ) {
          printf ("\n   local    remote  collection\n");
          printf (  "accesses  accesses  num   name\n");
        }
      }
      for (i=0; i<numc; i++) {
        if ( nodeprint ) {
          /* -- print collection profile data table --------------------------- */
          if ( dump ) {
            if ( ct = p_coll_list[i].localacs + p_coll_list[i].remoteacs ) {
              printf ("coll %d %d %d %4.2f %d %4.2f\n", no, i, 
                     p_coll_list[i].localacs, p_coll_list[i].localacs/ct*100.0,
                     p_coll_list[i].remoteacs, p_coll_list[i].remoteacs/ct*100.0);
            }
            else {
              printf ("coll %d %d 0 0.0 0 0.0\n", no, i);
            }
          }
          else {
            printf ("%8d  %8d  %3d   %s\n",
              p_coll_list[i].localacs, p_coll_list[i].remoteacs,
              i, p_coll_list[i].varname);
          }
        }
  
        /* -- compute collection profile summary ------------------------------ */
        p_coll_tbl[i].localacs  += p_coll_list[i].localacs;
        p_coll_tbl[i].remoteacs += p_coll_list[i].remoteacs;
      }
    } /* hpcxx_flag == FALSE */
    else {
	/* print aggregate info */
/*
      if ( nodeprint ) {
        if ( ! dump ) {
          printf ("\n   local    remote  collection\n");
          printf (  "accesses  accesses  num   name\n");
        }
      }
*/
      for (i=0; i<numa; i++) {
        if ( nodeprint ) {
          /* -- print aggregate profile data table --------------------------- */
          if ( dump ) {
            if ( p_aggr_list[i].total_events ) {
	      active_counters = 0;
	      for(j=0; j < MAX_COUNTERS; j++)
	      {
		if (p_aggr_list[i].counters[j]) active_counters++ ;
	      }
	      printf("aggregates %d,%d,%d %d %d ",no, ctx, thr, i, active_counters); 
	      /* and then the quads <eid, name, no, %val> */
	      for (j = 0; j < MAX_COUNTERS; j++) 
	      {
		/* print only those events that took place */ 
		if ( p_aggr_list[i].counters[j])
		{
	          if (eventnamebuf == NULL)
		  { /* print NULL */
		    printf("%d NULL %d %4.2f ", j, 
			p_aggr_list[i].counters[j], 
	p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
		  }
		  else
		  {
		    printf("%d %s %d %4.2f ", j, eventnamebuf[j],
			p_aggr_list[i].counters[j], 
	p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
		  }
		}
	      } /* printed the j counters */
	      printf("%s %s %s\n", p_aggr_list[i].container_name,
		p_aggr_list[i].container_type, p_aggr_list[i].var_name);
	    }
	    else /* all counters are 0 */
	    {
              printf("aggregates %d,%d,%d %d 0 NULL 0 0.0 %s %s %s\n", 
		no, ctx, thr, i, p_aggr_list[i].container_name,
		p_aggr_list[i].container_type, p_aggr_list[i].var_name);
	      /* node no ctx thr , eid 0 name NULL events 0 % 0.0 */
	    }
	  } 
          else 
	  { /* not dump */
	    printf("aggregates %s <%s> %s\n",p_aggr_list[i].container_name,
			p_aggr_list[i].container_type, p_aggr_list[i].var_name);
	    for(j = 0; j < MAX_COUNTERS; j++)
	    {
	      if(p_aggr_list[i].counters[j]) 
	      {
		if(eventnamebuf == NULL)
		{
		  printf("Event id %d\t: %d %4.2f	percent\n",
			j, p_aggr_list[i].counters[j],  
	p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
		}
		else
		{
		  printf("Event id %d name %s\t: %d %4.2f percent\n",
			j, eventnamebuf[j], p_aggr_list[i].counters[j],
	p_aggr_list[i].counters[j]*100.0/p_aggr_list[i].total_events);
		}
	      } 
	    } /* printed all events */
	  } /* not dump */
	} /* nodeprint */
        /* -- compute collection profile summary ------------------------------ */
	for (j = 0; j < MAX_COUNTERS; j++)
	{
	  p_aggr_tbl[i].counters[j] += p_aggr_list[i].counters[j];
	}
	p_aggr_tbl[i].total_events += p_aggr_list[i].total_events;
      } /* for i */
    } /* hpcxx_flag == TRUE */
  } /* numc > 0 */

  free (p_coll_list);
  free (p_func_list);
  free (p_prof_tbl);
  free (p_aggr_list);
  files_processed ++; /* increment counter for hpc++ */
  return (TRUE);
}

/*
 * PrintFuncSummary: Print summary function profile data
 * PrintSummary    : Print all summary profile information
 */

static void PrintFuncSummary (struct p_prof_elem *tab, double total,
                              int max, char *message, char *ident)
{
  /* -- print summary function profile data table --------------------------- */
  if ( dump ) {
    qsort ((void *) tab, numfunc, sizeof(struct p_prof_elem), MsecCmp);
    DumpFuncTab (tab,  ident, total, max, "excl");
    qsort ((void *) tab, numfunc, sizeof(struct p_prof_elem), CumMsecCmp);
    DumpFuncTab (tab,  ident, total, max, "incl");
  }
  else {
    qsort ((void *) tab, numfunc, sizeof(struct p_prof_elem), compar);
    printf ("\nFUNCTION SUMMARY (%s):\n", message);
    PrintFuncTab (tab, total, max);
  }
}

static void PrintSummary (int max, int numproc)
{
  int i,j,no_active_counters;
  double ct;

#ifdef DEBUG
  printf("PrintSummary: numproc = %d, dump = %d \n", numproc, dump);
#endif /* DEBUG */

  if ( (numproc > 1) || dump || (nodeprint == FALSE) ) {
    PrintFuncSummary (p_total_tbl, total_total, max, "total", "t");
    if (dumpminmax) {
      PrintFuncSummary (p_min_tbl, min_total, max, "min", "<");
      PrintFuncSummary (p_max_tbl, max_total, max, "max", ">");
    }

#ifdef DEBUG
  for(i=0; i < numfunc; i++) {
    printf("p_total_tbl[i].numcalls = %d\n", p_total_tbl[i].numcalls);
  }
#endif /* DEBUG */
  
    for (i=0; i<numfunc; i++) {
      p_total_tbl[i].numcalls /= numproc;
      p_total_tbl[i].numsubrs /= numproc;
      p_total_tbl[i].usec     /= numproc;
      p_total_tbl[i].cumusec  /= numproc;
    }
    total_total /= numproc;

#ifdef DEBUG
  for (i=0; i < numfunc; i++) {
    printf("PrintSummary(Mean): Func %d p_total_tbl numcalls %d, usec %lG, cumusec %lg\n", i, p_total_tbl[i].numcalls, p_total_tbl[i].usec, p_total_tbl[i].cumusec);
  }
#endif /* DEBUG */


    PrintFuncSummary (p_total_tbl, total_total, max, "mean", "m");
  }

  if(hpcxx_flag == FALSE) /* pc++ format */
  {
    if ( numcoll ) {
      if ( dump ) {
        for (i=0; i<numcoll; i++) {
          ct = p_coll_tbl[i].localacs + p_coll_tbl[i].remoteacs;
          printf ("cinfo %d %s<%s> %s %d %d %d %d %4.2f %d %4.2f\n",
            i, p_coll_tbl[i].collname, 
            p_coll_tbl[i].elemname, p_coll_tbl[i].varname,
            p_coll_tbl[i].numelem, p_coll_tbl[i].size, p_coll_tbl[i].dim,
            p_coll_tbl[i].localacs, p_coll_tbl[i].localacs/ct*100.0,
            p_coll_tbl[i].remoteacs, p_coll_tbl[i].remoteacs/ct*100.0);
        }
      }
      else {
        printf ("\nCOLLECTION SUMMARY:\n");
        printf ("------------------------------------------------------------\n");
        for (i=0; i<numcoll; i++) {
          printf ("%s<%s> %s, collection #%d\n", p_coll_tbl[i].collname,
            p_coll_tbl[i].elemname, p_coll_tbl[i].varname, i);
          printf ("\t%d elements of size %d, %d-dimensional\n",
            p_coll_tbl[i].numelem, p_coll_tbl[i].size, p_coll_tbl[i].dim);
          printf ("\t%8d local / %8d remote accesses\n",
            p_coll_tbl[i].localacs, p_coll_tbl[i].remoteacs);
        }
      }
    }
  }
  else /* hpcxx_flag == TRUE */
  {
    /* print aggregate information */

    if ( numaggr ) {
      if ( dump ) {
        for (i=0; i<numaggr; i++) {
	  no_active_counters  = 0;
	  for (j = 0; j < MAX_COUNTERS; j++) 
	  { 
	    if(p_aggr_tbl[i].counters[j]) no_active_counters ++;
	  } 
	
	  printf("ainfo %d %s %s  %s %d %d %d %d ", i, 
	    p_aggr_tbl[i].container_name, p_aggr_tbl[i].container_type,
	    p_aggr_tbl[i].var_name, p_aggr_tbl[i].numelem, 
	    p_aggr_tbl[i].size, p_aggr_tbl[i].dim, no_active_counters);

	  for(j = 0; j < MAX_COUNTERS; j++)
	  {
	    if (p_aggr_tbl[i].counters[j]) /* non zero */
	    { 
  	      if (eventnamebuf == NULL)
  	      { /* print NULL */
  	        printf("%d NULL %d %4.2f ", j, 
  	    	  p_aggr_tbl[i].counters[j], 
  	p_aggr_tbl[i].counters[j]*100.0/p_aggr_tbl[i].total_events);
  	      }
  	      else
  	      {
  	        printf("%d %s %d %4.2f ", j, eventnamebuf[j],
  	          p_aggr_tbl[i].counters[j], 
  	p_aggr_tbl[i].counters[j]*100.0/p_aggr_tbl[i].total_events);
  	      }
  	    }
	  } /* over counters j */
   	  printf("\n");
        } /* over all aggregates */
      } /* if dump */
      else {
        printf ("\nAGGREGATE SUMMARY:\n");
        printf ("------------------------------------------------------------\n");
        for (i=0; i<numaggr; i++) {
          printf ("%s<%s> %s, aggregate #%d\n", 
	    p_aggr_tbl[i].container_name,
            p_aggr_tbl[i].container_type, p_aggr_tbl[i].var_name, i);
          printf ("\t%d elements of size %d, %d-dimensional\n",
            p_aggr_tbl[i].numelem, p_aggr_tbl[i].size, p_aggr_tbl[i].dim);
	  for(j = 0; j < MAX_COUNTERS; j++)
	  {
	    if(p_aggr_tbl[i].counters[j]) 
	    {
	      if(eventnamebuf == NULL)
	      {
	        printf("Event id %d\t: %d %4.2f	percent\n",
		  j, p_aggr_tbl[i].counters[j],  
	p_aggr_tbl[i].counters[j]*100.0/p_aggr_tbl[i].total_events);
	      }
	      else
	      {
		printf("Event id %d name %s\t: %d %4.2f percent\n",
		  j, eventnamebuf[j], p_aggr_tbl[i].counters[j],
	p_aggr_tbl[i].counters[j]*100.0/p_aggr_tbl[i].total_events);
	      }
            }
	  } /* over counters */
	  printf("\n");
	} /* aggregates */
      } /* if dump */
    } /* numaggr > 0 */
  } /* hpcxx_flag */
  return;
}

static int IsFilePresent(char *filename)
{
FILE *in;
 
  if ( (in = fopen (filename, "r")) == NULL ) {
    return (FALSE);
  }
  else 
  {
    fclose(in);
    return (TRUE);
  }
}

static int ProcessFileInContext (int no, int ctx, int thr, int longname, int max, char prefix[], int ignore)
{ 
  /* check files from thread 0 to n in loop till ProcessFile returns
	FALSE  */
  for (thr = 0; 
    ProcessFile (no, ctx, thr, longname, max, prefix, ignore);
       thr ++)
	;
  return (FALSE);
}


static int ProcessFileInNode (int no, int ctx, int thr, int longname, int max, char prefix[], int ignore)
{ /* check files from context 0 to n */
  for (ctx = 0; 
    ProcessFileInContext(no, ctx, thr, longname, max, prefix, ignore);
       ctx ++) 
    ; /* blank */
    return (FALSE);
}
/*
 * profile main function
 */

int main (int argc, char *argv[])
{
  int argno, start;
  int ch;
  int i;
  int max = 999999999;
  int errflag;
  char *file = "profile";
  char proffile[SIZE_OF_FILENAME], *dir;

#ifdef TAU_WINDOWS
  int optind = 0;
#else
  extern char *optarg;
  extern int optind;
#endif //TAU_WINDOWS
  

  dir = getenv("PROFILEDIR");
  if(dir != NULL) {
    file = strsave(strcat(strcat(dir,"/"),file));
  } 
#ifdef TAU_WINDOWS
  /* -- parse command line arguments ---------------------------------------- */
  
  errflag = FALSE;
  for( int j = 1; j < argc; j++)
  {
	  
	  char *argchar = argv[j];
	  switch(argchar[0])
	  {
	  case '-':
		  {
			  switch(argchar[1])
				{
				case 'c': /* -- sort according to number of *C*alls -- */
					compar = CallCmp;
					break;
				case 'b': /* -- sort according to number of subroutines called -- */
					compar = SubrCmp;
					break;
				case 'd': /* -- *D*ump output format (for racy) -- */
					dump = TRUE;
					break;
				case 'f': /* -- profile data *F*ile prefix -- */
					//A file name given.  The next option should be the filename.
					//Check to make sure that there is SOMETHING and
					//that it is likely to be a filename.
					if(argv[j + 1] == NULL)
					{
						//Set the error flag and break out.
						errflag = TRUE;
					}
					
					else
					{
						if(argv[j+1][0] == '-')
						{
							//The chances are that the filename was forgotten.
							//Give a warning and then proceed as if nothing we wrong.
							cout << "It is likely that you have forgotten to give the filename after the -f option!!!" << endl;
							//Set file to the filename given.
							file = argv[j + 1];
							//Now, I need to increment i as i+1 has already been processed.
							j = j+1;
						}
						//Otherwise, just act normally.
						else
						{
							//Set file to the filename given.
							file = argv[j + 1];
							//Now, I need to increment i as i+1 has already been processed.
							j = j+1;
						}
					}

					break;
				case 'l': /* -- *L*ist function table for debug purposes -- */
					list = TRUE;
					break;
				case 'm': /* -- sort according to *M*illiseconds -- */
					compar = MsecCmp;
					break;
				case 'e': /* -- sort according to Milliseconds (*E*xclusive per call)  -- */
					compar = MsecPerCallCmp;
					break;
				case 'i': /* -- sort according to Milliseconds (*I*nclusive per call)  -- */
					compar = CumMsecPerCallCmp;
					break;
				case 'n': /* -- print only first n *N*umber of funtions -- */
					
					//A number given.  The next option should be the number of functions.
					//Check to make sure that there is SOMETHING and that it is likely to be a number.
					if(argv[j + 1] == NULL)
					{
						//Set the error flag and break out.
						errflag = TRUE;
					}
					else
					{

						
						if(argv[j+1][0] == '-')
						{
							//The chances are that the number value was forgotten.
							//Give a warning and then proceed as if nothing were wrong.
							cout << "It is likely that you have forgotten to give the number after the -n option!!!" << endl;
							//Set max to the number given.
							max = atoi(argv[j + 1]);
							//Now, I need to increment i as i+1 has already been processed.
							j = j+1;
							
						}
						//Otherwise, just act normally.
						else
						{
							//Set max to the number given.
							max = atoi(argv[j + 1]);
							//Now, I need to increment i as i+1 has already been processed.
							j = j+1;
						}
						//******A bit of debugging code!!*********
						cout << "The number give was : " << max << endl;
					}
					break;
				case 'r': /* -- *R*everse sorting order -- */
					sign = -1;
					break;
				case 's': /* -- print only *S*ummary profile information -- */
					nodeprint = FALSE;
					break;
				case 't': /* -- sort according to *T*otal milliseconds -- */
					compar = CumMsecCmp; /* default mode */
					break;
				case 'v': /* -- sort according to standard de*V*iation --*/
					compar = StdDevCmp;  
					break; 
				case 'x': /* -- dump min and ma*X* information as well (default don't) -- */
					dumpminmax  = TRUE;
					break;
				default:
					errflag = TRUE;
					break;
				}
		  }
		  break;
	  
	  default:
		  errflag = TRUE;
		  break;
	  }
  }  
  
#else  
  /* -- parse command line arguments ---------------------------------------- */
  errflag = FALSE;
  while ( (ch = getopt (argc, argv, "cbdf:lmeivn:rstx")) != EOF ) {
    switch ( ch ) {
    case 'c': /* -- sort according to number of *C*alls -- */
      compar = CallCmp;
      break;
    case 'b': /* -- sort according to number of subroutines called -- */
      compar = SubrCmp;
      break;
    case 'd': /* -- *D*ump output format (for racy) -- */
      dump = TRUE;
      break;
    case 'f': /* -- profile data *F*ile prefix -- */
      file = optarg;
      break;
    case 'l': /* -- *L*ist function table for debug purposes -- */
      list = TRUE;
      break;
    case 'm': /* -- sort according to *M*illiseconds -- */
      compar = MsecCmp;
      break;
    case 'e': /* -- sort according to Milliseconds (*E*xclusive per call)  -- */
      compar = MsecPerCallCmp;
      break;
    case 'i': /* -- sort according to Milliseconds (*I*nclusive per call)  -- */
      compar = CumMsecPerCallCmp;
      break;
    case 'n': /* -- print only first n *N*umber of funtions -- */
      max = atoi(optarg);
      break;
    case 'r': /* -- *R*everse sorting order -- */
      sign = -1;
      break;
    case 's': /* -- print only *S*ummary profile information -- */
      nodeprint = FALSE;
      break;
    case 't': /* -- sort according to *T*otal milliseconds -- */
      compar = CumMsecCmp; /* default mode */
      break;
    case 'v': /* -- sort according to standard de*V*iation --*/
      compar = StdDevCmp;  
      break; 
    case 'x': /* -- dump min and ma*X* information as well (default don't) -- */
      dumpminmax  = TRUE;
      break;
    default:
      errflag = TRUE;
      break;
    }
  }

#endif //TAU_WINDOWS


  if ( errflag ) {
    fprintf (stderr, "usage: %s [-c|-b|-m|-t|-e|-i|-v] [-r] [-s] [-n num] [-f filename] [-l] [node numbers]\n", argv[0]);
    fprintf(stderr," -c : Sort according to number of Calls \n");
    fprintf(stderr," -b : Sort according to number of suBroutines called by a function \n");
    fprintf(stderr," -m : Sort according to Milliseconds (exclusive time total)\n");
    fprintf(stderr," -t : Sort according to Total milliseconds (inclusive time total)  (default)\n");
    fprintf(stderr," -e : Sort according to Exclusive time per call (msec/call)\n");
    fprintf(stderr," -i : Sort according to Inclusive time per call (total msec/call)\n");
    fprintf(stderr," -v : Sort according to Standard Deviation (excl usec)\n");
    fprintf(stderr," -r : Reverse sorting order\n");
    fprintf(stderr," -s : print only Summary profile information \n");
    fprintf(stderr," -n <num> : print only first <num> number of functions \n");
    fprintf(stderr," -f filename : specify full path and Filename without node ids\n");
    fprintf(stderr," -l : List all functions and exit\n");
    fprintf(stderr," [node numbers] : prints only info about all contexts/threads of given node numbers\n");

	
    exit (1);
  }

  /* ASSUMPTION : Dynamic profiling and long file names are coupled. We'd always
  have profile.n.c.t with it - new file format */

  if(optind == argc) 
    start = 0; 
  else 
    start = atoi(argv[optind]);

  sprintf(proffile,"%s.%d.0.0", file, start);

  if (!dump) { // This statement not in the dump protocol 
    printf("Reading Profile files in %s.*\n", file);
  } 

  if (IsDynamicProfiling(proffile)) { /* we don't need to read .ftab files */


    if ( optind == argc) {  
    /* files not specified by specific node nos list  on cmdline */
      for(argno = 0; FillFunctionDBInNode(argno, 0, 0, file); argno++) ;
    }
    else { /* 4 45 68 ... - process this list of node nos. */
      for (argno = optind; argno < argc; argno++)
	FillFunctionDBInNode(atoi(argv[argno]), 0, 0, file);
    }

    numfunc = funcDB.size(); /* sets global number of functions */

    InitFuncNameBuf();
    /* funcnamebuf and functagbuf are initialized here */

    if (list)  { 
      DumpFunctionNamesInDB();  /* for debugging purposes */
      exit(0); 
    } 
    else if (dump) { /* NOTE : Using TEMPLATED_FUNCTIONS as sig to RACY */
     // DumpFtabFile(file);
    /* used by racy - but here there's no depfile - for compatibility only */
      //printf ("default.dep\n%d templated_functions\n", numfunc);
      if(profilestats) 
	{
	  if(hwcounters) {
	    printf ("default.dep\n%d templated_functions_hw_counters -stddev\n", numfunc);
	    printf ("%%time       counts total counts       #call      #subrs count/call     stddev name\n");
	    // printf ("%%time       counts total counts       #call      #subrs count/call name\n");
	  } else {
	    printf ("default.dep\n%d templated_functions -stddev\n", numfunc);
	    printf ("%%time         msec   total msec       #call      #subrs  usec/call     stddev name\n");
	    //printf ("%%time         msec   total msec       #call      #subrs  usec/call name\n");
	  }
	} else //not profilestats
	  { 
	    if(hwcounters) {
	      printf ("default.dep\n%d templated_functions_hw_counters\n", numfunc);
	      printf ("%%time       counts total counts       #call      #subrs count/call name\n");
	    } else {
	      printf ("default.dep\n%d templated_functions\n", numfunc);
	      printf ("%%time         msec   total msec       #call      #subrs  usec/call name\n");
	    }
	  }//end if profilestats

    }//if(dump) 


    /* Process the files for data - second pass */
    /* iterate over nodes, contexts and threads */
    if ( optind == argc ) {
      for (argno = 0; ProcessFileDynamicInNode(argno, 0, 0, max, file); argno++);
    }
    else {
      for (argno = optind; argno < argc; argno++)
        ProcessFileDynamicInNode (atoi(argv[argno]), 0, 0,  max, file);
      argno -= optind;
    }
    if (files_processed > 0) PrintSummary (max, files_processed);

    return 0;
  /* End of Dynamic Profiling */
  } else { /* static profiling using .ftab .ctab files */

    ReadNameTable (file); 
    /* read profile.ftab file and register function names*/

    ReadEventTable (file); /* for hpc++ */
    if ( list ) {
      printf ("%s:\n", depfile);
      for (i=0; i<numfunc; i++) {
        if ( functagbuf[i] > 0 )
          printf ("%5d  %s\n", functagbuf[i], funcnamebuf[i]);
      }
      exit (0); /* exit after printing list */
    } else if ( dump ) {
      printf ("%s\n%d functions\n", depfile, numfunc);
      printf ("%%time         msec   total_msec       #call      #subrs  usec/call name\n");
    }
  }

  

  sprintf(proffile,"%s.%d", file,start);
  if(IsFilePresent(proffile))
  { /* pc++ files - profile.0, etc. Use ctx 0, thr 0 and longname = FALSE */
    if ( optind == argc ) {
      for (argno = 0; ProcessFile(argno, 0, 0, FALSE,  max, file, TRUE); argno++);
    }
    else {
      for (argno = optind; argno < argc; argno++)
        ProcessFile (atoi(argv[argno]), 0, 0, FALSE, max, file, FALSE);
      argno -= optind;
    }
  /* pc++ format - we know how many files were processed */
  if ( argno ) PrintSummary (max, argno);
  }
  else 
  {
    sprintf(proffile,"%s.%d.0.0", file, start);
    if (IsFilePresent(proffile))
    { /* hpc++ files profile.0.0.0 present  - use longnames = TRUE */
    
      /* iterate over nodes, contexts and threads */
      if ( optind == argc ) {
        for (argno = 0; ProcessFileInNode(argno, 0, 0, TRUE,  max, file, TRUE); argno++);
      }
      else {
        for (argno = optind; argno < argc; argno++)
          ProcessFileInNode (atoi(argv[argno]), 0, 0, TRUE, max, file, TRUE);
        argno -= optind;
	/* Note: since we're iterating, we cannot use ignore as FALSE for
	hpc++ (it searches for files) */
      }
      if (files_processed > 0) PrintSummary (max, files_processed);
    }
    else
    { 
     /* both profile.0 and profile.0.0.0 not found */
	printf("Error : profile file %s not found",proffile);
 	exit(1);
    }
  }


  exit (0);
}
/***************************************************************************
 * $RCSfile: pprof.cpp,v $   $Author: sameer $
 * $Revision: 1.26 $   $Date: 2001/08/24 16:45:56 $
 * POOMA_VERSION_ID: $Id: pprof.cpp,v 1.26 2001/08/24 16:45:56 sameer Exp $                                                   
 ***************************************************************************/


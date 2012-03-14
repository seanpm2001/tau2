/*
 * This file is part of the Score-P software (http://www.score-p.org)
 *
 * Copyright (c) 2009-2011,
 *    RWTH Aachen University, Germany
 *    Gesellschaft fuer numerische Simulation mbH Braunschweig, Germany
 *    Technische Universitaet Dresden, Germany
 *    University of Oregon, Eugene, USA
 *    Forschungszentrum Juelich GmbH, Germany
 *    German Research School for Simulation Sciences GmbH, Juelich/Aachen, Germany
 *    Technische Universitaet Muenchen, Germany
 *
 * See the COPYING file in the package base directory for details.
 *
 */
/****************************************************************************
**  SCALASCA    http://www.scalasca.org/                                   **
**  KOJAK       http://www.fz-juelich.de/jsc/kojak/            ///            **
*****************************************************************************
**  Copyright (c) 1998-2009                                                **
**  Forschungszentrum Juelich, Juelich Supercomputing Centre               **
**                                                                         **
**  See the file COPYRIGHT in the package base directory for details       **
****************************************************************************/

#include <config.h>
#include "opari2/pomp2_lib.h"


#include "pomp2_region_info.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Profile/Profiler.h>
#ifdef TAU_OPENMP
#ifndef _OPENMP
#define _OPENMP
#endif /* _OPENMP */
#endif /* TAU_OPENMP */



/* These two defines specify if we want region based views or construct based
views or both */
#ifdef TAU_OPARI_REGION
#define TAU_OPENMP_REGION_VIEW
#elif TAU_OPARI_CONSTRUCT
#define TAU_AGGREGATE_OPENMP_TIMINGS
#else
#define TAU_AGGREGATE_OPENMP_TIMINGS
#define TAU_OPENMP_REGION_VIEW
#endif /* in the default mode, define both! */

omp_lock_t tau_ompregdescr_lock; 
#define OpenMP TAU_USER
#define TAU_EMBEDDED_MAPPING 1

#define TAU_OPARI_CONSTURCT_TIMER(timer, name, type, group) void *TauGlobal##timer(void) \
{ static void *ptr = NULL; \
  Tau_profile_c_timer(&ptr, name, type, group, #group); \
  return ptr; \
} 

#define TAU_OPARI_CONSTURCT_TIMER_START(timer) { void *ptr = TauGlobal##timer(); \
    Tau_start_timer(ptr, 0, Tau_get_tid()); }

#define TAU_OPARI_CONSTURCT_TIMER_STOP(timer) { void *ptr = TauGlobal##timer(); \
    Tau_stop_timer(ptr, Tau_get_tid()); }

TAU_OPARI_CONSTURCT_TIMER(tatomic, "atomic enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tbarrier, "barrier enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tcriticalb, "critical begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tcriticale, "critical enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tfor, "for enter/exit", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER(tmaster, "master begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tparallelb, "parallel begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tparallelf, "parallel fork/join", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tsectionb, "section begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tsectione, "sections enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tsingleb, "single begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tsinglee, "single enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tworkshare, "workshare enter/exit", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER(tregion, "inst region begin/end", "[OpenMP]", OpenMP); 
TAU_OPARI_CONSTURCT_TIMER( tflush , "flush enter/exit", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( torderedb , "ordered begin/end", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( torderede , "ordered enter/exit", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( ttaskcreate , "task create begin/create end", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( ttask , "task begin/end", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( tuntiedcreate , "untied task create begin/end", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( tuntied , "untied task begin/end", "[OpenMP]", OpenMP);
TAU_OPARI_CONSTURCT_TIMER( ttaskwait , "taskwait begin/end", "[OpenMP]", OpenMP);


#define NUM_OMP_TYPES 22

static string  omp_names[22] = {"atomic enter/exit", "barrier enter/exit", "critical begin/end", 
			     "critical enter/exit", "for enter/exit", "master begin/end",
			     "parallel begin/end", "parallel fork/join", "section begin/end",
			     "sections enter/exit", "single begin/end", "single enter/exit",
			      "workshare enter/exit", "inst region begin/end", "flush enter/exit",
			     "ordered begin/end","ordered enter/exit","task create begin/end",
			"task begin/end","untied task create begin/end","untied task begin/end",
			"taskwait begin/end" };


#define TAU_OMP_ATOMIC      0
#define TAU_OMP_BARRIER     1
#define TAU_OMP_CRITICAL_BE 2
#define TAU_OMP_CRITICAL_EE 3
#define TAU_OMP_FOR_EE      4
#define TAU_OMP_MASTER_BE   5
#define TAU_OMP_PAR_BE      6
#define TAU_OMP_PAR_FJ      7
#define TAU_OMP_SECTION_BE  8
#define TAU_OMP_SECTION_EE  9
#define TAU_OMP_SINGLE_BE  10
#define TAU_OMP_SINGLE_EE  11
#define TAU_OMP_WORK_EE    12
#define TAU_OMP_INST_BE    13
#define TAU_OMP_FLUSH_EE   14
#define TAU_OMP_ORDERED_BE 15
#define TAU_OMP_ORDERED_EE 16
#define TAU_OMP_TASK_CREATE 17
#define TAU_OMP_TASK       18
#define TAU_OMP_UNTIED_TASK_CREATE_BE 19
#define TAU_OMP_UNTIED_TASK_BE 20
#define TAU_OMP_TASKWAIT_BE  21



static int omp_tracing    = 1;
static int omp_fin_called = 0;




/** @name Functions generated by the instrumenter */
/*@{*/
/**
 * Returns the number of instrumented regions.@n
 * The instrumenter scans all opari-created include files with nm and greps
 * the POMP2_INIT_uuid_numRegions() function calls. Here we return the sum of
 * all numRegions.
 */
extern size_t
POMP2_Get_num_regions();

/**
 * Init all opari-created regions.@n
 * The instrumentor scans all opari-created include files with nm and greps
 * the POMP2_INIT_uuid_numRegions() function calls. The instrumentor then
 * defines this functions by calling all grepped functions.
 */
extern void
POMP2_Init_regions();

/**
 * Returns the opari version.
 */
extern const char*
POMP2_Get_opari2_version();

/*@}*/

/** @brief All relevant information for a region is stored here */
typedef struct
{
    /** region type of construct */
    char*  rtype;
    /** critical or user region name */
    char*  name;
    /** sections only: number of sections */
    int    num_sections;

    /** start file name*/
    char*  start_file_name;
    /** line number 1*/
    int    start_line_1;
    /** line number 2*/
    int    start_line_2;

    /** end file name*/
    char*  end_file_name;
    /** line number 1*/
    int    end_line_1;
    /** line number 2*/
    int    end_line_2;
    /** region id*/
    size_t id;

    /** space for performance data*/
    void* data; 

} my_pomp2_region;


/** Id of the currently executing task*/
POMP2_Task_handle pomp2_current_task = 0;
#pragma omp threadprivate(pomp2_current_task)

/** Counter of tasks used to determine task ids for newly created ta*/
POMP2_Task_handle pomp2_task_counter = 1;
#pragma omp threadprivate(pomp2_task_counter)

POMP2_Task_handle
POMP2_Get_new_task_handle()
{
    return ( ( POMP2_Task_handle )omp_get_thread_num() << 32 ) + pomp2_task_counter++;
}

static void
free_my_pomp2_region_member( char** member )
{
    if ( *member )
    {
        free( *member );
        *member = 0;
    }
}

static void
free_my_pomp2_region_members( my_pomp2_region* region )
{
    if ( region )
    {
        free_my_pomp2_region_member( &region->rtype );
        free_my_pomp2_region_member( &region->name );
        free_my_pomp2_region_member( &region->start_file_name );
        free_my_pomp2_region_member( &region->end_file_name );
    }
}

static void
assignString( char**      destination,
              const char* source )
{
    assert( source );
    *destination = (char *)(malloc( strlen( source ) * sizeof( char ) + 1 ));
    strcpy( *destination, source );
}


static void
initDummyRegionFromPOMP2RegionInfo(
    my_pomp2_region*         pomp2_region,
    const POMP2_Region_info* pomp2RegionInfo )
{
    assignString( &( pomp2_region->rtype ),
                  pomp2RegionType2String( pomp2RegionInfo->mRegionType ) );

    assignString( &pomp2_region->start_file_name,
                  pomp2RegionInfo->mStartFileName );
    pomp2_region->start_line_1 = pomp2RegionInfo->mStartLine1;
    pomp2_region->start_line_2 = pomp2RegionInfo->mStartLine2;

    assignString( &pomp2_region->end_file_name,
                  pomp2RegionInfo->mEndFileName );
    pomp2_region->end_line_1 = pomp2RegionInfo->mEndLine1;
    pomp2_region->end_line_2 = pomp2RegionInfo->mEndLine2;

    if ( pomp2RegionInfo->mRegionType == POMP2_User_region )
    {
        assignString( &pomp2_region->name,
                      pomp2RegionInfo->mUserRegionName );
    }
    else if ( pomp2RegionInfo->mRegionType == POMP2_Critical && pomp2RegionInfo->mCriticalName )
    {
        assignString( &pomp2_region->name,
                      pomp2RegionInfo->mCriticalName );
    }

    pomp2_region->num_sections = pomp2RegionInfo->mNumSections;
}


/* TAU specific calls */
int tau_openmp_init(void)
{
  omp_init_lock(&tau_ompregdescr_lock);
  return 0;
}


  int tau_openmp_initialized = tau_openmp_init();
void TauStartOpenMPRegionTimer(my_pomp2_region *r, int index)
{
/* For any region, create a mapping between a region r and timer t and
   start the timer. */

  omp_set_lock(&tau_ompregdescr_lock);

  if (!r->data) {
#ifdef TAU_OPENMP_PARTITION_REGION
    FunctionInfo **flist = new FunctionInfo*[NUM_OMP_TYPES];
    for (int i=0; i < NUM_OMP_TYPES; i++) {
      char rname[1024], rtype[1024];
      sprintf(rname, "%s %s (%s)", r->name, r->sub_name, omp_names[i]);
      sprintf(rtype, "[OpenMP location: file:%s <%d, %d>]",
	      r->start_file_name, r->start_line_1, r->end_line_1);
      
      FunctionInfo *f = new FunctionInfo(rname, rtype, OpenMP, "OpenMP");
      flist[i] = f;
    }
    r->data = (void*)flist;
#else
    char rname[1024], rtype[1024];
    sprintf(rname, "%s %s", r->rtype, r->name);
    sprintf(rtype, "[OpenMP location: file:%s <%d, %d>]",
	    r->start_file_name, r->start_line_1, r->end_line_1);
    
    FunctionInfo *f = new FunctionInfo(rname, rtype, OpenMP, "OpenMP");
    r->data = (void*)f;
#endif
  }
  
#ifdef TAU_OPENMP_PARTITION_REGION
  FunctionInfo *f = ((FunctionInfo **)r->data)[index];
#else 
  FunctionInfo *f = (FunctionInfo *)r->data;
#endif
  Tau_start_timer(f, 0, Tau_create_tid());
  
  omp_unset_lock(&tau_ompregdescr_lock);
}


void TauStopOpenMPRegionTimer(my_pomp2_region  *r, int index)
{

#ifdef TAU_OPENMP_PARTITION_REGION
    FunctionInfo *f = ((FunctionInfo **)r->data)[index];
#else
    FunctionInfo *f = (FunctionInfo *)r->data;
#endif

      Tau_stop_timer(f, Tau_get_tid());
//This silently ignored bugs, 
//Let the measurement layer deal with problems with the profiler
//And report any errors
/*    TauGroup_t gr = f->GetProfileGroup();

    int tid = RtsLayer::myThread(); 
    Profiler *p =TauInternal_CurrentProfiler(tid); 
    if(p == NULL){
      // nothing, it must have been disabled/throttled
    } else if (p->ThisFunction == f) {

      Tau_stop_timer(f, Tau_create_tid());
    } else {
      // nothing, it must have been disabled/throttled
    }*/
}
/*
 * Global variables
 */

int              pomp2_tracing = 0;
my_pomp2_region* my_pomp2_regions;

/*
 * C pomp2 function library
 */



void
POMP2_Finalize()
{
    static int   pomp2_finalize_called = 0;
    size_t       i;
    const size_t nRegions = POMP2_Get_num_regions();

    if ( my_pomp2_regions )
    {
        for ( i = 0; i < nRegions; ++i )
        {
            free_my_pomp2_region_members( &my_pomp2_regions[ i ] );
        }
        free( my_pomp2_regions );
        my_pomp2_regions = 0;
    }

    if ( !pomp2_finalize_called )
    {
        pomp2_finalize_called = 1;
#ifdef DEBUG_PROF
        fprintf( stderr, "  0: finalize\n" );
#endif /* DEBUG_PROF */
    }
}

void
POMP2_Init()
{
    static int pomp2_init_called = 0;

    if ( !pomp2_init_called )
    {
        pomp2_init_called = 1;

        atexit( POMP2_Finalize );
#ifdef DEBUG_PROF
        fprintf( stderr, "  0: init  code\n" );
#endif /* DEBUG_PROF */

        /* Allocate memory for your POMP2_Get_num_regions() regions */
        my_pomp2_regions = (my_pomp2_region *)(calloc( POMP2_Get_num_regions(),
                                   sizeof( my_pomp2_region ) ));
        //pomp2_tpd_ = ( void* )malloc( sizeof( int ) );
        //pomp2_tpd_ = ( long )0;

        POMP2_Init_regions();

        pomp2_tracing = 1;
    }
}

void
POMP2_Off()
{
    pomp2_tracing = 0;
}

void
POMP2_On()
{
    pomp2_tracing = 1;
}

void
POMP2_Begin( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
//my_pomp2_region* region = *pomp2_handle;

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tregion);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_INST_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin region %s\n",
                 omp_get_thread_num(), region->name );
    }
#endif /* DEBUG_PROF */

}

void
POMP2_End( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_INST_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tregion); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   region %s\n",
                 omp_get_thread_num(), region->name );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Assign_handle( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
    static size_t count = 0;
    /* printf( "%d POMP2_Assign_handle: \"%s\"\n", (int)count, ctc_string ); */

    POMP2_Region_info pomp2RegionInfo;
    ctcString2RegionInfo( ctc_string, &pomp2RegionInfo );

    initDummyRegionFromPOMP2RegionInfo( &my_pomp2_regions[ count ], &pomp2RegionInfo );
    my_pomp2_regions[ count ].id = count;
#ifdef DEBUG_PROF
    printf( "assign_handle %d %s\n", ( int )count, my_pomp2_regions[ count ].rtype );
#endif /* DEBUG_PROF */

    *pomp2_handle = &my_pomp2_regions[ count ];

    freePOMP2RegionInfoMembers( &pomp2RegionInfo );
    ++count;
    assert( count <= POMP2_Get_num_regions() );
}

void
POMP2_Atomic_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tatomic);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(( my_pomp2_region*) *pomp2_handle,TAU_OMP_ATOMIC); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter atomic\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Atomic_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(( my_pomp2_region*) *pomp2_handle, TAU_OMP_ATOMIC);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tatomic); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */
#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  atomic\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Implicit_barrier_enter( POMP2_Region_handle* pomp2_handle,POMP2_Task_handle*   pomp2_old_task )
{
    POMP2_Barrier_enter( pomp2_handle, pomp2_old_task,  "" );
}

extern void
POMP2_Implicit_barrier_exit( POMP2_Region_handle* pomp2_handle, POMP2_Task_handle   pomp2_old_task )
{
    POMP2_Barrier_exit( pomp2_handle, pomp2_old_task );
}


void
POMP2_Barrier_enter( POMP2_Region_handle* pomp2_handle, POMP2_Task_handle*   pomp2_old_task, const char ctc_string[] )
{
    *pomp2_old_task = pomp2_current_task;

#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tbarrier);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_BARRIER); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        if ( region->rtype[ 0 ] == 'b' )
        {
            fprintf( stderr, "%3d: enter barrier\n", omp_get_thread_num() );
        }
        else
        {
            fprintf( stderr, "%3d: enter implicit barrier of %s\n",
                     omp_get_thread_num(), region->rtype );
        }
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Barrier_exit( POMP2_Region_handle* pomp2_handle, POMP2_Task_handle    pomp2_old_task  )
{
    pomp2_old_task = pomp2_current_task;

#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_BARRIER);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tbarrier); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        if ( region->rtype[ 0 ] == 'b' )
        {
            fprintf( stderr, "%3d: exit  barrier\n", omp_get_thread_num() );
        }
        else
        {
            fprintf( stderr, "%3d: exit  implicit barrier of %s\n",
                     omp_get_thread_num(), region->rtype );
        }
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Flush_enter( POMP2_Region_handle* pomp2_handle,
		   const char           ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tflush);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_FLUSH_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter flush\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Flush_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_FLUSH_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  //TAU_UGLOBAL_TIMER_STOP(tflush);
  TAU_OPARI_CONSTURCT_TIMER_STOP(tflush); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  flush\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Critical_begin( POMP2_Region_handle* pomp2_handle )
{
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tcriticalb);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_CRITICAL_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */
#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin critical %s\n",
                 omp_get_thread_num(), region->rtype );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Critical_end( POMP2_Region_handle* pomp2_handle )
{
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
    //my_pomp2_region* region = *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_CRITICAL_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tcriticalb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */
  

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   critical %s\n",
                 omp_get_thread_num(), region->name );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Critical_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tcriticale);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_CRITICAL_EE); 
#endif /* TAU_OPENMP_REGION_VIEW */


#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter critical %s\n",
                 omp_get_thread_num(), region->name );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Critical_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    //my_pomp2_region* region = *pomp2_handle;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_CRITICAL_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tcriticale); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */



#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  critical %s\n",
                 omp_get_thread_num(), region->name );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_For_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tfor); 
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_FOR_EE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter for\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_For_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    
#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_FOR_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tfor); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */
  // as in a stack. lifo

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  for\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Master_begin( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tmaster);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_MASTER_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */


#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin master\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Master_end( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_MASTER_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tmaster); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   master\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Parallel_begin( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tparallelb);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_PAR_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin parallel\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Parallel_end( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_PAR_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tparallelb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */


#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   parallel\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Parallel_fork( POMP2_Region_handle* pomp2_handle,
                     int                  if_clause, 
                     int                  num_threads,
                     POMP2_Task_handle*   pomp2_old_task,
                     const char           ctc_string[] )
{
    *pomp2_old_task = pomp2_current_task;

#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tparallelf);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_PAR_FJ); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: fork  parallel\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Parallel_join( POMP2_Region_handle* pomp2_handle, POMP2_Task_handle   pomp2_old_task )
{
    pomp2_old_task = pomp2_current_task;

#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_PAR_FJ);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tparallelf); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: join  parallel\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Section_begin( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tsectionb);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_SECTION_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin section\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Section_end( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_SECTION_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tsectionb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   section\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Sections_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tsectione);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_SECTION_EE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter sections (%d)\n",
                 omp_get_thread_num(), region->num_sections );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Sections_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_SECTION_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tsectione); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  sections\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Single_begin( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tsingleb);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_SINGLE_BE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin single\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Single_end( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_SINGLE_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tsingleb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end   single\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Single_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tsinglee);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_SINGLE_EE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter single\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Single_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_SINGLE_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tsinglee); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  single\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Workshare_enter( POMP2_Region_handle* pomp2_handle, const char ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tworkshare);
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef TAU_OPENMP_REGION_VIEW
  TauStartOpenMPRegionTimer(region, TAU_OMP_WORK_EE); 
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter workshare\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Workshare_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;    

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_WORK_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tworkshare); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit  workshare\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
}

void
POMP2_Ordered_begin( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_ORDERED_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS

  TAU_OPARI_CONSTURCT_TIMER_START(torderedb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF

    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin ordered\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Ordered_end( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_ORDERED_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(torderedb); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end ordered\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Ordered_enter( POMP2_Region_handle* pomp2_handle,
                    const char           ctc_string[] )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_ORDERED_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(torderede); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: enter ordered\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Ordered_exit( POMP2_Region_handle* pomp2_handle )
{
#pragma omp critical
    if ( *pomp2_handle == NULL )
    {
        POMP2_Init();
    }

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_ORDERED_EE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(torderede); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF

    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: exit ordered\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}


void
POMP2_Task_create_begin( POMP2_Region_handle* pomp2_handle,
                         POMP2_Task_handle*   pomp2_old_task,
                         POMP2_Task_handle*   pomp2_new_task,
                         int                  pomp2_if,
                         const char           ctc_string[])
{
    *pomp2_old_task = pomp2_current_task;
    *pomp2_new_task = POMP2_Get_new_task_handle();

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASK_CREATE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(ttaskcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: task create begin\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Task_create_end( POMP2_Region_handle* pomp2_handle,
                       POMP2_Task_handle    pomp2_old_task )
{
    pomp2_current_task = pomp2_old_task;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASK_CREATE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(ttaskcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF
 

   if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: task create end\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Task_begin( POMP2_Region_handle* pomp2_handle,
                  POMP2_Task_handle    pomp2_task )
{
    pomp2_current_task = pomp2_task;

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASK);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(ttaskcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: task begin\n", omp_get_thread_num() );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Task_end( POMP2_Region_handle* pomp2_handle )
{

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASK);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(ttaskcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF

    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: task end\n", omp_get_thread_num());
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Untied_task_create_begin( POMP2_Region_handle* pomp2_handle,
                                POMP2_Task_handle*   pomp2_new_task,
                                POMP2_Task_handle*   pomp2_old_task,
                                int                  pomp2_if,
                                const char           ctc_string[] )
{
    *pomp2_new_task = POMP2_Get_new_task_handle();
    *pomp2_old_task = pomp2_current_task;

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_UNTIED_TASK_CREATE_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tuntiedcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: create  untied task\n", omp_get_thread_num() );
        fprintf( stderr, "%3d:         suspend task %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Untied_task_create_end( POMP2_Region_handle* pomp2_handle,
                              POMP2_Task_handle    pomp2_old_task )
{
    pomp2_current_task = pomp2_old_task;

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_UNTIED_TASK_CREATE_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tuntiedcreate); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: created  untied task\n", omp_get_thread_num() );
        fprintf( stderr, "%3d:          resume task %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Untied_task_begin( POMP2_Region_handle* pomp2_handle,
                         POMP2_Task_handle    pomp2_parent_task )
{
    pomp2_current_task = POMP2_Get_new_task_handle();


    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_UNTIED_TASK_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(tuntied); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF

    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: start  untied task %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Untied_task_end( POMP2_Region_handle* pomp2_handle )
{

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_UNTIED_TASK_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(tuntied); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF

    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end  untied task %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Taskwait_begin( POMP2_Region_handle* pomp2_handle,
                      POMP2_Task_handle*   pomp2_old_task,
                      const char           ctc_string[] )
{
    *pomp2_old_task = pomp2_current_task;

    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASKWAIT_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_START(ttaskwait); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF


    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: begin  taskwait\n", omp_get_thread_num() );
        fprintf( stderr, "%3d:  suspend task: %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}

void
POMP2_Taskwait_end( POMP2_Region_handle* pomp2_handle,
                    POMP2_Task_handle    pomp2_old_task )
{
    pomp2_current_task = pomp2_old_task;
    my_pomp2_region* region = ( my_pomp2_region*) *pomp2_handle;

#ifdef TAU_OPENMP_REGION_VIEW
  TauStopOpenMPRegionTimer(region, TAU_OMP_TASKWAIT_BE);
#endif /* TAU_OPENMP_REGION_VIEW */

#ifdef TAU_AGGREGATE_OPENMP_TIMINGS
  TAU_OPARI_CONSTURCT_TIMER_STOP(ttaskwait); /* global timer stop */
#endif /* TAU_AGGREGATE_OPENMP_TIMINGS */

#ifdef DEBUG_PROF



    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: end  taskwait\n", omp_get_thread_num() );
        fprintf( stderr, "%3d: resume task: %lld\n", omp_get_thread_num(), pomp2_current_task );
    }
#endif /*DEBUG_PROF*/
}


/*
   *----------------------------------------------------------------
 * C Wrapper for OpenMP API
 ******----------------------------------------------------------------
 */

void
POMP2_Init_lock( omp_lock_t* s )
{
  TAU_PROFILE("omp_init_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: init lock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_init_lock( s );
}

void
POMP2_Destroy_lock( omp_lock_t* s )
{
  TAU_PROFILE("omp_destroy_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: destroy lock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_destroy_lock( s );
}

void
POMP2_Set_lock( omp_lock_t* s )
{
  TAU_PROFILE("omp_set_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: set lock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_set_lock( s );
}

void
POMP2_Unset_lock( omp_lock_t* s )
{
  TAU_PROFILE("omp_unset_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: unset lock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_unset_lock( s );
}

int
POMP2_Test_lock( omp_lock_t* s )
{
  TAU_PROFILE("omp_test_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: test lock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    return omp_test_lock( s );
}

void
POMP2_Init_nest_lock( omp_nest_lock_t* s )
{
  TAU_PROFILE("omp_init_nest_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: init nestlock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_init_nest_lock( s );
}

void
POMP2_Destroy_nest_lock( omp_nest_lock_t* s )
{
  TAU_PROFILE("omp_destroy_nest_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: destroy nestlock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_destroy_nest_lock( s );
}

void
POMP2_Set_nest_lock( omp_nest_lock_t* s )
{
  TAU_PROFILE("omp_set_nest_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: set nestlock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_set_nest_lock( s );
}

void
POMP2_Unset_nest_lock( omp_nest_lock_t* s )
{
  TAU_PROFILE("omp_unset_nest_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: unset nestlock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    omp_unset_nest_lock( s );
}

int
POMP2_Test_nest_lock( omp_nest_lock_t* s )
{
  TAU_PROFILE("omp_test_nest_lock", "[OpenMP]", OpenMP);

#ifdef DEBUG_PROF
    if ( pomp2_tracing )
    {
        fprintf( stderr, "%3d: test nestlock\n", omp_get_thread_num() );
    }
#endif /* DEBUG_PROF */
    return omp_test_nest_lock( s );
}

/* *INDENT-OFF*  */
/** @todo include again if c part is ready */
#if 0
/*
   *----------------------------------------------------------------
 * Fortran  Wrapper for OpenMP API
 ******----------------------------------------------------------------
 */
/* *INDENT-OFF*  */
#if defined(__ICC) || defined(__ECC) || defined(_SX)
#define CALLFSUB(a) a
#else
#define CALLFSUB(a) FSUB(a)
#endif

void FSUB(POMP2_Init_lock)(omp_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: init lock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_init_lock)(s);
}

void FSUB(POMP2_Destroy_lock)(omp_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: destroy lock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_destroy_lock)(s);
}

void FSUB(POMP2_Set_lock)(omp_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: set lock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_set_lock)(s);
}

void FSUB(POMP2_Unset_lock)(omp_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: unset lock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_unset_lock)(s);
}

int  FSUB(POMP2_Test_lock)(omp_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: test lock\n", omp_get_thread_num());
  }
  return CALLFSUB(omp_test_lock)(s);
}

#ifndef __osf__
void FSUB(POMP2_Init_nest_lock)(omp_nest_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: init nestlock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_init_nest_lock)(s);
}

void FSUB(POMP2_Destroy_nest_lock)(omp_nest_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: destroy nestlock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_destroy_nest_lock)(s);
}

void FSUB(POMP2_Set_nest_lock)(omp_nest_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: set nestlock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_set_nest_lock)(s);
}

void FSUB(POMP2_Unset_nest_lock)(omp_nest_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: unset nestlock\n", omp_get_thread_num());
  }
  CALLFSUB(omp_unset_nest_lock)(s);
}

int  FSUB(POMP2_Test_nest_lock)(omp_nest_lock_t *s) {
  if ( pomp2_tracing ) {
    fprintf(stderr, "%3d: test nestlock\n", omp_get_thread_num());
  }
  return CALLFSUB(omp_test_nest_lock)(s);
}
#endif
#endif /*0*/
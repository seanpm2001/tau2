/*
 * Copyright 1993-2007 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.  Users and possessors of this source code
 * are hereby granted a nonexclusive, royalty-free license to use this code
 * in individual and commercial software.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

/* Computation of eigenvalues of a large symmetric, tridiagonal matrix */

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

// includes, project
#include "cutil.h"
#include "config.h"
#include "structs.h"
#include "util.h"
#include "matlab.h"

#include "bisect_large.cuh"

// includes, kernels
#include "bisect_kernel_large.cu"
#include "bisect_kernel_large_onei.cu"
#include "bisect_kernel_large_multi.cu"


////////////////////////////////////////////////////////////////////////////////
//! Initialize variables and memory for result
//! @param  result handles to memory
//! @param  matrix_size  size of the matrix
////////////////////////////////////////////////////////////////////////////////
void
initResultDataLargeMatrix( ResultDataLarge& result, const unsigned int mat_size) {

  // helper variables to initialize memory
    unsigned int zero = 0;
    unsigned int mat_size_f = sizeof(float) * mat_size;
    unsigned int mat_size_ui = sizeof(unsigned int) * mat_size;

    float* tempf = (float*) malloc( mat_size_f);
    unsigned int* tempui = (unsigned int*) malloc( mat_size_ui);
    for( unsigned int i = 0; i < mat_size; ++i) {
      tempf[i] = 0.0f;
      tempui[i] = 0;
    }

    // number of intervals containing only one eigenvalue after the first step
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_num_one, 
                                sizeof( unsigned int)) );
    CUDA_SAFE_CALL( cudaMemcpy( result.g_num_one, &zero, sizeof(unsigned int),
                                cudaMemcpyHostToDevice));

    // number of (thread) blocks of intervals with multiple eigenvalues after
    // the first iteration
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_num_blocks_mult, 
                                sizeof(unsigned int)));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_num_blocks_mult, &zero, 
                                sizeof(unsigned int),
                                cudaMemcpyHostToDevice ));

    
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_left_one, mat_size_f));
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_right_one, mat_size_f));
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_pos_one, mat_size_ui));

    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_left_mult, mat_size_f));
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_right_mult, mat_size_f));
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_left_count_mult,
                                mat_size_ui));
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_right_count_mult,
                                mat_size_ui));

    CUDA_SAFE_CALL( cudaMemcpy( result.g_left_one, tempf, mat_size_f,
                                cudaMemcpyHostToDevice));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_right_one, tempf, mat_size_f,
                                cudaMemcpyHostToDevice));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_pos_one, tempui, mat_size_ui,
                                cudaMemcpyHostToDevice));

    CUDA_SAFE_CALL( cudaMemcpy( result.g_left_mult, tempf, mat_size_f,
                                cudaMemcpyHostToDevice));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_right_mult, tempf, mat_size_f,
                                cudaMemcpyHostToDevice));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_left_count_mult, tempui, mat_size_ui,
                                cudaMemcpyHostToDevice));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_right_count_mult, tempui, mat_size_ui,
                                cudaMemcpyHostToDevice));

    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_blocks_mult, mat_size_ui));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_blocks_mult, tempui, mat_size_ui,
                                cudaMemcpyHostToDevice ));
    CUDA_SAFE_CALL(cudaMalloc((void**) &result.g_blocks_mult_sum, mat_size_ui));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_blocks_mult_sum, tempui, mat_size_ui,
                                cudaMemcpyHostToDevice ));

    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_lambda_mult, mat_size_f));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_lambda_mult, tempf, mat_size_f,
                                cudaMemcpyHostToDevice )); 
    CUDA_SAFE_CALL( cudaMalloc( (void**) &result.g_pos_mult, mat_size_ui));
    CUDA_SAFE_CALL( cudaMemcpy( result.g_pos_mult, tempf, mat_size_ui,
                                cudaMemcpyHostToDevice )); 
}
   
////////////////////////////////////////////////////////////////////////////////
//! Cleanup result memory
//! @param result  handles to memory
////////////////////////////////////////////////////////////////////////////////
void
cleanupResultDataLargeMatrix( ResultDataLarge& result) {

  CUDA_SAFE_CALL( cudaFree( result.g_num_one));
  CUDA_SAFE_CALL( cudaFree( result.g_num_blocks_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_left_one));
  CUDA_SAFE_CALL( cudaFree( result.g_right_one));
  CUDA_SAFE_CALL( cudaFree( result.g_pos_one));
  CUDA_SAFE_CALL( cudaFree( result.g_left_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_right_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_left_count_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_right_count_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_blocks_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_blocks_mult_sum));
  CUDA_SAFE_CALL( cudaFree( result.g_lambda_mult));
  CUDA_SAFE_CALL( cudaFree( result.g_pos_mult));
}

////////////////////////////////////////////////////////////////////////////////
//! Run the kernels to compute the eigenvalues for large matrices
//! @param  input   handles to input data
//! @param  result  handles to result data 
//! @param  mat_size  matrix size
//! @param  precision  desired precision of eigenvalues
//! @param  lg  lower limit of Gerschgorin interval
//! @param  ug  upper limit of Gerschgorin interval
//! @param  iterations  number of iterations (for timing)
////////////////////////////////////////////////////////////////////////////////
void
computeEigenvaluesLargeMatrix( const InputData& input, const ResultDataLarge& result,
                               const unsigned int mat_size, const float precision,
                               const float lg, const float ug,
                               const unsigned int iterations )
{   
    dim3  blocks( 1, 1, 1);
    dim3  threads( MAX_THREADS_BLOCK, 1, 1);

    unsigned int timer_step1 = 0;
    unsigned int timer_step2_one = 0;
    unsigned int timer_step2_mult = 0;
    unsigned int timer_total = 0;
    CUT_SAFE_CALL( cutCreateTimer( &timer_step1));
    CUT_SAFE_CALL( cutCreateTimer( &timer_step2_one));
    CUT_SAFE_CALL( cutCreateTimer( &timer_step2_mult));
    CUT_SAFE_CALL( cutCreateTimer( &timer_total));

    CUT_SAFE_CALL( cutStartTimer( timer_total));

    // do for multiple iterations to improve timing accuracy
    for( unsigned int iter = 0; iter < iterations; ++iter) {

      CUT_SAFE_CALL( cutStartTimer( timer_step1));
      bisectKernelLarge<<< blocks, threads >>>
        ( input.g_a, input.g_b, mat_size,
          lg, ug, 0, mat_size, precision,
          result.g_num_one, result.g_num_blocks_mult,
          result.g_left_one, result.g_right_one, result.g_pos_one,
          result.g_left_mult, result.g_right_mult,
          result.g_left_count_mult, result.g_right_count_mult,
          result.g_blocks_mult, result.g_blocks_mult_sum
        );
      CUDA_SAFE_CALL( cudaThreadSynchronize());
      CUT_SAFE_CALL( cutStopTimer( timer_step1));
      CUT_CHECK_ERROR( "Kernel launch failed.");

      // get the number of intervals containing one eigenvalue after the first
      // processing step
      unsigned int num_one_intervals;
      CUDA_SAFE_CALL( cudaMemcpy( &num_one_intervals, result.g_num_one, 
                                  sizeof(unsigned int),
                                  cudaMemcpyDeviceToHost));

      dim3 grid_onei;
      grid_onei.x = getNumBlocksLinear( num_one_intervals, MAX_THREADS_BLOCK);
      dim3 threads_onei;
      // use always max number of available threads to better balance load times
      // for matrix data
      threads_onei.x = MAX_THREADS_BLOCK;  

      // compute eigenvalues for intervals that contained only one eigenvalue 
      // after the first processing step
      CUT_SAFE_CALL( cutStartTimer( timer_step2_one));

      bisectKernelLarge_OneIntervals<<< grid_onei , threads_onei >>>
          ( input.g_a, input.g_b, mat_size, num_one_intervals,
            result.g_left_one, result.g_right_one, result.g_pos_one,
            precision
          ); 

      CUDA_SAFE_CALL( cudaThreadSynchronize());
      CUT_SAFE_CALL( cutStopTimer( timer_step2_one));

      // process intervals that contained more than one eigenvalue after
      // the first processing step

      // get the number of blocks of intervals that contain, in total when
      // each interval contains only one eigenvalue, not more than 
      // MAX_THREADS_BLOCK threads
      unsigned int  num_blocks_mult = 0;
      CUDA_SAFE_CALL( cudaMemcpy( &num_blocks_mult, result.g_num_blocks_mult,
                                  sizeof( unsigned int),
                                  cudaMemcpyDeviceToHost));

      // setup the execution environment
      dim3  grid_mult( num_blocks_mult, 1, 1);
      dim3  threads_mult( MAX_THREADS_BLOCK, 1, 1);

      CUT_SAFE_CALL( cutStartTimer( timer_step2_mult));
      
      bisectKernelLarge_MultIntervals<<< grid_mult, threads_mult >>>
          ( input.g_a, input.g_b, mat_size,
            result.g_blocks_mult, result.g_blocks_mult_sum,
            result.g_left_mult, result.g_right_mult,
            result.g_left_count_mult, result.g_right_count_mult,
            result.g_lambda_mult, result.g_pos_mult,
            precision
          );

      CUT_SAFE_CALL( cutStopTimer( timer_step2_mult));
      CUT_CHECK_ERROR( "bisectKernelLarge_MultIntervals() FAILED.");
    }

    CUT_SAFE_CALL( cutStopTimer( timer_total));


    printf( "Average time step 1: %f ms\n", 
            cutGetTimerValue( timer_step1) / (float) iterations );
    printf( "Average time step 2, one intervals: %f ms\n", 
            cutGetTimerValue( timer_step2_one) / (float) iterations );
    printf( "Average time step 2, mult intervals: %f ms\n", 
            cutGetTimerValue( timer_step2_mult) / (float) iterations );

    printf( "Average time TOTAL: %f ms\n", 
            cutGetTimerValue( timer_total) / (float) iterations );

    CUT_SAFE_CALL( cutDeleteTimer( timer_step1));
    CUT_SAFE_CALL( cutDeleteTimer( timer_step2_one));
    CUT_SAFE_CALL( cutDeleteTimer( timer_step2_mult));
    CUT_SAFE_CALL( cutDeleteTimer( timer_total));
}

////////////////////////////////////////////////////////////////////////////////
//! Process the result, that is obtain result from device and do simple sanity
//! checking
//! @param  input   handles to input data
//! @param  result  handles to result data
//! @param  mat_size  matrix size
//! @param  filename  output filename
////////////////////////////////////////////////////////////////////////////////
void
processResultDataLargeMatrix( const InputData& input, const ResultDataLarge& result,
                              const unsigned int mat_size, 
                              const char* filename, 
                              const unsigned int user_defined, char* exec_path) 
{
  const unsigned int mat_size_ui = sizeof(unsigned int) * mat_size;
  const unsigned int mat_size_f  = sizeof(float) * mat_size;

  // copy data from intervals that contained more than one eigenvalue after
  // the first processing step
  float* lambda_mult = (float*) malloc( sizeof(float) * mat_size);
  CUDA_SAFE_CALL( cudaMemcpy( lambda_mult, result.g_lambda_mult,
                              sizeof(float) * mat_size,
                              cudaMemcpyDeviceToHost ));
  unsigned int* pos_mult = 
      (unsigned int*) malloc( sizeof(unsigned int) * mat_size);
  CUDA_SAFE_CALL( cudaMemcpy( pos_mult, result.g_pos_mult,
                              sizeof(unsigned int) * mat_size,
                              cudaMemcpyDeviceToHost ));

  unsigned int* blocks_mult_sum = 
    (unsigned int*) malloc( sizeof(unsigned int) * mat_size);
  CUDA_SAFE_CALL( cudaMemcpy( blocks_mult_sum, result.g_blocks_mult_sum,
                              sizeof( unsigned int) * mat_size,
                              cudaMemcpyDeviceToHost ));

  unsigned int num_one_intervals;
  CUDA_SAFE_CALL( cudaMemcpy( &num_one_intervals, result.g_num_one, 
                               sizeof(unsigned int),
                               cudaMemcpyDeviceToHost));

  unsigned int sum_blocks_mult = mat_size - num_one_intervals;


  // copy data for intervals that contained one eigenvalue after the first 
  // processing step
  float* left_one = (float*) malloc( mat_size_f);
  float* right_one = (float*) malloc( mat_size_f);
  unsigned int* pos_one = (unsigned int*) malloc( mat_size_ui);
  CUDA_SAFE_CALL( cudaMemcpy( left_one, result.g_left_one, mat_size_f,
                              cudaMemcpyDeviceToHost) );
  CUDA_SAFE_CALL( cudaMemcpy( right_one, result.g_right_one, mat_size_f,
                              cudaMemcpyDeviceToHost) );
  CUDA_SAFE_CALL( cudaMemcpy( pos_one, result.g_pos_one, mat_size_ui,
                                cudaMemcpyDeviceToHost) );
 
  // extract eigenvalues
  float* eigenvals = (float*) malloc( mat_size_f);

  // singleton intervals generated in the second step
  for( unsigned int i = 0; i < sum_blocks_mult; ++i) {

    eigenvals[pos_mult[i] - 1] = lambda_mult[i];
  }

  // singleton intervals generated in the first step
  unsigned int index = 0;
  for( unsigned int i = 0; i < num_one_intervals; ++i, ++index) {

    eigenvals[pos_one[i] - 1] = left_one[i];
  }

  if( 1 == user_defined) {
    // store result
    writeTridiagSymMatlab( filename, input.a, input.b+1, eigenvals, mat_size);
    // CUT_SAFE_CALL( cutWriteFilef( filename, eigenvals, mat_size, 0.0f));
  }
  else {
    
    // compare with reference solution

    float* reference = NULL;
    unsigned int input_data_size = 0;

    char* ref_path = cutFindFilePath( "reference.dat", exec_path);
    CUT_CONDITION( 0 != ref_path);
    CUT_SAFE_CALL( cutReadFilef( ref_path, &reference, &input_data_size));
    CUT_CONDITION( input_data_size == mat_size);

    // there's an imprecision of Sturm count computation which makes an 
    // additional offset necessary
    float tolerance = 1.0e-5f + 5.0e-6f;

    if( CUTTrue == cutComparefe( reference, eigenvals, mat_size, tolerance)) {
      printf( "\nPASSED.\n");
    }
    else {
      printf( "FAILED.\n");
    }

    cutFree( ref_path);
    cutFree( reference);
  }

  freePtr( eigenvals);
  freePtr( lambda_mult);
  freePtr( pos_mult);
  freePtr( blocks_mult_sum); 
  freePtr( left_one); 
  freePtr( right_one); 
  freePtr( pos_one); 
}


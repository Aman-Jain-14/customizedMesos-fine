// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>

#include <mesos/executor.hpp>

#include <stout/duration.hpp>
#include <stout/os.hpp>

// include the following by yuquan.
#include <cstdlib>
#include <ctime>
#include <unistd.h>

// include the following by Aman.
#include <thread>
#include <chrono>
#include <csignal>
#include <limits.h>
#include <sys/time.h>
#include <omp.h>

using namespace mesos;
using namespace std;

using std::cout;
using std::endl;
using std::string;

std::sig_atomic_t volatile done;

#ifndef STREAM_ARRAY_SIZE
#   define STREAM_ARRAY_SIZE  44000000
#endif
#ifdef NTIMES
#if NTIMES<=1
#   define NTIMES 10
#endif
#endif
#ifndef NTIMES
#   define NTIMES 10
#endif
#ifndef OFFSET
#   define OFFSET 0
#endif
# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

static STREAM_TYPE  a[STREAM_ARRAY_SIZE+OFFSET],
      b[STREAM_ARRAY_SIZE+OFFSET],
      c[STREAM_ARRAY_SIZE+OFFSET];

static double avgtime[4] = {0}, maxtime[4] = {0},
    mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};

/*static char *label[4] = {"Copy:      ", "Scale:     ",
    "Add:       ", "Triad:     "};*/

/*static double bytes[4] = {
    2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    2 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE,
    3 * sizeof(STREAM_TYPE) * STREAM_ARRAY_SIZE
    };*/

extern double mysecond();
extern void checkSTREAMresults();
#ifdef TUNED
extern void tuned_STREAM_Copy();
extern void tuned_STREAM_Scale(STREAM_TYPE scalar);
extern void tuned_STREAM_Add();
extern void tuned_STREAM_Triad(STREAM_TYPE scalar);
#endif
#ifdef _OPENMP
extern int omp_get_num_threads();
#endif

#ifndef M
# define  M 20
#endif

int checktick()
    {
    int   i, minDelta, Delta;
    double  t1, t2, timesfound[M];

/*  Collect a sequence of M unique time values from the system. */

    for (i = 0; i < M; i++) {
  t1 = mysecond();
  while( ((t2=mysecond()) - t1) < 1.0E-6 )
      ;
  timesfound[i] = t1 = t2;
  }

/*
 * Determine the minimum difference between these M values.
 * This result will be our estimate (in microseconds) for the
 * clock granularity.
 */

    minDelta = 1000000;
    for (i = 1; i < M; i++) {
  Delta = (int)( 1.0E6 * (timesfound[i]-timesfound[i-1]));
  minDelta = MIN(minDelta, MAX(Delta,0));
  }

   return(minDelta);
    }



/* A gettimeofday routine to give access to the wall
   clock timer on most UNIX-like systems.  */

#include <sys/time.h>

double mysecond()
{
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        i++;
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
void checkSTREAMresults ()
{
  STREAM_TYPE aj,bj,cj,scalar;
  STREAM_TYPE aSumErr,bSumErr,cSumErr;
  STREAM_TYPE aAvgErr,bAvgErr,cAvgErr;
  double epsilon;
  ssize_t j;
  int k,ierr,err;

    /* reproduce initialization */
  aj = 1.0;
  bj = 2.0;
  cj = 0.0;
    /* a[] is modified during timing check */
  aj = 2.0E0 * aj;
    /* now execute timing loop */
  scalar = 3.0;
  for (k=0; k<NTIMES; k++)
        {
            cj = aj;
            bj = scalar*cj;
            cj = aj+bj;
            aj = bj+scalar*cj;
        }

    /* accumulate deltas between observed and expected results */
  aSumErr = 0.0;
  bSumErr = 0.0;
  cSumErr = 0.0;
  for (j=0; j<STREAM_ARRAY_SIZE; j++) {
    aSumErr += abs(a[j] - aj);
    bSumErr += abs(b[j] - bj);
    cSumErr += abs(c[j] - cj);
    // if (j == 417) printf("Index 417: c[j]: %f, cj: %f\n",c[j],cj); // MCCALPIN
  }
  aAvgErr = aSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;
  bAvgErr = bSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;
  cAvgErr = cSumErr / (STREAM_TYPE) STREAM_ARRAY_SIZE;

  if (sizeof(STREAM_TYPE) == 4) {
    epsilon = 1.e-6;
  }
  else if (sizeof(STREAM_TYPE) == 8) {
    epsilon = 1.e-13;
  }
  else {
    printf("WEIRD: sizeof(STREAM_TYPE) = %lu\n",sizeof(STREAM_TYPE));
    epsilon = 1.e-6;
  }

  err = 0;
  if (abs(aAvgErr/aj) > epsilon) {
    err++;
    printf ("Failed Validation on array a[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
    printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",aj,aAvgErr,abs(aAvgErr)/aj);
    ierr = 0;
    for (j=0; j<STREAM_ARRAY_SIZE; j++) {
      if (abs(a[j]/aj-1.0) > epsilon) {
        ierr++;
#ifdef VERBOSE
        if (ierr < 10) {
          printf("         array a: index: %ld, expected: %e, observed: %e, relative error: %e\n",
            j,aj,a[j],abs((aj-a[j])/aAvgErr));
        }
#endif
      }
    }
    printf("     For array a[], %d errors were found.\n",ierr);
  }
  if (abs(bAvgErr/bj) > epsilon) {
    err++;
    printf ("Failed Validation on array b[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
    printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",bj,bAvgErr,abs(bAvgErr)/bj);
    printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
    ierr = 0;
    for (j=0; j<STREAM_ARRAY_SIZE; j++) {
      if (abs(b[j]/bj-1.0) > epsilon) {
        ierr++;
#ifdef VERBOSE
        if (ierr < 10) {
          printf("         array b: index: %ld, expected: %e, observed: %e, relative error: %e\n",
            j,bj,b[j],abs((bj-b[j])/bAvgErr));
        }
#endif
      }
    }
    printf("     For array b[], %d errors were found.\n",ierr);
  }
  if (abs(cAvgErr/cj) > epsilon) {
    err++;
    printf ("Failed Validation on array c[], AvgRelAbsErr > epsilon (%e)\n",epsilon);
    printf ("     Expected Value: %e, AvgAbsErr: %e, AvgRelAbsErr: %e\n",cj,cAvgErr,abs(cAvgErr)/cj);
    printf ("     AvgRelAbsErr > Epsilon (%e)\n",epsilon);
    ierr = 0;
    for (j=0; j<STREAM_ARRAY_SIZE; j++) {
      if (abs(c[j]/cj-1.0) > epsilon) {
        ierr++;
#ifdef VERBOSE
        if (ierr < 10) {
          printf("         array c: index: %ld, expected: %e, observed: %e, relative error: %e\n",
            j,cj,c[j],abs((cj-c[j])/cAvgErr));
        }
#endif
      }
    }
    printf("     For array c[], %d errors were found.\n",ierr);
  }
  if (err == 0) {
    printf ("Solution Validates: avg error less than %e on all three arrays\n",epsilon);
  }
#ifdef VERBOSE
  printf ("Results Validation Verbose Results: \n");
  printf ("    Expected a(1), b(1), c(1): %f %f %f \n",aj,bj,cj);
  printf ("    Observed a(1), b(1), c(1): %f %f %f \n",a[1],b[1],c[1]);
  printf ("    Rel Errors on a, b, c:     %e %e %e \n",abs(aAvgErr/aj),abs(bAvgErr/bj),abs(cAvgErr/cj));
#endif
}

#ifdef TUNED
/* stubs for "tuned" versions of the kernels */
void tuned_STREAM_Copy()
{
  ssize_t j;
#pragma omp parallel for
        for (j=0; j<STREAM_ARRAY_SIZE; j++)
            c[j] = a[j];
}

void tuned_STREAM_Scale(STREAM_TYPE scalar)
{
  ssize_t j;
#pragma omp parallel for
  for (j=0; j<STREAM_ARRAY_SIZE; j++)
      b[j] = scalar*c[j];
}

void tuned_STREAM_Add()
{
  ssize_t j;
#pragma omp parallel for
  for (j=0; j<STREAM_ARRAY_SIZE; j++)
      c[j] = a[j]+b[j];
}

void tuned_STREAM_Triad(STREAM_TYPE scalar)
{
  ssize_t j;
#pragma omp parallel for
  for (j=0; j<STREAM_ARRAY_SIZE; j++)
      a[j] = b[j]+scalar*c[j];
}
/* end of stubs for the "tuned" versions of the kernels */
#endif

class TestExecutor : public Executor
{
public:
  virtual ~TestExecutor() {}

  virtual void registered(ExecutorDriver* driver,
                          const ExecutorInfo& executorInfo,
                          const FrameworkInfo& frameworkInfo,
                          const SlaveInfo& slaveInfo)
  {
    //cout << "Registered executor on " << slaveInfo.hostname() << endl;
  }

  virtual void reregistered(ExecutorDriver* driver,
                            const SlaveInfo& slaveInfo)
  {
    //cout << "Re-registered executor on " << slaveInfo.hostname() << endl;
  }

  virtual void disconnected(ExecutorDriver* driver) {}

  virtual void launchTask(ExecutorDriver* driver, const TaskInfo& task)
  {
    cout << "M\n";
    cout << task.task_id().value() << endl;

    TaskStatus status;
    status.mutable_task_id()->MergeFrom(task.task_id());
    status.set_state(TASK_RUNNING);

    driver->sendStatusUpdate(status);

    // This is where one would perform the requested task.
    // yuquan: add some pseudo-workload - sleeping for random sec.
    /*srand(time(nullptr));
    sleep(std::rand()%60);  // sleep for 0 ~ 60 sec*/

    //Aman: added Stream Memory Intensive Micro-Benchmark

    // Time at Start of Execution
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "%T") << '\n';

        int     quantum, checktick();
        //int     BytesPerWord;
        int     k;
        ssize_t   j;
        STREAM_TYPE   scalar;
        double    t, times[4][NTIMES];

    auto start = chrono::high_resolution_clock::now();
        /* --- SETUP --- determine precision and check timing --- */

        /*printf(HLINE);
        printf("STREAM version $Revision: 5.10 $\n");
        printf(HLINE);*/
        //BytesPerWord = sizeof(STREAM_TYPE);
        /*printf("This system uses %d bytes per array element.\n",
      BytesPerWord);*/

        /*printf(HLINE);
    #ifdef N
        printf("*****  WARNING: ******\n");
        printf("      It appears that you set the preprocessor variable N when compiling this code.\n");
        printf("      This version of the code uses the preprocesor variable STREAM_ARRAY_SIZE to control the array size\n");
        printf("      Reverting to default value of STREAM_ARRAY_SIZE=%llu\n",(unsigned long long) STREAM_ARRAY_SIZE);
        printf("*****  WARNING: ******\n");
    #endif*/

        /*printf("Array size = %llu (elements), Offset = %d (elements)\n" , (unsigned long long) STREAM_ARRAY_SIZE, OFFSET);
        printf("Memory per array = %.1f MiB (= %.1f GiB).\n", 
      BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0),
      BytesPerWord * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.0/1024.0));
        printf("Total memory required = %.1f MiB (= %.1f GiB).\n",
      (3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024.),
      (3.0 * BytesPerWord) * ( (double) STREAM_ARRAY_SIZE / 1024.0/1024./1024.));*/
        /*printf("Each kernel will be executed %d times.\n", NTIMES);
        printf(" The *best* time for each kernel (excluding the first iteration)\n"); 
        printf(" will be used to compute the reported bandwidth.\n");*/

    #ifdef _OPENMP
        //printf(HLINE);
    #pragma omp parallel 
        {
    #pragma omp master
      {
          k = omp_get_num_threads();
          //printf ("Number of Threads requested = %i\n",k);
            }
        }
    #endif

    #ifdef _OPENMP
      k = 0;
    #pragma omp parallel
    #pragma omp atomic 
        k++;
        //printf ("Number of Threads counted = %i\n",k);
    #endif

        /* Get initial value for system clock. */
    #pragma omp parallel for
        for (j=0; j<STREAM_ARRAY_SIZE; j++) {
          a[j] = 1.0;
          b[j] = 2.0;
          c[j] = 0.0;
      }

        //printf(HLINE);
        if  ( (quantum = checktick()) >= 1) ;
      /*printf("Your clock granularity/precision appears to be "
          "%d microseconds.\n", quantum);*/
        else {
      /*printf("Your clock granularity appears to be "
          "less than one microsecond.\n");*/
      quantum = 1;
        }

        t = mysecond();
    #pragma omp parallel for
        for (j = 0; j < STREAM_ARRAY_SIZE; j++)
        a[j] = 2.0E0 * a[j];
        t = 1.0E6 * (mysecond() - t);

        /*printf("Each test below will take on the order"
      " of %d microseconds.\n", (int) t  );
        printf("   (= %d clock ticks)\n", (int) (t/quantum) );*/
        /*printf("Increase the size of the arrays if this shows that\n");
        printf("you are not getting at least 20 clock ticks per test.\n");*/

        /*printf(HLINE);

        printf("WARNING -- The above is only a rough guideline.\n");
        printf("For best results, please be sure you know the\n");
        printf("precision of your system timer.\n");
        printf(HLINE);*/
        
        /*  --- MAIN LOOP --- repeat test cases NTIMES times --- */

        scalar = 3.0;
        for (k=0; k<NTIMES; k++)
      {
      times[0][k] = mysecond();
    #ifdef TUNED
            tuned_STREAM_Copy();
    #else
    #pragma omp parallel for
      for (j=0; j<STREAM_ARRAY_SIZE; j++)
          c[j] = a[j];
    #endif
      times[0][k] = mysecond() - times[0][k];
      
      times[1][k] = mysecond();
    #ifdef TUNED
            tuned_STREAM_Scale(scalar);
    #else
    #pragma omp parallel for
      for (j=0; j<STREAM_ARRAY_SIZE; j++)
          b[j] = scalar*c[j];
    #endif
      times[1][k] = mysecond() - times[1][k];
      
      times[2][k] = mysecond();
    #ifdef TUNED
            tuned_STREAM_Add();
    #else
    #pragma omp parallel for
      for (j=0; j<STREAM_ARRAY_SIZE; j++)
          c[j] = a[j]+b[j];
    #endif
      times[2][k] = mysecond() - times[2][k];
      
      times[3][k] = mysecond();
    #ifdef TUNED
            tuned_STREAM_Triad(scalar);
    #else
    #pragma omp parallel for
      for (j=0; j<STREAM_ARRAY_SIZE; j++)
          a[j] = b[j]+scalar*c[j];
    #endif
      times[3][k] = mysecond() - times[3][k];
      }

        /*  --- SUMMARY --- */

        for (k=1; k<NTIMES; k++) /* note -- skip first iteration */
      {
      for (j=0; j<4; j++)
          {
          avgtime[j] = avgtime[j] + times[j][k];
          mintime[j] = MIN(mintime[j], times[j][k]);
          maxtime[j] = MAX(maxtime[j], times[j][k]);
          }
      }
        
        /*printf("Function    Best Rate MB/s  Avg time     Min time     Max time\n");
        for (j=0; j<4; j++) {
        avgtime[j] = avgtime[j]/(double)(NTIMES-1);

        printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
             1.0E-06 * bytes[j]/mintime[j],
             avgtime[j],
             mintime[j],
             maxtime[j]);
        }
        printf(HLINE);*/

        /* --- Check Results --- */
        //checkSTREAMresults();
        //printf(HLINE);

    //srand(time(nullptr));
    sleep(9);

    auto end= chrono::high_resolution_clock::now();

    //Time at Finish of Execution
    now = std::chrono::system_clock::now();
    now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "%T") << '\n';
    
    //Total Time Taken for execution
    cout <<chrono::duration_cast<chrono::microseconds>(end - start).count()/1000000.00<<"\n";
    
    //cout << "\nFinishing task " << task.task_id().value() << endl;

    status.mutable_task_id()->MergeFrom(task.task_id());
    status.set_state(TASK_FINISHED);

    driver->sendStatusUpdate(status);
  }

  virtual void killTask(ExecutorDriver* driver, const TaskID& taskId) {}
  virtual void frameworkMessage(ExecutorDriver* driver, const string& data) {}
  virtual void shutdown(ExecutorDriver* driver) {}
  virtual void error(ExecutorDriver* driver, const string& message) {}

  void counter(long long int &rf)
  {
    while (!done)
    {
        rf++;  // or whatever; make sure this returns frequently
    }
  }
};


int main(int argc, char** argv)
{
  TestExecutor executor;
  MesosExecutorDriver driver(&executor);
  return driver.run() == DRIVER_STOPPED ? 0 : 1;
}

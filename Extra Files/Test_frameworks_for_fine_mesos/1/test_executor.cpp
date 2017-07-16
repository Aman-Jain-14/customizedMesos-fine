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

using namespace mesos;
using namespace std;

using std::cout;
using std::endl;
using std::string;

std::sig_atomic_t volatile done;

void game_over(int)
{
  done = 1;
}

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
    cout << "C\n";
    cout << task.task_id().value() << endl;
    
    TaskStatus status;
    status.mutable_task_id()->MergeFrom(task.task_id());
    status.set_state(TASK_RUNNING);

    driver->sendStatusUpdate(status);

    // This is where one would perform the requested task.
    // yuquan: add some pseudo-workload - sleeping for random sec.
    /*srand(time(nullptr));
    sleep(std::rand()%60);  // sleep for 0 ~ 60 sec*/

    //Aman: added multi-threaded counter
    //Program executes for 5 seconds (can be changed), will
    //comapre the value received after execution
    thread t1[16];
    long long int n[16], sum;
    int lim;
    lim = 4;
    done = 0;
    std::signal(SIGALRM, game_over);
    alarm(15); // this program will self-destruct in 5 seconds

    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "%T") << '\n';

    for(int i=0 ; i<16 ; i++)
      n[i] = 0;
    sum = 0;

    auto start= chrono::high_resolution_clock::now();
    for(int i=0 ; i<lim ; i++)
    {
      t1[i] = thread(&TestExecutor::counter,this, ref(n[i]));
    }

    for(int i=0 ; i<lim ; i++)
      t1[i].join();

    for(int i=0 ; i<16 ; i++)
      sum += n[i];

    auto end= chrono::high_resolution_clock::now();

    now = std::chrono::system_clock::now();
    now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "%T") << '\n';

    cout <<chrono::duration_cast<chrono::microseconds>(end - start).count()/1000000.00 <<"\n";

    //cout<<"\nSum = "<<sum<<"\n";
    
    //cout << "Finishing task " << task.task_id().value() << endl;

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

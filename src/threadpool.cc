#include "threadpool.h"


/*
 * Constructor
 *************/
ThreadPool::ThreadPool(int numThreads): m_workers(numThreads), m_bTerminate(false)
{
    for(std::unique_ptr<std::thread>& worker: m_workers)
    {
        worker.reset(new std::thread(&ThreadPool::loop, this));
    }
}

/* Copy constructor
**********************/

ThreadPool::ThreadPool(const ThreadPool& other) 
{
    std::unique_lock<std::mutex> lockList(m_lockJobsList);
    m_jobs = other.m_jobs;

}
/*
 * Destructor
 ************/
ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lockList(m_lockJobsList);
        m_bTerminate = true;
        m_notifyJob.notify_all();
    }

    for(std::unique_ptr<std::thread>& worker: m_workers)
    {
        worker->join();
    }
}

/*
 * Schedule a job

 ****************/

void ThreadPool::enqueueJob(Task&& task) 
{
      add(std::forward<Task>(task));
}
    
void ThreadPool::enqueueJob(const Task& task) 
{
       add(task);
}

template<typename F>
void ThreadPool::add(F&& x)
{
    std::unique_lock<std::mutex> lockList(m_lockJobsList);
    m_jobs.push_back(std::forward<F>(x));
    m_notifyJob.notify_one();
}


/*
 * Retrieve the next job to execute
 **********************************/
using Task = std::function<void()>;
Task ThreadPool::getNextJob()
{
    std::unique_lock<std::mutex> lockList(m_lockJobsList);

    while(!m_bTerminate)
    {
        if(!m_jobs.empty())
        {
          Task job = m_jobs.front();
            m_jobs.pop_front();
            return job;
        }

        m_notifyJob.wait(lockList);
    }

    throw Terminated("Thread terminated");
}


/*
 * Function executed by each worker
 **********************************/
void ThreadPool::loop()
{
    try
    {
        for(;;)
        {
            Task job = getNextJob();
            job();
        }
    }
    catch(Terminated& e)
    {
    }
}
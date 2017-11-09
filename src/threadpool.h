#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <thread>
#include <vector>
#include <list>
#include <memory>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <tuple>

/**
 * @brief The ThreadPool class launches a pre-defined number of threads and
 *        keeps them ready to execute jobs.
 **/

class ThreadPool
{
public:

    /**
     * @brief Initializes the pool and launches the threads.
     * @param numThreads number of threads to launch.
     */
    using Task = std::function<void()>;
    ThreadPool(int numThreads);
    ThreadPool(const ThreadPool& other);


    /**
     * @brief Destructors
     * Sends a "terminate" signal to the threads and waits for
     *  their termination.
     *  The threads will complete the currently running job
     *  before checking for the "terminate" flag.
     */
    virtual ~ThreadPool();
    /**
     * @brief Schedule a job for execution.
     *
     * The first available thread will pick up the job and run it.
     *
     * @param job             a function that executes the job. It is called
     *                         in the thread that picked it up
                            
     */

    void enqueueJob(Task&& task) ;
    void enqueueJob(const Task& task) ;
    template<typename F>
    void add(F&& x );
  
private:
    /**
     * @brief Function executed in each worker thread.
     *
     * Runs until the termination flag m_bTerminate is set to true
     *  in the class destructor
     */
    void loop();


    /**
     * @brief Returns the next job scheduled for execution.
     *
     * The function blocks if the list of scheduled jobs is empty until
     *  a new job is scheduled or until m_bTerminate is set to true
     *  by the class destructor, in which case it throws Terminated.
     *
     * When a valid job is found it is removed from the queue and
     *  returned to the caller.
     *
     * @return the next job to execute
     */
    Task getNextJob();


    /**
     * @brief Contains the running working threads (workers).
     */
    std::vector<std::unique_ptr<std::thread> > m_workers;


    /**
     * @brief Queue of jobs scheduled for execution and not yet executed.
     */
    std::list<Task > m_jobs;


    /**
     * @brief Mutex used to access the queue of scheduled jobs (m_jobs).
     */
    std::mutex m_lockJobsList;


    /**
     * @brief Condition variable used to notify that a new job has been
     *        inserted in the queue (m_jobs).
     */
    std::condition_variable m_notifyJob;


    /**
     * @brief This flag is set to true by the class destructor to signal
     *         the worker threads that they have to terminate.
     */
    std::atomic<bool> m_bTerminate;


    /**
     * @brief This exception is thrown by getNextJob() when the flag
     *         m_bTerminate has been set.
     */
    class Terminated: public std::runtime_error
    {
    public:
        Terminated(const std::string& what): std::runtime_error(what) {}
    };

};

#endif //THREADPOOL_H
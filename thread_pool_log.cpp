#include<assert.h>
#include<zlib.h>
#include <sys/types.h>    
#include <sys/stat.h> 
#include <sstream>
#include "thread_pool_log.hpp"

namespace log_thread_pool{

	std::unique_ptr<LogFile> active_logger = nullptr;

	LogFile::LogFile(const std::string& file_name,
                 double max_size,
                 unsigned int max_file_num,
                 bool compress,
                 log_level mlevel)
    : file_name_(file_name), max_size_(max_size), max_file_num_(max_file_num),
      compress_(compress), m_level(mlevel)
	{
	    assert(max_file_num_ > 0);
	    cur_size_ = GetFileSize();
	    ofs_.open(file_name_, std::ofstream::out|std::ofstream::app);
	}

	LogFile::~LogFile()
	{
	    if (ofs_.is_open()) {
	        ofs_.close();
	    }
	}

	void LogFile::Append(std::string msg)
	{
	    double msg_size = (double)msg.size();
	    if (cur_size_+ msg_size >= max_size_) {
	        Rotate();
	    }

	    ofs_ << std::forward<std::string>(msg) << std::endl;
	    cur_size_ += msg_size;
	}

	void LogFile::compress_file(const char *oldfile, const char *new_file)
	{
	    // gzFile gf = gzopen(new_file, "wb");
	    // if (gf == NULL) {
	    //     std::cout << "gzopen() failed" << std::endl;
	    //     return;
	    // }

	    // const int BUF_LEN = 500;
	    // char buf[BUF_LEN];
	    // size_t reads = 0;

	    // FILE *fd = fopen(oldfile, "rb");
	    // if (fd == NULL) {
	    //     std::cout << "fopen(" << oldfile << ") failed" << std::endl;
	    // }

	    // while((reads = fread(buf, 1, BUF_LEN, fd)) > 0) {
	    //     if (ferror(fd)) {
	    //         std::cout << "fread() failed!" << std::endl;
	    //         break;
	    //     }
	    //     gzwrite(gf, buf, reads);
	    // }

	    // fclose(fd);
	    // gzclose(gf);
	}

	void LogFile::Rotate()
	{
	    if (ofs_.is_open()) {
	        ofs_.close();
	    }

	    std::string history_file = NextHistoryFile();
	    if (compress_) {
	        compress_file(file_name_.c_str(), history_file.c_str());
	    } else {
	        std::rename(file_name_.c_str(), history_file.c_str());
	    }
	    ofs_.open(file_name_, std::ofstream::out|std::ofstream::trunc);
	    cur_size_ = 0;
	}

	std::string LogFile::NextHistoryFile()
	{
	    static int next_file_no = 0;
	    int file_num = (next_file_no++) % max_file_num_;
	    return file_name_ + "." + std::to_string(file_num);
	}

	double LogFile::GetFileSize()
	{
	    struct stat statbuf;
	    if (stat(file_name_.c_str(), &statbuf) == 0) {
	        return (double)statbuf.st_size;
	    } else {
	        perror("Faild to get log file size");
	        return 0;
	    }
	}

	void LogFile::debug(const std::string& msg)
	{
		if (m_level >= log_level::debug)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			Append(msg);
		}
	}

	void LogFile::info(const std::string& msg)
	{
		if (m_level >= log_level::info)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			Append(msg);
		}
	}

	void LogFile::warn(const std::string& msg)
	{
		if (m_level >= log_level::warn)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			Append(msg);
		}
	}

	void LogFile::error(const std::string& msg)
	{
		if (m_level >= log_level::error)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			Append(msg);
		}
	}

	void Debug(const std::string& msg)
	{
		if(active_logger)
			active_logger->debug(msg);
	}

	void Info(const std::string& msg)
	{
		if(active_logger)
			active_logger->debug(msg);
	}

	void Warn(const std::string& msg)
	{
		if(active_logger)
			active_logger->debug(msg);
	}

	void Error(const std::string& msg)
	{
		if(active_logger)
			active_logger->debug(msg);
	}

	
	std::string get_tid(){
	  std::stringstream tmp;
	  tmp << std::this_thread::get_id();
	  return tmp.str();
	}

	ThreadPool::ThreadPool()
		:m_mutex(), 
		m_cond(), 
		m_isStarted(false)
	{ }

	ThreadPool::~ThreadPool()
	{
		if(m_isStarted)
			stop();
	}

	void ThreadPool::start(){
		assert(m_threads.empty());
		m_isStarted = true;
		m_threads.reserve(kInitThreadsSize);
		for(int i=0; i<kInitThreadsSize; i++)
			m_threads.push_back(new std::thread(std::bind(&ThreadPool::threadLoop, this)));
	}

	void ThreadPool::stop(){
		Debug("thread_pool::stop() is be executed, stop threads!");
		std::unique_lock<std::mutex> lock(m_mutex);
		m_isStarted = false;
		m_cond.notify_all();
		Debug("thread_pool::stop() notifyAll().");

		for(Threads::iterator it = m_threads.begin(); it!=m_threads.end(); it++)
		{
			(*it)->join();
			delete *it;
		}
		m_threads.clear();
	}

	void ThreadPool::threadLoop(){
		Debug("thread_pool::threadLopp() is be executed, tid:" + get_tid() + "start.");
		while(m_isStarted)
		{
			Task task = take();
			if(task){
				task();
			}
		}
		Debug("thread_pool::threadLoop() tid : " + get_tid() + " exit.");
	}

	void ThreadPool::addTask(const Task& task)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		TaskPair taskPair(level2, task);
		m_tasks.push(taskPair);
		m_cond.notify_one();
	}

	void ThreadPool::addTask(const TaskPair& taskPair)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_tasks.push(taskPair);
		m_cond.notify_one();
	}

	ThreadPool::Task ThreadPool::take()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while(m_tasks.empty() && m_isStarted){
			Debug("ThreadPool::take() is be executed, tid: " + get_tid() + " wait");
			m_cond.wait(lock);
		}
		Debug("ThreadPool::take() tid: " + get_tid() + " wakeup.");

		Task task;
		Tasks::size_type size = m_tasks.size();
		if(!m_tasks.empty() && m_isStarted)
		{
			task = m_tasks.top().second;
			m_tasks.pop();
			assert(size - 1 == m_tasks.size());
		}
		return task;
	}


}
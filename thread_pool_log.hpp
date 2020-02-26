#ifndef _THREAD_POOL_LOG_HPP_
#define _THREAD_POOL_LOG_HPP_

#include<fstream>
#include<iostream>
#include<vector>
#include<thread>
#include<queue>
#include<utility>
#include<functional>
#include<mutex>
#include<condition_variable>

namespace log_thread_pool{

	class LogFile{
	public:
		//日志级别设定
		enum class log_level{
			error = 0,
			warn = 1,
			info = 2,
			debug = 3
		};

	public:
		//禁止复制拷贝
		LogFile(const LogFile&) = delete;
		LogFile& operator=(const LogFile &) = delete;

		//参数依次是：日志文件名、日志文件大小上限、最大历史日志文件数、是否压缩、日志记录级别
		LogFile(const std::string&, double, unsigned int, bool, log_level);

		~LogFile();

		//向日志文件追加内容
		void Append(std::string msg);
		//压缩日志文件
		void compress_file(const char *oldfile, const char *new_file);

	public:
		void debug(const std::string& msg);
		void info(const std::string& msg);
		void warn(const std::string& msg);
		void error(const std::string& msg);

	private:
		//日志文件转储
		void Rotate();
		//获取当前日志文件大小
		double GetFileSize();
		//返回下一个历史日志文件名
		std::string NextHistoryFile();

	private:
		std::ofstream ofs_;
		std::string file_name_;   //日志文件名
		double cur_size_;         //当前日志文件大小
		double max_size_;         //日志文件大小上限
		unsigned int max_file_num_;  //最大历史日志文件数
		bool compress_;           //是否压缩日志文件
		log_level m_level;        //需要记录的日志最小级别
		std::mutex m_mutex;       //互斥锁
	};

	extern std::unique_ptr<LogFile> active_logger;
	void Debug(const std::string& msg);
	void Info(const std::string& msg);
	void Warn(const std::string& msg);
	void Error(const std:: string& msg);
	std::string get_tid();

	class ThreadPool{
	public:
		static const int kInitThreadsSize = 3;    //初始化线程池线程数
		enum taskPriorityE{ level0, level1, level2,};  //任务优先级
		typedef std::function<void()> Task;
		typedef std::pair<taskPriorityE, Task> TaskPair;

		ThreadPool();
		~ThreadPool();

		void start();
		void stop();
		void addTask(const Task&);      //无优先级任务添加
		void addTask(const TaskPair&);  //有优先级任务添加

	private:
		ThreadPool(const ThreadPool&);    //通过定义为私有函数的方式禁止复制构造
		const ThreadPool& operator=(const ThreadPool&);//禁止复制拷贝

		struct TaskPriorityCmp{
			bool operator()(const ThreadPool::TaskPair p1, const ThreadPool::TaskPair p2)
			{
				return p1.first > p2.first; //first小的值优先
			}
		};

		void threadLoop();
		Task take();

		typedef std::vector<std::thread*> Threads;
		typedef std::priority_queue<TaskPair, std::vector<TaskPair>, TaskPriorityCmp> Tasks;
		Threads m_threads;
		Tasks m_tasks;

		std::mutex m_mutex;
		std::condition_variable m_cond;
		bool m_isStarted;
};

}
#endif

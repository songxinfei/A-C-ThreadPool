#include <iostream>
#include <chrono>
#include<thread>
#include <condition_variable>
#include "thread_pool_log.hpp"

std::mutex g_mutex;


void priorityFunc(){
	for(int i=1; i<4; i++)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::lock_guard<std::mutex> lock(g_mutex);
		log_thread_pool::Debug("priorityFunc() [at thread [ " + log_thread_pool::get_tid() + " ] output");
	}
}

void testFunc(){
	for(int i=1; i<4; i++)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::lock_guard<std::mutex> lock(g_mutex);
		log_thread_pool::Debug("testFunc() [at thread [ " + log_thread_pool::get_tid() + " ] output");
	}
}

int main(){
	log_thread_pool::active_logger = std::unique_ptr<log_thread_pool::LogFile>(new log_thread_pool::LogFile(
															"test_log.txt", 512, 8, false, 
															log_thread_pool::LogFile::log_level::debug));
	log_thread_pool::ThreadPool threadPool;
	threadPool.start();
	for(int i=0; i<5; i++)
		threadPool.addTask(testFunc);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	threadPool.addTask(log_thread_pool::ThreadPool::TaskPair(log_thread_pool::ThreadPool::level0, priorityFunc));

	getchar();
	return 0;
}
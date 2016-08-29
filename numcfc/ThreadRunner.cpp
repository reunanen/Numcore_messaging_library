
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2012 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "ThreadRunner.h"
#include "Time.h"

#include <map>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <assert.h>

namespace numcfc {

class ThreadRunner::Impl
{
public:
	Impl() {
		m_notified = false;
		m_supposedToStop = false;
	}

	void Wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (!m_notified) {
			m_condition.wait(lock);
		}
		m_notified = false;
	}

	bool Wait(double maxNumberOfSecondsToWait)
	{
		numcfc::TimeElapsed te;
		std::unique_lock<std::mutex> lock(m_mutex);
		while (!m_notified && te.GetElapsedSeconds() < maxNumberOfSecondsToWait) {
			double maxWaitTime = maxNumberOfSecondsToWait - te.GetElapsedSeconds();
			if (maxWaitTime < 0) {
				maxWaitTime = 0;
			}
			m_condition.wait_for(lock, std::chrono::microseconds(static_cast<int>(maxWaitTime * 1e6)));
		}
		bool retVal = m_notified;
		m_notified = false;
		return retVal;
	}

	void Notify()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_notified = true;
		}
		m_condition.notify_one();	
	}

	void Join()
	{
		if (!m_joinedAlready) {
			assert(m_thread.joinable());
			m_thread.join();
			m_joinedAlready = true;
		}
		else {
			assert(!m_thread.joinable());
		}
	}

	mutable std::mutex m_threadObjectMutex;
	std::thread m_thread;
	bool m_supposedToStop;
	
private:
	std::mutex m_mutex;
	std::condition_variable m_condition;

	bool m_notified;
	bool m_joinedAlready = false;
};

ThreadRunner::ThreadRunner()
{
	pimpl_ = new ThreadRunner::Impl;
}

ThreadRunner::~ThreadRunner()
{
	AskThreadToStop();
	JoinThread();
	delete pimpl_;
}

void ThreadRunner::StartThread()
{
	std::unique_lock<std::mutex> lock(pimpl_->m_threadObjectMutex);

	pimpl_->m_supposedToStop = false;
	assert(!pimpl_->m_thread.joinable());

	pimpl_->m_thread = std::thread(RunThread, std::ref(*this));
}

void ThreadRunner::RunThread(ThreadRunner& threadRunner)
{
	const auto threadStartTime = std::chrono::high_resolution_clock::now();
		
	try {
		threadRunner.operator ()();
	}
	catch (...) {
		;
	}
}

void ThreadRunner::AskThreadToStop()
{
	std::unique_lock<std::mutex> lock(pimpl_->m_threadObjectMutex);
	pimpl_->m_supposedToStop = true;
	pimpl_->Notify();
}

bool ThreadRunner::IsSupposedToStop() const
{
	return pimpl_->m_supposedToStop;
}

void ThreadRunner::Wait()
{
	pimpl_->Wait();
}

bool ThreadRunner::Wait(double maxNumberOfSecondsToWait)
{
	return pimpl_->Wait(maxNumberOfSecondsToWait);
}

void ThreadRunner::Notify()
{
	pimpl_->Notify();
}

void ThreadRunner::JoinThread()
{
	pimpl_->Join();
}


class MultiThreadRunner::Impl
{
public:
	typedef std::map< std::string, ThreadRunner::Impl > ThreadImpls;
	ThreadImpls m_threads;
};

MultiThreadRunner::MultiThreadRunner()
{
	pimpl_ = new MultiThreadRunner::Impl;
}

MultiThreadRunner::~MultiThreadRunner()
{
	AskAllThreadsToStop();
	while (!pimpl_->m_threads.empty()) {
		numcfc::SleepMinimal();
	}
	delete pimpl_;
}

bool MultiThreadRunner::StartThread(const std::string& threadTask)
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i == pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = pimpl_->m_threads[threadTask];

		std::unique_lock<std::mutex> lock(entry.m_threadObjectMutex);
		entry.m_thread = std::thread(RunThread, std::ref(*this), threadTask);
		return true;
	}
	else {
		return false;
	}
}

void MultiThreadRunner::RunThread(MultiThreadRunner& threadRunner, const std::string& threadTask)
{
	const auto threadStartTime = std::chrono::high_resolution_clock::now();
	try {
		threadRunner.operator ()(threadTask);
	}
	catch (...) {
		;
	}
	threadRunner.pimpl_->m_threads.erase(threadTask);
}

bool MultiThreadRunner::AskThreadToStop(const std::string& threadTask)
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i != pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = i->second;
		entry.m_supposedToStop = true;
		return true;
	}
	else {
		return false;
	}
}

void MultiThreadRunner::AskAllThreadsToStop()
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.begin();
	Impl::ThreadImpls::iterator iEnd = pimpl_->m_threads.end();
	for (; i != iEnd; i++) {
		ThreadRunner::Impl& entry = i->second;
		entry.m_supposedToStop = true;		
	}
}

bool MultiThreadRunner::IsSupposedToStop(const std::string& threadTask) const
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i != pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = i->second;
		return entry.m_supposedToStop;
	}
	else {
		return true;
	}
}

void MultiThreadRunner::Wait(const std::string& threadTask)
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i != pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = i->second;
		entry.Wait();
	}
}

void MultiThreadRunner::Notify(const std::string& threadTask)
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i != pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = i->second;
		entry.Notify();
	}
}

bool MultiThreadRunner::JoinThread(const std::string& threadTask)
{
	Impl::ThreadImpls::iterator i = pimpl_->m_threads.find(threadTask);
	if (i != pimpl_->m_threads.end()) {
		ThreadRunner::Impl& entry = i->second;		
		entry.Join();
		return true;
	}
	else {
		return false;
	}
}

}
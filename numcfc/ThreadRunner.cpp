
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2012 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "ThreadRunner.h"
#include "Time.h"
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace numcfc {

class ThreadRunner::Impl
{
public:
	Impl() {
		m_notified = false;
		m_supposedToStop = false;
	}
	Impl(const Impl& rhs) {
		m_notified = rhs.m_notified;
		m_supposedToStop = rhs.m_supposedToStop;
		
		boost::mutex::scoped_lock lockRhs(rhs.m_threadObjectMutex);
		boost::mutex::scoped_lock lock(m_threadObjectMutex);
		m_spThread = rhs.m_spThread;
	}

	void Wait()
	{
		boost::unique_lock<boost::mutex> lock(m_mutex);
		while (!m_notified) {
			m_condition.wait(lock);
		}
		m_notified = false;
	}

	bool Wait(double maxNumberOfSecondsToWait)
	{
		numcfc::TimeElapsed te;
		boost::unique_lock<boost::mutex> lock(m_mutex);
		while (!m_notified && te.GetElapsedSeconds() < maxNumberOfSecondsToWait) {
			double maxWaitTime = maxNumberOfSecondsToWait - te.GetElapsedSeconds();
			if (maxWaitTime < 0) {
				maxWaitTime = 0;
			}
			boost::posix_time::time_duration td = boost::posix_time::microseconds(static_cast<boost::int64_t>(maxWaitTime * 1e6));
			m_condition.timed_wait(lock, td);
		}
		bool retVal = m_notified;
		m_notified = false;
		return retVal;
	}

	void Notify()
	{
		{
			boost::lock_guard<boost::mutex> lock(m_mutex);
			m_notified = true;
		}
		m_condition.notify_one();	
	}

	bool Join()
	{
		boost::shared_ptr<boost::thread> spThread;
		{
			boost::mutex::scoped_lock lock(m_threadObjectMutex);
			spThread = m_spThread;
		}

		if (spThread.get() != NULL) {
			spThread->join();
			//assert(m_spThread.get() == NULL); // doesn't necessarily hold when Ctrl-C'd
			return true;
		}
		else {
			return false;
		}
	}

	mutable boost::mutex m_threadObjectMutex;
	boost::shared_ptr<boost::thread> m_spThread;
	bool m_supposedToStop;
	
private:
	boost::mutex m_mutex;
	boost::condition_variable m_condition;

	bool m_notified;
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

bool ThreadRunner::StartThread()
{
	boost::mutex::scoped_lock lock(pimpl_->m_threadObjectMutex);

	if (pimpl_->m_spThread.get() == NULL) {
		pimpl_->m_supposedToStop = false;
		boost::shared_ptr<boost::thread> spThread(new boost::thread(RunThread, boost::ref(*this)));
		pimpl_->m_spThread = spThread;
		return true;
	}
	else {
		return false;
	}
}

void ThreadRunner::RunThread(ThreadRunner& threadRunner)
{
	boost::posix_time::ptime ptThreadStarted = boost::posix_time::microsec_clock::universal_time();
	boost::shared_ptr<boost::thread> spThread;
		
	{
		boost::mutex::scoped_lock lock(threadRunner.pimpl_->m_threadObjectMutex);
		spThread = threadRunner.pimpl_->m_spThread; // keep a reference...
	}

	try {
		threadRunner.operator ()();
	}
	catch (...) {
		;
	}

	{
		boost::mutex::scoped_lock lock(threadRunner.pimpl_->m_threadObjectMutex);
		threadRunner.pimpl_->m_spThread.reset();
	}
}

bool ThreadRunner::AskThreadToStop()
{
	boost::mutex::scoped_lock lock(pimpl_->m_threadObjectMutex);
	if (pimpl_->m_spThread.get() != NULL) {
		pimpl_->m_supposedToStop = true;
		pimpl_->Notify();
		return true;
	}
	else {
		return false;
	}
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

bool ThreadRunner::JoinThread()
{
	return pimpl_->Join();
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
		boost::shared_ptr<boost::thread> spThread(new boost::thread(RunThread, boost::ref(*this), threadTask));

		boost::mutex::scoped_lock lock(entry.m_threadObjectMutex);
		entry.m_spThread = spThread;
		return entry.m_spThread.get() != NULL;
	}
	else {
		return false;
	}
}

void MultiThreadRunner::RunThread(MultiThreadRunner& threadRunner, const std::string& threadTask)
{
	boost::posix_time::ptime ptThreadStarted = boost::posix_time::microsec_clock::universal_time();
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
		return entry.Join();
	}
	else {
		return false;
	}
}

}
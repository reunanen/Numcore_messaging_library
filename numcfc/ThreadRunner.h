
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_THREAD_RUNNER_H
#define NUMCFC_THREAD_RUNNER_H

#include <string>

namespace numcfc
{

class ThreadRunner {
public:
	ThreadRunner();
	virtual ~ThreadRunner();

	virtual void operator()() = 0;

	bool StartThread();
	bool AskThreadToStop();

	bool IsSupposedToStop() const;

	bool JoinThread();

	void Wait();
	bool Wait(double maxNumberOfSecondsToWait); // returns true if notified, false if max time reached
	void Notify();

private:
	static void RunThread(ThreadRunner& threadRunner);

	friend class MultiThreadRunner;
	class Impl;
	Impl* pimpl_;
};

class MultiThreadRunner {
public:
	MultiThreadRunner();
	virtual ~MultiThreadRunner();

	virtual void operator()(const std::string& threadTask) = 0;

	bool StartThread(const std::string& threadTask);
	bool AskThreadToStop(const std::string& threadTask);
	void AskAllThreadsToStop();
	bool IsSupposedToStop(const std::string& threadTask) const;
	bool JoinThread(const std::string& threadTask);

	void Wait(const std::string& threadTask);
	void Notify(const std::string& threadTask);

private:
	static void RunThread(MultiThreadRunner& threadRunner, const std::string& threadTask);

	class Impl;
	Impl* pimpl_;
};

}

#endif // __NUMCFC_THREAD_RUNNER_H__

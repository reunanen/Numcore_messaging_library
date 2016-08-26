
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CLAIM_THROUGHPUT_STATISTICS_H
#define CLAIM_THROUGHPUT_STATISTICS_H

#include <numcfc/Time.h>
#include <mutex>
#include <deque>

namespace claim {

//! This class is not to intended to be used by client code (exception: the MessageRecorder application)
class ThroughputStatistics {
public:
	ThroughputStatistics() {
		m_windowLength = 5.0;
	}

	void AddThroughput(size_t bytes) {
		std::unique_lock<std::mutex> lock(m_mutex);
		Maintain();
		numcfc::TimeElapsed te;
		te.ResetToCurrent();
		m_throughputStatistics.push_back(std::make_pair(te, bytes));
	}
	std::pair<double, double> GetThroughputPerSec() {
		std::unique_lock<std::mutex> lock(m_mutex);
		Maintain();
		std::pair<double, double> p;
		p.first = m_throughputStatistics.size() / m_windowLength;
		p.second = 0;
		std::deque< std::pair<numcfc::TimeElapsed, size_t> >::iterator iter = m_throughputStatistics.begin(), iterEnd = m_throughputStatistics.end();
		for (; iter != iterEnd; iter++) {
			p.second += iter->second / m_windowLength;
		}
		return p;
	}

private:
	void Maintain() {
		while (!m_throughputStatistics.empty()) {
			if (m_throughputStatistics.front().first.GetElapsedSeconds() >= m_windowLength) {
				m_throughputStatistics.pop_front();
			}
			else {
				break;
			}
		}
	}
	std::mutex m_mutex;
	std::deque< std::pair<numcfc::TimeElapsed, size_t> > m_throughputStatistics; // first: item count, second: total bytes
	double m_windowLength;
};

}

#endif // CLAIM_THROUGHPUT_STATISTICS_H
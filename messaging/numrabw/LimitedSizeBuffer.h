//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//                     2016,2018 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <numcfc/Time.h>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <assert.h>

template <typename T>
class LimitedSizeBuffer {
public:
    LimitedSizeBuffer() : m_maxItemCount(1024), m_maxByteCount(1024 * 1024), m_currentByteCount(0) {}

    void SetMaxItemCount(size_t maxItemCount) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_maxItemCount = maxItemCount;
    }

    void SetMaxByteCount(size_t maxByteCount) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_maxByteCount = maxByteCount;
    }

    bool push_back(const T& item) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_items.size() >= m_maxItemCount) {
            return false;
        }
        else if (m_currentByteCount + item.GetSize() >= m_maxByteCount && m_items.size() > 0) { // exception: allow large messages if the buffer is otherwise empty
            return false;
        }
        m_items.push_back(item);
        m_currentByteCount += item.GetSize();

        { // signaling
            {
                std::lock_guard<std::mutex> lock(m_mutexSignaling);
                m_notified = true;
            }
            m_condSignaling.notify_one();
        }

        return true;
    }
    bool pop_front(T& item, double maxSecondsToWait = 0) {
        if (m_items.empty()) {
            if (maxSecondsToWait <= 0) {
                return false;
            }
            { // signaling
                numcfc::TimeElapsed te;
                te.ResetToCurrent();
                std::unique_lock<std::mutex> lock(m_mutexSignaling);
                double secondsLeft = maxSecondsToWait;
                while (!m_notified && secondsLeft > 0) {
                    m_condSignaling.wait_for(lock, std::chrono::milliseconds(static_cast<int>(secondsLeft * 1000)));
                    if (!m_notified) {
                        secondsLeft = maxSecondsToWait - te.GetElapsedSeconds();
                        if (secondsLeft > 0.0 && secondsLeft <= 1.0) {
                            numcfc::SleepMinimal(); // this is to prevent another loop in the normal case of returning false
                            secondsLeft = maxSecondsToWait - te.GetElapsedSeconds();
                        }
                    }
                }
                if (m_notified) {
                    m_notified = false;
                }
                else {
                    return false;
                }
            }
        }

        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_items.empty()) {
            return false; // somebody else got it
        }
        item = m_items.front();
        size_t newByteCount = m_currentByteCount - item.GetSize();
        assert(newByteCount <= m_currentByteCount);
        m_currentByteCount = newByteCount;
        m_items.pop_front();
        assert((m_currentByteCount == 0) == m_items.empty());
        return true;
    }

    std::pair<size_t, size_t> GetItemAndByteCount() const {
        std::unique_lock<std::mutex> lock(m_mutex);
        std::pair<size_t, size_t> p(std::make_pair(m_items.size(), m_currentByteCount));
        return p;
    }

private:
    mutable std::mutex m_mutex;

    // for signaling
    mutable bool m_notified;
    mutable std::mutex m_mutexSignaling;
    mutable std::condition_variable m_condSignaling;

    std::deque<T> m_items;
    size_t m_maxItemCount;
    size_t m_maxByteCount;
    size_t m_currentByteCount;
};

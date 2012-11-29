
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_TIME_H
#define NUMCFC_TIME_H

#include <string>

namespace numcfc
{

class Time {
public:
	static unsigned int GetExtendedISOLength();
        enum TimeType {
		Universal, Local, Unknown
	};

	Time(); // inits to current universal time
	explicit Time(TimeType type);

	// TODO: these shall be made explicit with no default value for the type argument
	/*explicit*/ Time(const char* szExtendedISO, TimeType type = Unknown);
	/*explicit*/ Time(double fSecondsSince1970, TimeType type = Unknown);

	const Time& InitCurrentUniversal();
	const Time& InitCurrentLocal();

	std::string ToExtendedISO() const;
	bool FromExtendedISO(const char* szTime);

	double GetTime() const;

	Time operator+ (double seconds) const;
	double operator- (const Time& t) const; // returns the difference in seconds

	bool operator< (const Time& that) const;
	bool operator<= (const Time& that) const;
	bool operator> (const Time& that) const;
	bool operator>= (const Time& that) const;
	bool operator== (const Time& that) const;
	bool operator!= (const Time& that) const;

	Time ConvertUniversalToLocal() const;
	static Time ConvertUniversalToLocal(const Time& tUniversal);

	bool IsUniversalTime() const;
	bool IsLocalTime() const;

	Time AddSecs(double seconds) const;
	double SecsTo(const Time& t) const;

private:
	friend class TimeImpl;
	//TimeImpl* m_pImpl;

	int m_year;
	int m_month;
	int m_day;
	int m_hour;
	int m_minute;
	int m_second;
	double m_fraction_of_second;

	TimeType m_type;
};

class TimeElapsed {
public:
	TimeElapsed();
	TimeElapsed(const TimeElapsed& src);
	~TimeElapsed();

	TimeElapsed& operator= (const TimeElapsed& src);

	void ResetToCurrent();
	void ResetToPast();

	double GetElapsedSeconds() const;

	// let's use the named constructor idiom, because
	// usually we really want to create either:
	static TimeElapsed NoTimeElapsed();  // or:
	static TimeElapsed LongTimeElapsed();

private:
	class Impl;
	Impl* pimpl_;
};

void Sleep(double seconds);

//! Sleep as little as possible, such that the program anyway does not take 100% of CPU.
void SleepMinimal();

}

#endif // __NUMCFC_TIME_H__

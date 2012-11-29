
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2012 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Time.h"
#include <stdio.h> // for sprintf
#include <assert.h>
#include <time.h>
#include <cstring>
#include <limits>

#ifdef WIN32
#include <windows.h>
#include <winbase.h>
#pragma warning (disable: 4996) // warning C4996: 'strcpy': This function or variable may be unsafe. Consider using strcpy_s instead.
#else
#include <boost/date_time/posix_time/posix_time_types.hpp>
//#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/local_time_adjustor.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#endif // WIN32

#ifdef max
#undef max
#endif

namespace numcfc {

class TimeImpl {
public:
#ifdef WIN32
	static void FromWin32SystemTime(const struct _SYSTEMTIME& sysTime, Time& t)
	{
		t.m_year = sysTime.wYear;
		t.m_month = sysTime.wMonth;
		t.m_day = sysTime.wDay;
		t.m_hour = sysTime.wHour;
		t.m_minute = sysTime.wMinute;
		t.m_second = sysTime.wSecond;
		t.m_fraction_of_second = sysTime.wMilliseconds / 1000.f;
		assert(t.m_fraction_of_second < 1);
	}
	static void ToWin32SystemTime(const Time& t, struct _SYSTEMTIME& sysTime)
	{
		sysTime.wYear = t.m_year;
		sysTime.wMonth = t.m_month;
		sysTime.wDay = t.m_day;
		sysTime.wHour = t.m_hour;
		sysTime.wMinute = t.m_minute;
		sysTime.wSecond = t.m_second;
		sysTime.wMilliseconds = (int) (t.m_fraction_of_second * 1000);
	}
#else // WIN32
	static void FromBoostPtime(const boost::posix_time::ptime& pt, Time& t)
	{
		boost::gregorian::date d = pt.date();
		t.m_year = d.year();
		t.m_month = d.month();
		t.m_day = d.day();
		boost::posix_time::time_duration tod = pt.time_of_day();
		t.m_hour = tod.hours();
		t.m_minute = tod.minutes();
		t.m_second = tod.seconds();
		t.m_fraction_of_second = tod.fractional_seconds() / pow(10.0, tod.num_fractional_digits());
	}
	static boost::posix_time::ptime ToBoostPtime(const Time &t)
	{
	  boost::gregorian::date d(t.m_year, t.m_month, t.m_day);
	  boost::posix_time::time_duration tod(t.m_hour, t.m_minute, t.m_second, static_cast<boost::posix_time::time_duration::fractional_seconds_type>(t.m_fraction_of_second * pow(10.0, boost::posix_time::time_duration::num_fractional_digits())));
	  return boost::posix_time::ptime(d, tod);
	}
#endif // WIN32
};


Time::Time()
{
	InitCurrentUniversal();
}

Time::Time(TimeType type)
{
	switch(type) {
	case Universal:
		InitCurrentUniversal();
		break;
	case Local:
		InitCurrentLocal();
		break;
	default:
		InitCurrentUniversal();
		m_type = Unknown;
		break;
	}
}

Time::Time(const char* szExtendedISO, TimeType type)
{
	FromExtendedISO(szExtendedISO);

	switch(type) {
	case Universal:
	case Local:
		m_type = type;
		break;
	default:
		m_type = Unknown;
		break;
	}
}

Time::Time(double fSecsSince1970, TimeType type)
{
	double f = fSecsSince1970 + 0.0005; // see bug #574
	if (f < 2.0) {
		throw std::runtime_error("Dates before year 1970 are not supported!");
	}
	char buf[100];
	time_t t = (time_t) f;
	if (type == Universal) {
		strftime(buf, 90, "%Y-%m-%dT%H:%M:%S", gmtime(&t));
		m_type = Universal;
	}
	else if (type == Local) {
		strftime(buf, 90, "%Y-%m-%dT%H:%M:%S", localtime(&t));
		m_type = Local;
	}
	else {
#ifdef WIN32
#ifdef _DEBUG
		OutputDebugStringA("\n--> Time constructor: Time type is unknown - assuming universal time.");
#endif // _DEBUG
#endif // WIN32
		strftime(buf, 90, "%Y-%m-%dT%H:%M:%S", gmtime(&t));
		m_type = Unknown;
	}
	double frac = 1000 * f - 1000 * floor(f);
	assert(frac < 1000);
	sprintf(buf + strlen(buf), ".%03d", static_cast<int>(frac));
	FromExtendedISO(buf);
}

const Time& Time::InitCurrentUniversal()
{
#ifdef WIN32
	struct _SYSTEMTIME sysTime;
	GetSystemTime(&sysTime);
	TimeImpl::FromWin32SystemTime(sysTime, *this);
#else
	boost::posix_time::ptime pt = boost::posix_time::microsec_clock::universal_time();
	TimeImpl::FromBoostPtime(pt, *this);
#endif // WIN32
	m_type = Universal;
	return *this;
}

const Time& Time::InitCurrentLocal()
{
#ifdef WIN32
	struct _SYSTEMTIME sysTime;
	GetLocalTime(&sysTime);
	TimeImpl::FromWin32SystemTime(sysTime, *this);
#else
	boost::posix_time::ptime pt = boost::posix_time::microsec_clock::local_time();
	TimeImpl::FromBoostPtime(pt, *this);
#endif // WIN32
	m_type = Local;	
	return *this;
}

std::string Time::ToExtendedISO() const
{
	assert(m_fraction_of_second < 1);

	char buf[4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 3 + 1];
	sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03d", m_year, m_month, m_day, m_hour, m_minute, m_second, (int) (m_fraction_of_second * 1000));

#ifdef _DEBUG
	Time t(buf, m_type);
	assert(m_year == t.m_year);
	assert(m_month == t.m_month);
	assert(m_day == t.m_day);
	assert(m_hour == t.m_hour);
	assert(m_minute == t.m_minute);
	assert(m_second == t.m_second);
#endif // _DEBUG

	return buf;
}

bool Time::FromExtendedISO(const char* szTime)
{
	size_t len = strlen(szTime);
	if (len < 10) {
		return false;
	}
	char temp[5];
	strncpy(temp, szTime, 4);
	temp[4] = '\0';
	m_year = atoi(temp);
	szTime += 5;
	strncpy(temp, szTime, 2);
	temp[2] = '\0';
	m_month = atoi(temp);
	szTime += 3;
	strncpy(temp, szTime, 2);
	//temp[2] = '\0';
	m_day = atoi(temp);
	szTime += 3;

	if (len >= 13) {
		strncpy(temp, szTime, 2);
		//temp[2] = '\0';
		m_hour = atoi(temp);
		szTime += 3;
	}
	else {
		m_hour = 0;
	}

	if (len >= 16) {
		strncpy(temp, szTime, 2);
		//temp[2] = '\0';
		m_minute = atoi(temp);
		szTime += 3;
	}
	else {
		m_minute = 0;
	}

	if (len >= 19) {
		strncpy(temp, szTime, 2);
		//temp[2] = '\0';
		m_second = atoi(temp);
		szTime += 3;
	}
	else {
		m_second = 0;
	}

	if (len < GetExtendedISOLength()) {
		m_fraction_of_second = 0;
	}
	else {
		m_fraction_of_second = atoi(szTime) / 1000.f;
		assert(m_fraction_of_second < 1);
	}

	return true;
}

#ifndef WIN32
#include "mktime.c" // we really need mkgmtime
#endif // WIN32

double Time::GetTime() const
{
	struct tm t;

	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_year = m_year - 1900;
	t.tm_mon = m_month - 1;
	t.tm_mday = m_day;
	t.tm_hour = m_hour;
	t.tm_min = m_minute;
	t.tm_sec = m_second;

	time_t tt = 0;

	if (m_type == Universal) {
		t.tm_isdst = 0;
		tt = _mkgmtime(&t);
		if (tt <= 1) {
			throw std::runtime_error("Invalid UTC date/time: " + ToExtendedISO());
		}
	}
	else if (m_type == Local) {
		// MSDN: "A value less than zero to have the C run-time library code compute whether standard time or daylight savings time is in effect."
		t.tm_isdst = -1; 
		tt = mktime(&t);
		if (tt < 0) {
			throw std::runtime_error("Invalid local date/time: " + ToExtendedISO());
		}
	}
	else {
#ifdef WIN32
#ifdef _DEBUG
		OutputDebugStringA("\n--> GetTime(): Time type is unknown - assuming universal time.\n\n");
#endif // _DEBUG
#endif // WIN32
		t.tm_isdst = 0;
		tt = _mkgmtime(&t);
		if (tt <= 1) {
			throw std::runtime_error("Invalid date/time: " + ToExtendedISO());
		}
	}

	double d = static_cast<double>(tt);
	d += m_fraction_of_second;

#ifdef _DEBUG
	Time tim(d, m_type);
	assert(*this == tim);
#endif // _DEBUG
	return d;
}

Time Time::operator+ (double seconds) const
{
	Time t = *this;
	if (t.m_fraction_of_second + seconds >= 0 && t.m_fraction_of_second + seconds < 1) {
		t.m_fraction_of_second += seconds;
		assert(t.m_fraction_of_second < 1);
	}
	else {
		double t_ = GetTime();
		t_ += seconds;
		Time temp(t_, m_type);
		t = temp;
		assert(t.m_fraction_of_second < 1);
	}
#ifdef _DEBUG
	double diff = t.GetTime() - GetTime();
	assert(diff > seconds - 0.001 && diff < seconds + 0.001);
#endif // _DEBUG
	return t;
}

double Time::operator- (const Time& t) const
{
	assert(this->m_type != Unknown);
	assert(t.m_type != Unknown);
	assert(this->m_type == t.m_type); // TODO: convert on the fly		

	double d1 = this->GetTime();
	double d2 = t.GetTime();

	return d1 - d2;
}

bool Time::operator< (const Time& that) const
{
	assert(this->m_type == Unknown || that.m_type == Unknown || this->m_type == that.m_type); // TODO: convert on the fly		

	if (this->m_year < that.m_year)				{ return true;  }
	else if (this->m_year > that.m_year)		{ return false;	}
	if (this->m_month < that.m_month)			{ return true;	}
	else if (this->m_month > that.m_month)		{ return false;	}
	if (this->m_day < that.m_day)				{ return true;	}
	else if (this->m_day > that.m_day)			{ return false;	}
	if (this->m_hour < that.m_hour)				{ return true;	}
	else if (this->m_hour > that.m_hour)		{ return false;	}
	if (this->m_minute < that.m_minute)			{ return true;	}
	else if (this->m_minute > that.m_minute)	{ return false; }
	if (this->m_second < that.m_second)			{ return true;	}
	else if (this->m_second > that.m_second)	{ return false;	}

	return this->m_fraction_of_second < that.m_fraction_of_second;
}

bool Time::operator<= (const Time& that) const
{
	assert(this->m_type == Unknown || that.m_type == Unknown || this->m_type == that.m_type); // TODO: convert on the fly

	if (this->m_year < that.m_year)				{ return true;  }
	else if (this->m_year > that.m_year)		{ return false;	}
	if (this->m_month < that.m_month)			{ return true;	}
	else if (this->m_month > that.m_month)		{ return false;	}
	if (this->m_day < that.m_day)				{ return true;	}
	else if (this->m_day > that.m_day)			{ return false;	}
	if (this->m_hour < that.m_hour)				{ return true;	}
	else if (this->m_hour > that.m_hour)		{ return false;	}
	if (this->m_minute < that.m_minute)			{ return true;	}
	else if (this->m_minute > that.m_minute)	{ return false; }
	if (this->m_second < that.m_second)			{ return true;	}
	else if (this->m_second > that.m_second)	{ return false;	}

	return this->m_fraction_of_second <= that.m_fraction_of_second;
}

bool Time::operator> (const Time& that) const
{
	// this > that ?
	return !(*this <= that);
}

bool Time::operator>= (const Time& that) const
{
	// this >= that ?
	return !(*this < that);
}

bool Time::operator== (const Time& that) const
{
	// todo: optimize this
	return (*this <= that && that <= *this);
}

bool Time::operator!= (const Time& that) const
{
	// todo: optimize this
	return (*this < that || that < *this);
}


unsigned int Time::GetExtendedISOLength()
{
	//     YYYY-   MM  -   DD  T   hh  :   mm  :   ss  .   fff
	return 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 3;
}

Time Time::ConvertUniversalToLocal() const
{
	Time tLocal = ConvertUniversalToLocal(*this);
	return tLocal;
}

Time Time::ConvertUniversalToLocal(const Time& tUniversal)
{
	assert(!tUniversal.IsLocalTime());
	Time tLocal;
#ifdef WIN32
	struct _SYSTEMTIME sysTimeUniversal, sysTimeLocal;
	TimeImpl::ToWin32SystemTime(tUniversal, sysTimeUniversal);
	SystemTimeToTzSpecificLocalTime(NULL, &sysTimeUniversal, &sysTimeLocal);
	TimeImpl::FromWin32SystemTime(sysTimeLocal, tLocal);
#else // WIN32
	boost::posix_time::ptime ptUniversal = TimeImpl::ToBoostPtime(tUniversal);
   	typedef boost::date_time::c_local_adjustor<boost::posix_time::ptime> local_adj;
	boost::posix_time::ptime ptLocal = local_adj::utc_to_local(ptUniversal);
	TimeImpl::FromBoostPtime(ptLocal, tLocal);
#endif // WIN32	
	tLocal.m_type = Local;
	return tLocal;
}

bool Time::IsUniversalTime() const
{
	return m_type == Universal;
}

bool Time::IsLocalTime() const
{
	return m_type == Local;
}

Time Time::AddSecs(double seconds) const
{
	return operator+(seconds);
}

double Time::SecsTo(const Time& t) const
{
	return t.operator-(*this);
}

class TimeElapsed::Impl {
public:
	Time t0;
};

TimeElapsed::TimeElapsed()
{
	pimpl_ = new Impl;
	ResetToCurrent();
}

TimeElapsed::TimeElapsed(const TimeElapsed& src)
{
	pimpl_ = new Impl;
	*pimpl_ = *src.pimpl_;
}

TimeElapsed::~TimeElapsed()
{
	delete pimpl_;
}

TimeElapsed& TimeElapsed::operator= (const TimeElapsed& src)
{
	*pimpl_ = *src.pimpl_;
	return *this;
}

void TimeElapsed::ResetToCurrent()
{
	Time tNow(Time::Universal);
	pimpl_->t0.InitCurrentUniversal();
}

void TimeElapsed::ResetToPast()
{
	double d = 2.0; // 1 = error (see: http://msdn.microsoft.com/en-us/library/2093ets1(v=vs.80).aspx
	Time t(d, numcfc::Time::Universal);
	pimpl_->t0 = t;
}

double TimeElapsed::GetElapsedSeconds() const
{
	Time tNow(Time::Universal);
	double t0 = pimpl_->t0.GetTime();
	double t1 = tNow.GetTime();
	return t1 - t0;
}

TimeElapsed TimeElapsed::NoTimeElapsed()
{
	TimeElapsed te;
	te.ResetToCurrent();
	return te;
}

TimeElapsed TimeElapsed::LongTimeElapsed()
{
	TimeElapsed te;
	te.ResetToPast();
	return te;
}

void Sleep(double seconds)
{
	if (seconds < 0) {
		seconds = 0.0;
	}
#ifdef _WIN32
	if (seconds > 0 && seconds < 0.001) {
		seconds = 0.001;
	}
	::Sleep((DWORD) (seconds * 1000));	
#else // _WIN32
	if (seconds > 0 && seconds < 1e-6) {
		seconds = 1e-6;
	}
	usleep(seconds * 1000000);
#endif // _WIN32
}

void SleepMinimal()
{
#ifdef _WIN32
	::Sleep(1);	
#else // _WIN32
	// usleep(1) makes at least Juha's good old IBM Thinkpad T22 running Xubuntu Linux 8.04 consume 100% of CPU
	usleep(2);
#endif // _WIN32
}

}


//               Copyright 2018 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <messaging/claim/PostOffice.h>
#include <messaging/claim/AttributeMessage.h>
#include <numcfc/Logger.h>
#include <numcfc/Time.h>

#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

int main(int argc, char* argv[])
{
	numcfc::Logger::LogAndEcho("disk-space-logger starting - initializing...");

	numcfc::IniFile iniFile("disk-space-logger.ini");

    claim::PostOffice postOffice;
    postOffice.Initialize(iniFile, "dsl");

    if (iniFile.IsDirty()) {
        numcfc::Logger::LogAndEcho("Saving the ini file...");
        iniFile.Save();
    }

    auto next = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_until(next);
        next += std::chrono::milliseconds(1000);

        claim::AttributeMessage amsg;
        amsg.m_type = "influx-output";

#if _WIN32
        const DWORD logicalDrives = GetLogicalDrives();       

        for (int drive = 0; drive < 32; ++drive) {
            const auto driveExists = (logicalDrives >> drive) & 1;
            if (driveExists) {
                const char driveLetter = 'A' + drive;
                const std::string driveRoot = driveLetter + std::string(":\\");
                ULARGE_INTEGER freeBytesAvailableToCaller = { 0 }, totalNumberOfBytes = { 0 }, totalNumberOfFreeBytes = { 0 };
                if (GetDiskFreeSpaceExA(driveRoot.c_str(), &freeBytesAvailableToCaller, &totalNumberOfBytes, &totalNumberOfFreeBytes)) {
                    std::ostringstream oss;
                    oss << std::setprecision(12) << totalNumberOfFreeBytes.QuadPart * 1e-9;
                    amsg.m_attributes[std::string("freeBytes_GB_") + driveLetter] = oss.str();
                    numcfc::Logger::LogAndEcho(driveLetter + std::string(": free space = ") + oss.str() + " GB");
                }
            }
        }
#endif // _WIN32

        postOffice.Send(amsg);
    }
}
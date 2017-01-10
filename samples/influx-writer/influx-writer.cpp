//               Copyright 2017 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <messaging/claim/PostOffice.h>
#include <messaging/claim/AttributeMessage.h>
#include <numcfc/Logger.h>

#include <curl/curl.h>

int main(int argc, char* argv[])
{
	numcfc::Logger::LogAndEcho("influx-writer starting - initializing...");

	numcfc::IniFile iniFile("influx-writer.ini");

    claim::PostOffice postOffice;
    postOffice.Initialize(iniFile);

    postOffice.Subscribe("influx-output");

    const std::string influxURL = iniFile.GetSetValue("InfluxDB", "URL", "http://localhost:8086");
    const std::string url = (!influxURL.empty() && influxURL.back() != '/') ? influxURL + "/" : influxURL;

    const std::string db = iniFile.GetSetValue("InfluxDB", "Database", "db");

    const std::string retention = iniFile.GetSetValue("InfluxDB", "Retention", "4w", "See https://docs.influxdata.com/influxdb/v0.9/query_language/database_management/#retention-policy-management");

    numcfc::Logger::LogAndEcho("Writing to: " + url + " : " + db);

    if (iniFile.IsDirty()) {
        numcfc::Logger::LogAndEcho("Saving the ini file...");
        iniFile.Save();
    }

    auto curl = curl_easy_init();

    if (!curl) {
        numcfc::Logger::LogAndEcho("curl_easy_init() failed", "log_error");
        exit(1);
    }

    // create database
    curl_easy_setopt(curl, CURLOPT_URL, (url + "query").c_str());
    
    const std::string createDatabase = "q=CREATE DATABASE " + db;

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, createDatabase.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, createDatabase.length());

    auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        numcfc::Logger::LogAndEcho("Creating a database failed: " + std::string(curl_easy_strerror(result)), "log_error");
    }

    // create retention policy
    const std::string createRetentionPolicy = "q=CREATE RETENTION POLICY default_retention_policy ON " + db + " DURATION " + retention + " REPLICATION 1 DEFAULT";

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, createRetentionPolicy.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, createRetentionPolicy.length());

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        numcfc::Logger::LogAndEcho("Creating a retention policy failed: " + std::string(curl_easy_strerror(result)), "log_error");
    }

    // write data
    curl_easy_setopt(curl, CURLOPT_URL, (url + "write?db=" + db).c_str());

    while (true) {
        slaim::Message msg;
        while (postOffice.Receive(msg, 0.0)) {
            if (msg.GetType() == "influx-output") {
                claim::AttributeMessage amsg(msg);
                for (const auto& attribute : amsg.m_attributes) {
                    const std::string write = attribute.first + " value=" + attribute.second;

                    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, write.c_str());
                    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, write.length());

                    auto result = curl_easy_perform(curl);
                    if (result != CURLE_OK) {
                        numcfc::Logger::LogAndEcho("Writing data failed: " + std::string(curl_easy_strerror(result)), "log_error");
                    }
                }
            }
        }
    }
}
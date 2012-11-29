
//               Copyright 2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <messaging/claim/PostOffice.h>
#include <messaging/claim/AttributeMessage.h>
#include <numcfc/ThreadRunner.h>
#include <numcfc/Logger.h>
#include <numcfc/IdGenerator.h>
#include <numcfc/Time.h>

#include <sstream>
#include <assert.h>

class Producer : public numcfc::ThreadRunner
{
public:
	Producer(const std::string& id, numcfc::IniFile& iniFile)
		: id(id), numberOfMessagesSent(0)
	{
		postOffice.Initialize(iniFile, "producer");
	}

	virtual void operator()() {
		// produce and send numbers
		int counter = 1;
		while (!IsSupposedToStop()) {
			claim::AttributeMessage amsg;
			amsg.m_type = "Number";
			amsg.m_attributes["id"] = id;

			std::ostringstream oss;
			oss << counter++;
			amsg.m_body = oss.str();

			postOffice.Send(amsg.GetRawMessage());
			++numberOfMessagesSent;

			numcfc::SleepMinimal();

			std::string error = postOffice.GetError();
			if (!error.empty()) {
				numcfc::Logger::LogAndEcho(error, "error");
			}
		}
	}

	int GetNumberOfMessagesSent() const {
		return numberOfMessagesSent;
	}

private:
	std::string id;
	claim::PostOffice postOffice;
	int numberOfMessagesSent;
};

class Consumer : public numcfc::ThreadRunner
{
public:
	Consumer(const std::string& id, numcfc::IniFile& iniFile)
		: id(id), expectedNumber(1), successCounter(0)
		, errorCounter(0), someoneElseCounter(0)
	{
		postOffice.Initialize(iniFile, "consumer");
		postOffice.Subscribe("Number");
	}

	virtual void operator()() {
		// receive messages and consume them
		while (!IsSupposedToStop()) {
			slaim::Message msg;
			double maxSecondsToWait = 1.0;
			if (postOffice.Receive(msg, maxSecondsToWait)) {
				if (msg.GetType() == "Number") {
					ProcessNumberMessage(msg);
				}
				else {
					assert(false); // should not happen
				}
			}

			std::string error = postOffice.GetError();
			if (!error.empty()) {
				numcfc::Logger::LogAndEcho(error, "error");
			}
		}
	}

	int GetSuccessCounter() const {
		return successCounter;
	}

	int GetErrorCounter() const {
		return errorCounter;
	}

	int GetNumberOfMessagesFromSomeoneElse() const {
		return someoneElseCounter;
	}

private:
	std::string id;
	claim::PostOffice postOffice;
	int expectedNumber;
	int successCounter;
	int errorCounter;
	int someoneElseCounter;

	void ProcessNumberMessage(const slaim::Message& msg) {
		claim::AttributeMessage amsg(msg);
		bool mine = (amsg.m_attributes["id"] == id);
		if (mine) {
			int number = atoi(amsg.m_body.c_str());
			if (number == expectedNumber) {
				++expectedNumber;
				++successCounter;
			}
			else {
				++errorCounter;
			}
		}
		else {
			++someoneElseCounter;
		}
	}
};

int main(int argc, char* argv[])
{
	numcfc::Logger::LogAndEcho("Program starting - initializing...");

	std::cout
		<< "                         This product uses software developed by" << std::endl
		<< "                         Spread Concepts LLC for use in the Spread" << std::endl
		<< "                         toolkit. For more information about" << std::endl
		<< "                         Spread see http://www.spread.org" << std::endl;

	numcfc::IdGenerator idGenerator;
	std::string myId = idGenerator.GenerateId();

	numcfc::IniFile iniFile("producer-consumer.ini");

	Producer producer(myId, iniFile);
	Consumer consumer(myId, iniFile);

	if (iniFile.IsDirty()) {
		numcfc::Logger::LogAndEcho("Saving the ini file...");
		iniFile.Save();
	}

	numcfc::Sleep(1);

	numcfc::Logger::LogAndEcho("Starting the producer...");
	producer.StartThread();
	numcfc::Logger::LogAndEcho("Starting the consumer...");
	consumer.StartThread();

	numcfc::Logger::LogAndEcho("Done! Now waiting for 5 seconds...");
	numcfc::Sleep(5);

	numcfc::Logger::LogAndEcho("Waiting for the producer to stop...");
	producer.AskThreadToStop();
	producer.JoinThread();

	numcfc::Logger::LogAndEcho("Done! Now waiting for 2 more seconds...");
	numcfc::Sleep(2);

	numcfc::Logger::LogAndEcho("Waiting for the consumer to stop...");
	consumer.AskThreadToStop();
	consumer.JoinThread();

	numcfc::Logger::LogAndEcho("Done!");

	{
		std::ostringstream oss;
		oss << "Number of messages sent: " << producer.GetNumberOfMessagesSent();
		numcfc::Logger::LogAndEcho(oss.str());
	}

	{
		std::ostringstream oss;
		oss << "Number of successes:     " << consumer.GetSuccessCounter();
		numcfc::Logger::LogAndEcho(oss.str());
	}

	{
		std::ostringstream oss;
		oss << "Number of errors:        " << consumer.GetErrorCounter();
		numcfc::Logger::LogAndEcho(oss.str());
	}

	int someoneElseCounter = consumer.GetNumberOfMessagesFromSomeoneElse();
	if (someoneElseCounter > 0) {
		std::ostringstream oss;
		oss << "Number of messages from someone else: " << someoneElseCounter;
		numcfc::Logger::LogAndEcho(oss.str());
	}
}
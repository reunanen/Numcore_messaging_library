
//           Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "../cppzmq/zmq.hpp"
#include "../config.h"
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <chrono>

class RegisteredClient
{
public:
    RegisteredClient(const std::vector<char>& zmqId, unsigned int clientNumber)
        : zmqId(zmqId)
        , clientNumber(clientNumber)
    {
        timeHeartbeatLastReceived = std::chrono::high_resolution_clock::now();
    }

    static std::string GetFullClientIdentifier(const std::string& clientIdentifier, unsigned int clientNumber)
    {
        if (clientNumber == 1) {
            return clientIdentifier;
        }
        else {
            std::ostringstream oss;
            oss << clientIdentifier << "#" << clientNumber;
            return oss.str();
        }
    }

    std::vector<char> zmqId;
    unsigned int clientNumber;
    std::chrono::time_point<std::chrono::high_resolution_clock> timeHeartbeatLastReceived;

    std::unordered_set<std::string> subscriptions;
};

class Broker
{
public:
    Broker();
    void Run();

private:
    bool Peek();
    RegisteredClient* FindRegisteredClient(const std::vector<char>& zmqId);
    unsigned int FindSmallestAvailableClientNumber(const std::deque<RegisteredClient>& clientsForId);
    void RemoveInactiveClients();

    zmq::context_t context;
    zmq::socket_t router;

    std::unordered_map<std::string, std::deque<RegisteredClient>> registeredClients;
};

Broker::Broker()
    : router(context, ZMQ_ROUTER)
{
    std::ostringstream address;
    address << "tcp://*:" << num0w::defaultPort;

    std::cout << "Broker initializing @ " << address.str() << "...";

    const int one = 1;
    router.setsockopt(ZMQ_TCP_KEEPALIVE, &one, sizeof one);
    router.setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &one, sizeof one);
    router.setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &one, sizeof one);

    router.bind(address.str());

    std::cout << "Done!" << std::endl;
}

std::string ToString(const zmq::message_t& message)
{
    return std::string(static_cast<const char*>(message.data()), static_cast<const char*>(message.data()) + message.size());
}

std::vector<char> ToVector(const zmq::message_t& message)
{
    return std::vector<char>(static_cast<const char*>(message.data()), static_cast<const char*>(message.data()) + message.size());
}

zmq::message_t ToMessage(const std::string& string)
{
    return zmq::message_t(string.data(), string.size());
}

zmq::message_t CopyMessage(const zmq::message_t& message)
{
    return zmq::message_t(message.data(), message.size());
}

void Broker::Run()
{
	while (true) {
        if (Peek()) {
            zmq::message_t zmqIdMessage;

            if (router.recv(&zmqIdMessage, ZMQ_DONTWAIT)) {
                std::vector<char> zmqId = ToVector(zmqIdMessage);
                zmq::message_t headerMessage;
                if (router.recv(&headerMessage, ZMQ_DONTWAIT)) {
                    std::string header = ToString(headerMessage);

                    RegisteredClient* registeredClient = nullptr;

                    if (header != "Register") {
                        registeredClient = FindRegisteredClient(zmqId);
                        if (registeredClient == nullptr) {
                            // error: non-registered client
                            router.send(CopyMessage(zmqIdMessage), ZMQ_SNDMORE);
                            router.send(ToMessage("UnregisteredError"), 0);
                        }
                    }

                    if (header == "Heartbeat") {
                        if (registeredClient) {
                            const auto now = std::chrono::high_resolution_clock::now();
                            registeredClient->timeHeartbeatLastReceived = now;
                        }
                    }
                    else if (header == "Publish") {
                        // handle later
                    }
                    else { // logging
                        std::cout << "Received message '" << header << "' from {";
                        for (int i = 0, end = zmqId.size(); i < end; ++i) {
                            if (i > 0) {
                                std::cout << ", ";
                            }
                            std::cout << static_cast<int>(static_cast<unsigned char>(zmqId[i]));
                        }
                        std::cout << "}" << std::endl;
                    }

                    if (header == "Heartbeat") {
                        // already handled
                    }
                    else if (header == "Register") {
                        zmq::message_t clientIdentifierMessage;
                        if (router.recv(&clientIdentifierMessage, ZMQ_DONTWAIT)) {
                            std::string clientIdentifier = ToString(clientIdentifierMessage);

                            auto& clientsForId = registeredClients[clientIdentifier];
                            auto i = std::find_if(clientsForId.begin(), clientsForId.end(),
                                [&zmqId](const RegisteredClient& registeredClient) {
                                    return registeredClient.zmqId == zmqId;
                                });

                            if (i == clientsForId.end()) {
                                unsigned int newClientNumber = FindSmallestAvailableClientNumber(clientsForId);
                                clientsForId.push_back(RegisteredClient(zmqId, newClientNumber));
                                clientIdentifier = RegisteredClient::GetFullClientIdentifier(clientIdentifier, newClientNumber);
                                std::cout << "Registered new client " << clientIdentifier << std::endl;
                            }
                            else {
                                clientIdentifier = RegisteredClient::GetFullClientIdentifier(clientIdentifier, i->clientNumber);
                                std::cout << "Re-registered existing client " << clientIdentifier << std::endl;
                            }

                            router.send(CopyMessage(zmqIdMessage), ZMQ_SNDMORE);
                            router.send(headerMessage, ZMQ_SNDMORE);
                            router.send(ToMessage(clientIdentifier), 0);
                        }
                        else {
                            std::cerr << "Sequence error: no client identifier" << std::endl;
                        }
                    }
                    else if (header == "Subscribe" || header == "Unsubscribe") {
                        zmq::message_t messageTypeMessage;
                        if (router.recv(&messageTypeMessage, ZMQ_DONTWAIT)) {
                            std::string messageType = ToString(messageTypeMessage);
                            std::cout << "  Message type: " << messageType << std::endl;

                            if (registeredClient) {
                                if (header == "Subscribe") {
                                    registeredClient->subscriptions.insert(messageType);
                                }
                                else {
                                    assert(header == "Unsubscribe");
                                    registeredClient->subscriptions.erase(messageType);
                                }
                            }
                        }
                        else {
                            std::cerr << "Sequence error: no message type" << std::endl;
                        }
                    }
                    else if (header == "Publish") {
                        zmq::message_t messageTypeMessage;
                        if (router.recv(&messageTypeMessage, ZMQ_DONTWAIT)) {
                            std::string messageType = ToString(messageTypeMessage);
                            zmq::message_t payloadMessage;
                            if (router.recv(&payloadMessage, ZMQ_DONTWAIT)) {
                                int matchedSubscriberCount = 0;
                                for (auto& i : registeredClients) {
                                    for (auto& registeredClient : i.second) {
                                        if (registeredClient.subscriptions.find(messageType) != registeredClient.subscriptions.end()) {
                                            router.send(zmq::message_t(registeredClient.zmqId.begin(), registeredClient.zmqId.end()), ZMQ_SNDMORE);
                                            router.send(ToMessage("Publish"), ZMQ_SNDMORE);
                                            router.send(CopyMessage(messageTypeMessage), ZMQ_SNDMORE);
                                            router.send(CopyMessage(payloadMessage), 0);
                                            ++matchedSubscriberCount;
                                        }
                                    }
                                }
                                if (matchedSubscriberCount == 0) {
                                    //std::cout << "Warning: no subscribers found (message type = " << messageType << ")" << std::endl;
                                }
                            }
                            else {
                                std::cerr << "Sequence error: no payload" << std::endl;
                            }
                        }
                        else {
                            std::cerr << "Sequence error: no message type" << std::endl;
                        }
                    }
                    else {
                        std::cerr << "Sequence error: unexpected header " << header << std::endl;
                    }
                }
            }
            else {
                std::cerr << "No message available, even though Peek() returned true" << std::endl;
            }
        }

        RemoveInactiveClients();
	}
}

bool Broker::Peek()
{
    zmq::pollitem_t pollItem;
    pollItem.socket = router;
    pollItem.events = ZMQ_POLLIN;

    return zmq::poll(&pollItem, 1, 1000L) > 0;
}

RegisteredClient* Broker::FindRegisteredClient(const std::vector<char>& zmqId)
{
    for (auto& i : registeredClients) {
        for (auto& registeredClient : i.second) {
            if (registeredClient.zmqId == zmqId) {
                return &registeredClient;
            }
        }
    }
    return nullptr;
}

unsigned int Broker::FindSmallestAvailableClientNumber(const std::deque<RegisteredClient>& clientsForId)
{
    // TODO: optimize
    unsigned int candidateNumber = 1;
    while (true) {
        const bool found = std::find_if(clientsForId.begin(), clientsForId.end(),
            [candidateNumber](const RegisteredClient& client) {
                return client.clientNumber == candidateNumber;
            }
        ) != clientsForId.end();

        if (!found) {
            return candidateNumber;
        }
        ++candidateNumber;
    };
}

void Broker::RemoveInactiveClients()
{
    const auto now = std::chrono::high_resolution_clock::now();

    for (auto& i : registeredClients) {
        std::deque<RegisteredClient>& clients = i.second;

        const int timeout_ms = 10000;

        clients.erase(
            std::remove_if(clients.begin(), clients.end(),
                [&](const RegisteredClient& client) {
                    const bool remove = std::chrono::duration_cast<std::chrono::milliseconds>(now - client.timeHeartbeatLastReceived).count() > timeout_ms;
                    if (remove) {
                        std::cout << "Removing " << RegisteredClient::GetFullClientIdentifier(i.first, client.clientNumber) << " due to inactivity" << std::endl;
                    }
                    return remove;
                }
            ), clients.end()
        );
    }
}

int main(int argc, char* argv[])
{
    try {
        Broker broker;
        broker.Run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}


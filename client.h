#ifndef CLIENT_H
#define CLIENT_H

#include <fcntl.h>
#include <unistd.h>
#include <vector>
#include <mutex>
#include <iostream>

#include "forward_declarations.h"

#include "threaddata.h"
#include "mqttpacket.h"
#include "exceptions.h"
#include "cirbuf.h"


#define CLIENT_BUFFER_SIZE 1024 // Must be power of 2
#define CLIENT_MAX_BUFFER_SIZE 65536
#define MQTT_HEADER_LENGH 2

class Client
{
    int fd;

    CirBuf readbuf;

    char *writebuf = NULL; // With many clients, it may not be smart to keep a (big) buffer around.
    size_t writeBufsize = CLIENT_BUFFER_SIZE;
    int wwi = 0;
    int wri = 0;

    bool authenticated = false;
    bool connectPacketSeen = false;
    bool readyForWriting = false;
    bool readyForReading = true;
    bool disconnectWhenBytesWritten = false;
    bool disconnecting = false;

    std::string clientid;
    std::string username;
    uint16_t keepalive = 0;

    std::string will_topic;
    std::string will_payload;
    bool will_retain = false;
    char will_qos = 0;

    ThreadData_p threadData;
    std::mutex writeBufMutex;


    size_t getWriteBufMaxWriteSize()
    {
        size_t available = writeBufsize - wwi;
        return available;
    }

    // Note: this is not the inverse of free space, because there can be non-used lead-in in the buffer!
    size_t getWriteBufBytesUsed()
    {
        return wwi - wri;
    };

    void growWriteBuffer(size_t add_size);


    void setReadyForWriting(bool val);
    void setReadyForReading(bool val);

public:
    Client(int fd, ThreadData_p threadData);
    Client(const Client &other) = delete;
    Client(Client &&other) = delete;
    ~Client();

    int getFd() { return fd;}
    void markAsDisconnecting();
    bool readFdIntoBuffer();
    bool bufferToMqttPackets(std::vector<MqttPacket> &packetQueueIn, Client_p &sender);
    void setClientProperties(const std::string &clientId, const std::string username, bool connectPacketSeen, uint16_t keepalive);
    void setWill(const std::string &topic, const std::string &payload, bool retain, char qos);
    void setAuthenticated(bool value) { authenticated = value;}
    bool getAuthenticated() { return authenticated; }
    bool hasConnectPacketSeen() { return connectPacketSeen; }
    ThreadData_p getThreadData() { return threadData; }
    std::string &getClientId() { return this->clientid; }

    void writePingResp();
    void writeMqttPacket(const MqttPacket &packet);
    void writeMqttPacketAndBlameThisClient(const MqttPacket &packet);
    bool writeBufIntoFd();
    bool readyForDisconnecting() const { return disconnectWhenBytesWritten && wwi == wri && wwi == 0; }

    // Do this before calling an action that makes this client ready for writing, so that the EPOLLOUT will handle it.
    void setReadyForDisconnect() { disconnectWhenBytesWritten = true; }

    std::string repr();

};

#endif // CLIENT_H

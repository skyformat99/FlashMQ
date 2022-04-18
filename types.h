/*
This file is part of FlashMQ (https://www.flashmq.org)
Copyright (C) 2021 Wiebe Cazemier

FlashMQ is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, version 3.

FlashMQ is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public
License along with FlashMQ. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef TYPES_H
#define TYPES_H

#include "stdint.h"
#include <list>
#include <string>
#include <memory>
#include <chrono>
#include <vector>

#include "forward_declarations.h"

enum class PacketType
{
    Reserved = 0,
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5,
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14,

    Reserved2 = 15
};

enum class ProtocolVersion
{
    None = 0,
    Mqtt31 = 0x03,
    Mqtt311 = 0x04,
    Mqtt5 = 0x05
};

enum class Mqtt5Properties
{
    None = 0,
    PayloadFormatIndicator = 1,
    MessageExpiryInterval = 2,
    ContentType = 3,
    ResponseTopic = 8,
    CorrelationData = 9,
    SubscriptionIdentifier = 11,
    SessionExpiryInterval = 17,
    AssignedClientIdentifier = 18,
    ServerKeepAlive = 19,
    AuthenticationMethod = 21,
    AuthenticationData = 22,
    RequestProblemInformation = 23,
    WillDelayInterval = 24,
    RequestResponseInformation = 25,
    ResponseInformation = 26,
    ServerReference = 28,
    ReasonString = 31,
    ReceiveMaximum = 33,
    TopicAliasMaximum = 34,
    TopicAlias = 35,
    MaximumQoS = 36,
    RetainAvailable = 37,
    UserProperty = 38,
    MaximumPacketSize = 39,
    WildcardSubscriptionAvailable = 40,
    SubscriptionIdentifierAvailable = 41,
    SharedSubscriptionAvailable = 42
};

/**
 * @brief The ConnAckReturnCodes enum are for MQTT3
 */
enum class ConnAckReturnCodes
{
    Accepted = 0,
    UnacceptableProtocolVersion = 1,
    ClientIdRejected = 2,
    ServerUnavailable = 3,
    MalformedUsernameOrPassword = 4,
    NotAuthorized = 5
};

/**
 * @brief The ReasonCodes enum are for MQTT5.
 */
enum class ReasonCodes
{
    Success = 0,
    GrantedQoS0 = 0,
    GrantedQoS1 = 1,
    GrantedQoS2 = 2,
    DisconnectWithWill = 4,
    NoMatchingSubscribers = 16,
    NoSubscriptionExisted = 17,
    ContinueAuthentication = 24,
    ReAuthenticate = 25,
    UnspecifiedError = 128,
    MalformedPacket = 129,
    ProtocolError = 130,
    ImplementationSpecificError = 131,
    UnsupportedProtocolVersion = 132,
    ClientIdentifierNotValid = 133,
    BadUserNameOrPassword = 134,
    NotAuthorized = 135,
    ServerUnavailable = 136,
    ServerBusy = 137,
    Banned = 138,
    ServerShuttingDown = 139,
    BadAuthenticationMethod = 140,
    KeepAliveTimeout = 141,
    SessionTakenOver = 142,
    TopicFilterInvalid = 143,
    TopicNameInvalid = 144,
    PacketIdentifierInUse = 145,
    ReceiveMaximumExceeded = 147,
    TopicAliasInvalid = 148,
    PacketTooLarge = 149,
    MessageRateTooHigh = 150,
    QuoteExceeded = 151,
    AdministrativeAction = 152,
    PayloadFormatInvalid = 153,
    RetainNotSupported = 154,
    QosNotSupported = 155,
    UseAnotherServer = 156,
    ServerMoved = 157,
    SharedSubscriptionsNotSupported = 158,
    ConnectionRateExceeded = 159,
    MaximumConnectTime = 160,
    SubscriptionIdentifiersNotSupported = 161,
    WildcardSubscriptionsNotSupported = 162
};

class ConnAck
{
public:
    ConnAck(const ProtocolVersion protVersion, ReasonCodes return_code, bool session_present=false);

    const ProtocolVersion protocol_version;
    uint8_t return_code;
    bool session_present = false;
    std::shared_ptr<Mqtt5PropertyBuilder> propertyBuilder;

    size_t getLengthWithoutFixedHeader() const;
};

class SubAck
{
public:
    const ProtocolVersion protocol_version;
    const uint16_t packet_id;
    std::list<ReasonCodes> responses;
    std::shared_ptr<Mqtt5PropertyBuilder> propertyBuilder;

    SubAck(const ProtocolVersion protVersion, uint16_t packet_id, const std::list<ReasonCodes> &subs_qos_reponses);
    size_t getLengthWithoutFixedHeader() const;
};

class UnsubAck
{
public:
    const ProtocolVersion protocol_version;
    const uint16_t packet_id;
    std::shared_ptr<Mqtt5PropertyBuilder> propertyBuilder;
    std::vector<ReasonCodes> reasonCodes;
    UnsubAck(const ProtocolVersion protVersion, uint16_t packet_id, const int unsubCount);
    size_t getLengthWithoutFixedHeader() const;
};

/**
 * @brief The PublishBase class was incepted to have an easy default copy constuctor that doesn't copy all fields.
 */
class PublishBase
{
    friend class SessionsAndSubscriptionsDB;

    bool hasExpireInfo = false;
    std::chrono::time_point<std::chrono::steady_clock> createdAt;
    std::chrono::seconds expiresAfter;

public:
    std::string topic;
    std::string payload;
    char qos = 0;
    bool retain = false; // Note: existing subscribers don't get publishes of retained messages with retain=1. [MQTT-3.3.1-9]
    bool splitTopic = true;
    uint16_t topicAlias = 0;
    bool skipTopic = false;
    std::shared_ptr<Mqtt5PropertyBuilder> propertyBuilder; // Only contains data for sending, not receiving

    PublishBase() = default;
    PublishBase(const std::string &topic, const std::string &payload, char qos);
    size_t getLengthWithoutFixedHeader() const;
    void setClientSpecificProperties();
    void clearClientSpecificProperties();
    void constructPropertyBuilder();
    bool hasUserProperties() const;
    bool hasExpired() const;

    void setExpireAfter(uint32_t s);
    bool getHasExpireInfo() const;
    const std::chrono::time_point<std::chrono::steady_clock> getCreatedAt() const;
};

class Publish : public PublishBase
{
public:
    std::vector<std::string> subtopics;

    Publish() = default;
    Publish(const Publish &other);
    Publish(const std::string &topic, const std::string &payload, char qos);
};

class WillPublish : public Publish
{
    bool isQueued = false;
    std::chrono::time_point<std::chrono::steady_clock> queuedAt;
public:
    uint32_t will_delay = 0;
    WillPublish() = default;
    WillPublish(const Publish &other);
    void setQueuedAt();
    uint32_t getQueuedAtAge() const;
};

class PubResponse
{
public:
    PubResponse(const PubResponse &other) = delete;
    PubResponse(const ProtocolVersion protVersion, const PacketType packet_type, ReasonCodes reason_code, uint16_t packet_id);

    const PacketType packet_type;
    const ProtocolVersion protocol_version;
    const ReasonCodes reason_code;
    uint16_t packet_id;
    uint8_t getLengthIncludingFixedHeader() const;
    uint8_t getRemainingLength() const;
    bool needsReasonCode() const;
};

class Disconnect
{
public:
    ReasonCodes reasonCode;
    std::shared_ptr<Mqtt5PropertyBuilder> propertyBuilder;
    Disconnect(const ProtocolVersion protVersion, ReasonCodes reason_code);
    size_t getLengthWithoutFixedHeader() const;
};

#endif // TYPES_H

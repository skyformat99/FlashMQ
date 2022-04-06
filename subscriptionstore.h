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

#ifndef SUBSCRIPTIONSTORE_H
#define SUBSCRIPTIONSTORE_H

#include <unordered_map>
#include <forward_list>
#include <list>
#include <mutex>
#include <pthread.h>

#include "forward_declarations.h"

#include "client.h"
#include "session.h"
#include "utils.h"
#include "retainedmessage.h"
#include "logger.h"


struct Subscription
{
    std::weak_ptr<Session> session; // Weak pointer expires when session has been cleaned by 'clean session' connect or when it was remove because it expired
    char qos;
    bool operator==(const Subscription &rhs) const;
    void reset();
};

struct ReceivingSubscriber
{
    const std::shared_ptr<Session> session;
    const char qos;

public:
    ReceivingSubscriber(const std::shared_ptr<Session> &ses, char qos);
};

class SubscriptionNode
{
    std::string subtopic;
    std::unordered_map<std::string, Subscription> subscribers;

public:
    SubscriptionNode(const std::string &subtopic);
    SubscriptionNode(const SubscriptionNode &node) = delete;
    SubscriptionNode(SubscriptionNode &&node) = delete;

    std::unordered_map<std::string, Subscription> &getSubscribers();
    const std::string &getSubtopic() const;
    void addSubscriber(const std::shared_ptr<Session> &subscriber, char qos);
    void removeSubscriber(const std::shared_ptr<Session> &subscriber);
    std::unordered_map<std::string, std::unique_ptr<SubscriptionNode>> children;
    std::unique_ptr<SubscriptionNode> childrenPlus;
    std::unique_ptr<SubscriptionNode> childrenPound;

    SubscriptionNode *getChildren(const std::string &subtopic) const;

    int cleanSubscriptions();
};

class RetainedMessageNode
{
    friend class SubscriptionStore;

    std::unordered_map<std::string, std::unique_ptr<RetainedMessageNode>> children;
    std::unordered_set<RetainedMessage> retainedMessages;

    void addPayload(const std::string &topic, const std::string &payload, char qos, int64_t &totalCount);
    RetainedMessageNode *getChildren(const std::string &subtopic) const;
};

/**
 * @brief A QueuedSessionRemoval is a sort of delayed request for removal. They are kept in a sorted list for fast insertion,
 * and fast dequeueing of expired entries from the start.
 *
 * You can have multiple of these in the pending list. If a client has picked up the session again, the removal is not executed.
 */
class QueuedSessionRemoval
{
    std::weak_ptr<Session> session;
    std::chrono::time_point<std::chrono::steady_clock> expiresAt;

public:
    QueuedSessionRemoval(const std::shared_ptr<Session> &session);
    std::chrono::time_point<std::chrono::steady_clock> getExpiresAt() const;
    std::shared_ptr<Session> getSession() const;
};

class SubscriptionStore
{
#ifdef TESTING
    friend class MainTests;
#endif

    SubscriptionNode root;
    SubscriptionNode rootDollar;
    pthread_rwlock_t subscriptionsRwlock = PTHREAD_RWLOCK_INITIALIZER;
    std::unordered_map<std::string, std::shared_ptr<Session>> sessionsById;
    const std::unordered_map<std::string, std::shared_ptr<Session>> &sessionsByIdConst;

    std::mutex queuedSessionRemovalsMutex;
    std::list<QueuedSessionRemoval> queuedSessionRemovals;

    pthread_rwlock_t retainedMessagesRwlock = PTHREAD_RWLOCK_INITIALIZER;
    RetainedMessageNode retainedMessagesRoot;
    RetainedMessageNode retainedMessagesRootDollar;
    int64_t retainedMessageCount = 0;

    std::mutex pendingWillsMutex;
    std::list<std::weak_ptr<Publish>> pendingWillMessages;

    std::chrono::time_point<std::chrono::steady_clock> lastTreeCleanup;

    Logger *logger = Logger::getInstance();

    void publishNonRecursively(const std::unordered_map<std::string, Subscription> &subscribers,
                               std::forward_list<ReceivingSubscriber> &targetSessions) const;
    void publishRecursively(std::vector<std::string>::const_iterator cur_subtopic_it, std::vector<std::string>::const_iterator end,
                            SubscriptionNode *this_node, std::forward_list<ReceivingSubscriber> &targetSessions) const;
    void giveClientRetainedMessagesRecursively(ProtocolVersion protocolVersion, std::vector<std::string>::const_iterator cur_subtopic_it,
                                               std::vector<std::string>::const_iterator end, RetainedMessageNode *this_node, bool poundMode,
                                               std::forward_list<MqttPacket> &packetList) const;
    void getRetainedMessages(RetainedMessageNode *this_node, std::vector<RetainedMessage> &outputList) const;
    void getSubscriptions(SubscriptionNode *this_node, const std::string &composedTopic, bool root,
                          std::unordered_map<std::string, std::list<SubscriptionForSerializing>> &outputList) const;
    void countSubscriptions(SubscriptionNode *this_node, int64_t &count) const;

    SubscriptionNode *getDeepestNode(const std::string &topic, const std::vector<std::string> &subtopics);
public:
    SubscriptionStore();

    void addSubscription(std::shared_ptr<Client> &client, const std::string &topic, const std::vector<std::string> &subtopics, char qos);
    void removeSubscription(std::shared_ptr<Client> &client, const std::string &topic);
    void registerClientAndKickExistingOne(std::shared_ptr<Client> &client);
    void registerClientAndKickExistingOne(std::shared_ptr<Client> &client, bool clean_start, uint16_t maxQosPackets, uint32_t sessionExpiryInterval);
    std::shared_ptr<Session> lockSession(const std::string &clientid);

    void sendQueuedWillMessages();
    void queueWillMessage(std::shared_ptr<Publish> &willMessage, bool forceNow = false);
    void queuePacketAtSubscribers(PublishCopyFactory &copyFactory, bool dollar = false);
    uint64_t giveClientRetainedMessages(const std::shared_ptr<Client> &client, const std::shared_ptr<Session> &ses,
                                        const std::vector<std::string> &subscribeSubtopics, char max_qos);

    void setRetainedMessage(const std::string &topic, const std::vector<std::string> &subtopics, const std::string &payload, char qos);

    void removeSession(const std::shared_ptr<Session> &session);
    void removeExpiredSessionsClients();

    int64_t getRetainedMessageCount() const;
    uint64_t getSessionCount() const;
    int64_t getSubscriptionCount();

    void saveRetainedMessages(const std::string &filePath);
    void loadRetainedMessages(const std::string &filePath);

    void saveSessionsAndSubscriptions(const std::string &filePath);
    void loadSessionsAndSubscriptions(const std::string &filePath);

    void queueSessionRemoval(const std::shared_ptr<Session> &session);
};

#endif // SUBSCRIPTIONSTORE_H

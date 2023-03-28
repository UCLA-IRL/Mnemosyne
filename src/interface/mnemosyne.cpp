#include "mnemosyne/mnemosyne.hpp"
#include "mnemosyne/backend.hpp"
#include "interface/seen-event-set.h"
#include "interface/self-inserted-set.h"
#include "util.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>

NDN_LOG_INIT(mnemosyne.impl);

using namespace ndn;
namespace mnemosyne {

Mnemosyne::Mnemosyne(const Config &config, KeyChain &keychain, Face &network,
                     std::shared_ptr<ndn::security::Validator> recordValidator,
                     std::shared_ptr<ndn::security::Validator> eventValidator) :
        m_randomEngine(std::random_device()()),
        m_config(config),
        m_keychain(keychain),
        m_scheduler(network.getIoService()),
        m_eventValidator(std::move(eventValidator)),
        m_seenEvents(std::make_unique<interface::SeenEventSet>(config.seenEventTtl)),
        m_selfInsertEventProducers(std::make_unique<interface::SelfInsertedSet>(config.selfInsertResetFreq)),
        m_ready(false),
        m_lastImmutableSeqNo(0),
        m_dagSync(m_config, keychain, network, std::move(recordValidator),
                  [this](const auto &record) { onRecordUpdate(record); }) {
    for (const auto &psName: config.svsPubSubInterfacePrefixes) {
        m_interfacePubSubs.emplace_back(psName, config.peerPrefix, network, [](const auto &i) {}, getSecurityOption());
    }
    for (const auto &syncName: config.svsInterfacePrefixes) {
        m_interfaceSyncs.push_back(std::make_unique<svs::SVSync>(syncName, config.peerPrefix, network,
                                                                 [this, groupId = m_interfaceSyncs.size()](
                                                                         const auto &d) { onSyncUpdate(groupId, d); },
                                                                 getSecurityOption()));
    }

    m_scheduler.schedule(time::nanoseconds(std::chrono::nanoseconds(config.startUpDelay).count()),
                         [this]() {
                             m_ready = true;
                             for (auto &ps: m_interfacePubSubs) {
                                 ps.subscribeToProducer(Name("/"), [this](const auto &d) { onSubscriptionData(d); });
                             }
                         });
}

void Mnemosyne::onSubscriptionData(const svs::SVSPubSub::SubscriptionData &subData) {
    if (!subData.packet) {
        NDN_LOG_WARN("error");
        return;
    }
    onEventData(*subData.packet, subData.producerPrefix);
}

void Mnemosyne::onSyncUpdate(uint32_t groupId, const std::vector<ndn::svs::MissingDataInfo> &info) {
    if (!m_ready) {
        return;
    }
    for (const auto &s: info) {
        for (ndn::svs::SeqNo i = s.low; i <= s.high; i++) {
            NDN_LOG_DEBUG("Interface Sync " << groupId << " Fetching item " << s.nodeId << " " << i);
            m_interfaceSyncs[groupId]->fetchData(s.nodeId, i, [this, nodeId = s.nodeId](const Data &data) {
                onEventData(data, nodeId);
            }, m_config.interfaceSyncRetries);
        }
    }
}

void Mnemosyne::onEventData(const Data &data, const ndn::Name& producer) {
    auto delayedEventInsert = [this, data, producer]() {
        if (m_seenEvents->hasEvent(data.getFullName())) {
            NDN_LOG_DEBUG("Event data " << data.getFullName()
                                        << " found in DAG. ");
            return;
        }
        NDN_LOG_DEBUG("Event data " << data.getFullName()
                                    << " not found in DAG. Publishing...");
        m_selfInsertEventProducers->insert(producer);
        Record record(data, producer);
        auto ret = m_dagSync.createRecord(record);
        if (ret.success())
            NDN_LOG_INFO(m_config.peerPrefix << " Published event data " << data.getFullName()
                                             << " in record " << Record::getRecordSeqId(record.getRecordFullName()));
    };

    NDN_LOG_DEBUG("Received event data " << data.getFullName());
    if (m_selfInsertEventProducers->count(producer)) {
        delayedEventInsert();
        return;
    }

    std::uniform_int_distribution<uint32_t> delayDistribution(m_config.insertBackoffMinMs, m_config.insertBackoffMaxMs);
    if (!m_seenEvents->hasEvent(data.getFullName()))
        m_scheduler.schedule(time::milliseconds(delayDistribution(m_randomEngine)), delayedEventInsert);
}

ndn::svs::SecurityOptions Mnemosyne::getSecurityOption() {
    ndn::svs::SecurityOptions option(m_keychain);
    option.validator = m_eventValidator ? make_shared<::util::cxxValidator>(m_eventValidator) : nullptr;
    option.encapsulatedDataValidator = option.validator;
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(m_keychain, security::signingByIdentity(
            m_config.peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = option.dataSigner;
    return option;
}

void Mnemosyne::onRecordUpdate(const Record &record) {
    auto onValidated = [&](const Data &eventData) {
        const auto &eventFullName = eventData.getFullName();
        m_seenEvents->addEvent(eventFullName);

        //remove self inserted event
        m_selfInsertEventProducers->receivedOther(eventFullName);

        // logging replication id
        auto replicationSeqId = m_dagSync.getMaxReferenceSeqNo();
        if (replicationSeqId != m_lastImmutableSeqNo) {
            m_lastImmutableSeqNo = replicationSeqId;
            NDN_LOG_INFO(m_config.peerPrefix << " immutable record ends at " << replicationSeqId);
        }
    };

    if (m_eventValidator) {
        m_eventValidator->validate(record.getContentData().value(), onValidated,
                                   [](const auto &data, const auto &error) {
                                       NDN_LOG_ERROR("Verification error on event record " << data.getFullName() << ": "
                                                                                           << error);
                                   });
    } else {
        onValidated(record.getContentData().value());
    }
}

Mnemosyne::~Mnemosyne() = default;

}  // namespace mnemosyne
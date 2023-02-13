#include "mnemosyne/mnemosyne.hpp"
#include "mnemosyne/backend.hpp"
#include "interface/seen-event-set.h"
#include "util.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>

NDN_LOG_INIT(mnemosyne.impl);

using namespace ndn;
namespace mnemosyne {


Mnemosyne::Mnemosyne(const Config &config, KeyChain &keychain, Face &network, std::shared_ptr<ndn::security::Validator> recordValidator, std::shared_ptr<ndn::security::Validator> eventValidator) :
        m_config(config),
        m_keychain(keychain),
        m_backend(std::make_shared<Backend>(config.databaseType, config.databasePath, m_config.seqNoBackupFreq)),
        m_dagSync(m_config, m_backend, keychain, network, std::move(recordValidator)),
        m_scheduler(network.getIoService()),
        m_eventValidator(std::move(eventValidator)),
        m_seenEvents(std::make_unique<interface::SeenEventSet>(config.seenEventTtl)),
        m_ready(false) {
    for (const auto &psName: config.svsPubSubInterfacePrefixes) {
        m_interfacePubSubs.emplace_back(psName, config.peerPrefix, network, [](const auto &i) {}, getSecurityOption());
    }
    for (const auto &syncName: config.svsInterfacePrefixes) {
        m_interfaceSyncs.push_back(std::make_unique<svs::SVSync>(syncName, config.peerPrefix, network,
                                                                 [this, groupId = m_interfaceSyncs.size()](
                                                                         const auto &d) { onSyncUpdate(groupId, d); },
                                                                 getSecurityOption()));
    }
    m_dagSync.setOnRecordCallback([&](const auto &record) { onRecordUpdate(record); });

    m_scheduler.schedule(time::nanoseconds(std::chrono::nanoseconds(config.startUpDelay).count()),
                         [this]() {
                             m_ready = true;
                             for (auto &ps: m_interfacePubSubs) {
                                 ps.subscribeToProducer(Name("/"), [this](const auto &d) { onSubscriptionData(d); });
                             }
                         });
}

void Mnemosyne::onSubscriptionData(const svs::SVSPubSub::SubscriptionData& subData) {
    if (!subData.packet) {
        NDN_LOG_WARN("error");
        return;
    }
    onEventData(*subData.packet, subData.producerPrefix, subData.seqNo);
}

void Mnemosyne::onSyncUpdate(uint32_t groupId, const std::vector<ndn::svs::MissingDataInfo>& info) {
    if (!m_ready) return;
    for (const auto& s : info) {
        for (ndn::svs::SeqNo i = s.low; i < s.high; i ++) {
            m_interfaceSyncs[groupId]->fetchData(s.nodeId, i, [this, s, i](const Data& data){
                onEventData(data, s.nodeId, i);
            });
        }
    }
}

void Mnemosyne::onEventData(const Data& data, ndn::Name producer, ndn::svs::SeqNo seqId) {
    m_eventValidator->validate(data,
                               [this, producer, seqId](const Data &eventData) {
                                   std::uniform_int_distribution<uint32_t> delayDistribution(0,
                                                                                             m_config.insertBackoffMaxMs);
                                   NDN_LOG_INFO("Received event data " << eventData.getFullName());
                                   if (m_seenEvents->hasEvent(eventData.getFullName())) return;
                                   m_scheduler.schedule(time::milliseconds(delayDistribution(m_randomEngine)),
                                                        [this, eventData, producer, seqId]() {
                                                            if (m_seenEvents->hasEvent(eventData.getFullName())) {
                                                                NDN_LOG_INFO("Event data " << eventData.getFullName()
                                                                                           << " found in DAG. ");
                                                                return;
                                                            } else {
                                                                NDN_LOG_INFO("Event data " << eventData.getFullName()
                                                                                           << " not found in DAG. Publishing...");
                                                            }
                                                            Record record(eventData, producer, seqId);
                                                            m_dagSync.createRecord(record);
                                                        });
                               }, [](const Data &eventData, auto &&error) {
                NDN_LOG_ERROR("Event data " << eventData.getFullName() << " verification error: " << error);
            });
}

ndn::svs::SecurityOptions Mnemosyne::getSecurityOption() {
    ndn::svs::SecurityOptions option(m_keychain);
    option.validator = make_shared<::util::cxxValidator>(m_eventValidator);
    option.encapsulatedDataValidator = option.validator;
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(m_keychain, security::signingByIdentity(m_config.peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = option.dataSigner;
    return option;
}

void Mnemosyne::onRecordUpdate(const Record& record) {
    m_eventValidator->validate(record.getContentData().value(), [&](const auto& eventData){
        const auto& eventFullName = eventData.getFullName();
        m_seenEvents->addEvent(eventFullName);
    }, [](const auto& data, const auto& error){
        NDN_LOG_INFO("Verification error on event record " << data.getFullName() << ": " << error);
    });
}

Mnemosyne::~Mnemosyne() = default;

}  // namespace mnemosyne
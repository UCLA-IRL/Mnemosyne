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


const std::string Mnemosyne::SEEN_EVENT_BACKUP_KEY = "MnemosyneSeenEvent";


Mnemosyne::Mnemosyne(const Config &config, KeyChain &keychain, Face &network,
                     std::shared_ptr<ndn::security::Validator> recordValidator,
                     std::shared_ptr<ndn::security::Validator> eventValidator) :
        m_config(config),
        m_keychain(keychain),
        m_backend(std::make_shared<Backend>(config.databaseType, config.databasePath, m_config.seqNoBackupFreq)),
        m_scheduler(network.getIoService()),
        m_eventValidator(std::move(eventValidator)),
        m_seenEvents(std::make_unique<interface::SeenEventSet>(config.seenEventTtl)),
        m_ready(false),
        m_dagSync(m_config, m_backend, keychain, network, std::move(recordValidator),
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

    onBackupRecovery();
    m_backend->addBackupCallback([this]() { return onBackup(); });
}

void Mnemosyne::onBackupRecovery() {
    auto s = m_backend->getMetaData(SEEN_EVENT_BACKUP_KEY);
    if (s) {
        try {
            ndn::Block block(make_span(reinterpret_cast<const uint8_t *>(s->data()), s->size()));
            m_seenEvents->decode(block);
            std::cerr << "Backend: seen event recovery success\n";
        } catch (const std::exception &e) {
            std::cerr << "Backend: seen event recovery failed with exception: " << e.what() << "\n";
            exit(1);
        }
    }
}

void Mnemosyne::onSubscriptionData(const svs::SVSPubSub::SubscriptionData &subData) {
    if (!subData.packet) {
        NDN_LOG_WARN("error");
        return;
    }
    m_eventValidator->validate(*subData.packet, [this, prefix=subData.producerPrefix, seqId=subData.seqNo](const auto& data) {
        onEventData(data, prefix, seqId);
    }, [](const Data &eventData, auto &&error) {
        NDN_LOG_ERROR("Event data " << eventData.getFullName() << " verification error: " << error);
    });
}

void Mnemosyne::onSyncUpdate(uint32_t groupId, const std::vector<ndn::svs::MissingDataInfo> &info) {
    if (!m_ready) {
        return;
    }
    for (const auto &s: info) {
        for (ndn::svs::SeqNo i = s.low; i <= s.high; i++) {
            m_interfaceSyncs[groupId]->fetchData(s.nodeId, i, [this, s, i](const Data &data) {
                onEventData(data, s.nodeId, i);
            });
        }
    }
}

void Mnemosyne::onEventData(const Data &data, const ndn::Name& producer, ndn::svs::SeqNo seqId) {
    std::uniform_int_distribution<uint32_t> delayDistribution(0,
                                                              m_config.insertBackoffMaxMs);
    NDN_LOG_INFO("Received event data " << data.getFullName());
    if (m_seenEvents->hasEvent(data.getFullName())) return;
    m_scheduler.schedule(time::milliseconds(delayDistribution(m_randomEngine)),
                         [this, data, producer, seqId]() {
                             if (m_seenEvents->hasEvent(data.getFullName())) {
                                 NDN_LOG_INFO("Event data " << data.getFullName()
                                                            << " found in DAG. ");
                                 return;
                             } else {
                                 NDN_LOG_INFO("Event data " << data.getFullName()
                                                            << " not found in DAG. Publishing...");
                             }
                             Record record(data, producer, seqId);
                             m_dagSync.createRecord(record);
                         });
}

ndn::svs::SecurityOptions Mnemosyne::getSecurityOption() {
    ndn::svs::SecurityOptions option(m_keychain);
    option.validator = make_shared<::util::cxxValidator>(m_eventValidator);
    option.encapsulatedDataValidator = option.validator;
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(m_keychain, security::signingByIdentity(
            m_config.peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = option.dataSigner;
    return option;
}

void Mnemosyne::onRecordUpdate(const Record &record) {
    m_eventValidator->validate(record.getContentData().value(), [&](const auto &eventData) {
        const auto &eventFullName = eventData.getFullName();
        m_seenEvents->addEvent(eventFullName);
    }, [](const auto &data, const auto &error) {
        NDN_LOG_INFO("Verification error on event record " << data.getFullName() << ": " << error);
    });
}

bool Mnemosyne::onBackup() {
    auto b = m_seenEvents->encode();
    std::string page((const char *) b.wire(), b.size());
    if (m_backend->placeMetaData(SEEN_EVENT_BACKUP_KEY, page)) {
        NDN_LOG_INFO("Mnemosyne: seen event metadata backup write success");
        return true;
    } else {
        NDN_LOG_INFO("Mnemosyne: seen event metadata backup write failure");
        return false;
    }
}

Mnemosyne::~Mnemosyne() = default;

}  // namespace mnemosyne
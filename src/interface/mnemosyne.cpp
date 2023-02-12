#include "mnemosyne/mnemosyne.hpp"
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
        m_dagSync(m_config, keychain, network, std::move(recordValidator)),
        m_scheduler(network.getIoService()),
        m_interfacePS(config.interfacePrefix, config.peerPrefix, network, [](const auto& i){}, getSecurityOption()),
        m_eventValidator(std::move(eventValidator)),
        m_seenEvents(std::make_unique<interface::SeenEventSet>(config.seenEventTtl))
{
    m_interfacePS.subscribeToProducer(Name("/"), [&](const auto& d){ onSubscriptionData(d);});
    m_dagSync.setOnRecordCallback([&](const auto& record) {onRecordUpdate(record);});
}

void Mnemosyne::onSubscriptionData(const svs::SVSPubSub::SubscriptionData& subData) {
    if (!subData.packet) {
        NDN_LOG_WARN("error");
        return;
    }
    m_eventValidator->validate(*subData.packet,
                               [this, producer=subData.producerPrefix, seqId=subData.seqNo](const Data& eventData){
        std::uniform_int_distribution<uint32_t> delayDistribution(0, m_config.insertBackoffMaxMs);
        NDN_LOG_INFO("Received event data " << eventData.getFullName());
        if (m_seenEvents->hasEvent(eventData.getFullName())) return;
        m_scheduler.schedule(time::milliseconds(delayDistribution(m_randomEngine)), [this, eventData, producer, seqId]() {
            if (m_seenEvents->hasEvent(eventData.getFullName())) {
                NDN_LOG_INFO("Event data " << eventData.getFullName() << " found in DAG. ");
                return;
            } else {
                NDN_LOG_INFO("Event data " << eventData.getFullName() << " not found in DAG. Publishing...");
            }
            Record record(eventData, producer, seqId);
            m_dagSync.createRecord(record);
        });
    }, [](const Data& eventData, auto&& error){
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

void Mnemosyne::onRecordUpdate(Record record) {
    m_eventValidator->validate(record.getContentData().value(), [&](const auto& eventData){
        const auto& eventFullName = eventData.getFullName();
        m_seenEvents->addEvent(eventFullName);
    }, [](const auto& data, const auto& error){
        NDN_LOG_INFO("Verification error on event record " << data.getFullName() << ": " << error);
    });
}

Mnemosyne::~Mnemosyne() = default;

}  // namespace mnemosyne
#ifndef MNEMOSYNE_MNEMOSYNE_H_
#define MNEMOSYNE_MNEMOSYNE_H_

#include "config.hpp"
#include "mnemosyne/mnemosyne-dag-logger.hpp"
#include <ndn-svs/svspubsub.hpp>

using namespace ndn;
namespace mnemosyne {

namespace interface {
class SeenEventSet;
}

class Mnemosyne {
  public:
    /**
     * Initialize a Mnemosyne instance from the config.
     * @p config, input, the configuration of multicast prefix, peer prefix, and settings of Mnemosyne behavior
     * @p keychain, input, the local NDN keychain instance
     * @p face, input, the localhost NDN face to send/receive NDN packets.
     * @p recordValidator, a validator that validates records from other nodes
     * @p eventValidator, a validator that validates events from clients
     */
    Mnemosyne(const mnemosyne::Config &config, security::KeyChain &keychain, Face &network,
              std::shared_ptr<ndn::security::Validator> recordValidator,
              std::shared_ptr<ndn::security::Validator> eventValidator);

    virtual ~Mnemosyne();

    inline std::shared_ptr<Backend> getBackend() {
        return m_backend;
    }

  private:
    void onSubscriptionData(const svs::SVSPubSub::SubscriptionData& subData);
    void onSyncUpdate(uint32_t groupId, const std::vector<ndn::svs::MissingDataInfo>& info);
    void onEventData(const Data& data, ndn::Name producer, ndn::svs::SeqNo seqId);

    ndn::svs::SecurityOptions getSecurityOption();

    void onRecordUpdate(const Record& record);

  protected:
    const Config m_config;
    std::shared_ptr<Backend> m_backend;
    security::KeyChain &m_keychain;
    MnemosyneDagLogger m_dagSync;
    Scheduler m_scheduler;
    std::shared_ptr<ndn::security::Validator> m_eventValidator;
    //TODO persistence
    std::unique_ptr<interface::SeenEventSet> m_seenEvents;
    std::mt19937_64 m_randomEngine;

    bool m_ready;

    std::list<svs::SVSPubSub> m_interfacePubSubs;
    std::vector<std::unique_ptr<svs::SVSync>> m_interfaceSyncs;
  };

} // namespace mnemosyne

#endif // MNEMOSYNE_MNEMOSYNE_H_
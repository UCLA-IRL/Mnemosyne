#ifndef MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_
#define MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_

#include "record.hpp"
#include "config.hpp"
#include "return-code.hpp"
#include <ndn-svs/svsync-shared.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/io.hpp>
#include <stack>
#include <random>
#include <utility>


using namespace ndn;
namespace mnemosyne {

class Backend;
class DagReferenceChecker;

namespace dag {
class RecordSync;
}

class MnemosyneDagLogger {
  public:
    /**
   * Initialize a MnemosyneDagLogger instance from the config.
   * @p config, input, the configuration of multicast prefix, peer prefix, and settings of Dledger behavior
   * @p keychain, input, the local NDN keychain instance
   * @p face, input, the localhost NDN face to send/receive NDN packets.
   */
    MnemosyneDagLogger(const Config &config, security::KeyChain &keychain, Face &network, std::shared_ptr<ndn::security::Validator> m_recordValidator);

    virtual ~MnemosyneDagLogger();

    /**
     * Create a new record to the MnemosyneDagLogger.
     * @p record, input, a record instance which contains the record payload
     */
    virtual ReturnCode
    createRecord(Record &record);

    /**
     * Get an existing record from the MnemosyneDagLogger.
     * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
     */
    virtual optional<Record>
    getRecord(const std::string &recordName) const;

    /**
     * Check whether the record exists in the MnemosyneDagLogger.
     * @p recordName, input, the name of the record, which is an NDN full name (i.e., containing ImplicitSha256DigestComponent component)
     */
    virtual bool
    hasRecord(const std::string &recordName) const;

    /**
      * list the record exists in the MnemosyneDagLogger.
      * @p recordName, input, the name of the record, which is an NDN name prefix.
      */
    virtual std::list<Name>
    listRecord(const std::string &prefix) const;

    const Name& getPeerPrefix() const;

    void setOnRecordCallback(std::function<void(const Record&)> callback) {
        m_onRecordCallback = std::move(callback);
    }

  private:
    void onUpdate(const std::vector<ndn::svs::MissingDataInfo>& info);

    void addReceivedRecord(std::unique_ptr<Record> record, const Name& producer, svs::SeqNo seqId);

    static ndn::svs::SecurityOptions getSecurityOption(KeyChain& keychain, shared_ptr<ndn::security::Validator> recordValidator, Name peerPrefix);

  protected:
    uint64_t m_KnownSelfSeqId;
    const Config m_config;
    std::shared_ptr<Backend> m_backend;
    std::unique_ptr<DagReferenceChecker> m_dagReferenceChecker;
    security::KeyChain &m_keychain;
    std::unique_ptr<mnemosyne::dag::RecordSync> m_dagSync;
    std::function<void(const Record&)> m_onRecordCallback;

    std::unordered_map<Name, Name> m_lastRecordInChains;

    std::mt19937_64 m_randomEngine;

    void addPublicGenesisRecord();

    void restoreRecordSyncVersionVector();
};

} // namespace mnemosyne

#endif // MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_
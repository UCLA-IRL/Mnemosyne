#ifndef MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_
#define MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_

#include "record.hpp"
#include "logger-config.hpp"
#include "return-code.hpp"
#include "backend.hpp"
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

class DagReferenceChecker;

namespace dag {
class RecordSync;

class ReplicationCounter;
}

class MnemosyneDagLogger {
  public:
    /**
   * Initialize a MnemosyneDagLogger instance from the config.
   * @p config, input, the configuration of multicast prefix, peer prefix, and settings of Dledger behavior
   * @p keychain, input, the local NDN keychain instance
   * @p face, input, the localhost NDN face to send/receive NDN packets.
   */
    MnemosyneDagLogger(const LoggerConfig &config, security::KeyChain &keychain,
                       Face &network, std::shared_ptr<ndn::security::Validator> m_recordValidator,
                       std::function<void(const Record &)> onRecordCallback = nullptr);

    virtual ~MnemosyneDagLogger();

    /**
     * Create a new record to the MnemosyneDagLogger.
     * @p record, input, a record instance which contains the record payload
     */
    virtual ReturnCode
    createRecord(Record &record);

    /**
     * return the current list for determining the replication count.
     * @return a list of highest sequence number for each weight, starting at maxCount and ending with 0
     */
    std::list<uint64_t> getReplicationSeqId() const;
    uint64_t getMaxReferenceSeqNo() const;

    const Name &getPeerPrefix() const;

    void setOnRecordCallback(std::function<void(const Record &)> callback) {
        m_onRecordCallback = std::move(callback);
    }

    inline std::shared_ptr<Backend> getBackend() {
        return m_backend;
    }

  private:
    void onUpdate(const std::vector<ndn::svs::MissingDataInfo> &info);

    void addReceivedRecord(std::unique_ptr<Record> record, const Name &producer, svs::SeqNo seqId);

    bool versionBackupCallback();

    static ndn::svs::SecurityOptions
    getSecurityOption(KeyChain &keychain, shared_ptr<ndn::security::Validator> recordValidator, Name peerPrefix);

    static const std::string SEQ_NO_BACKUP_KEY;

  protected:
    uint64_t m_KnownSelfSeqId;
    const LoggerConfig m_config;
    std::shared_ptr<Backend> m_backend;
    std::unique_ptr<DagReferenceChecker> m_dagReferenceChecker;
    std::unique_ptr<dag::ReplicationCounter> m_replicationCounter;
    ndn::svs::VersionVector m_dagCollectedVersions;
    std::unique_ptr<dag::RecordSync> m_dagSync;
    std::function<void(const Record &)> m_onRecordCallback;

    std::unordered_map<Name, std::pair<Name, uint32_t>> m_lastRecordInChains;

    std::mt19937_64 m_randomEngine;

    void addPublicGenesisRecord();

    void restoreRecordSyncVersionVector();
};

} // namespace mnemosyne

#endif // MNEMOSYNE_MNEMOSYNE_DAG_SYNC_H_
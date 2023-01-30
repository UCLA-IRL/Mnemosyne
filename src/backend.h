#ifndef MNEMOSYNE_BACKEND_H_
#define MNEMOSYNE_BACKEND_H_

#include <ndn-cxx/data.hpp>
#include "seq-no-recovery.h"

using namespace ndn;
namespace mnemosyne {

class Storage;

class Backend {
  public:
    Backend(const std::string &dbDir, uint32_t seqNoBackupFreq = 1);

  public:
    ~Backend() = default;

    // @param the recordName must be a full name (i.e., containing explicit digest component)
    shared_ptr<Data>
    getRecord(const Name &recordName) const;

    bool
    putRecord(const shared_ptr<const Data> &recordData);

    void
    deleteRecord(const Name &recordName);

    std::list<Name>
    listRecord(const Name &prefix) const;

    void SeqNumAdd(uint32_t group, const ndn::Name& producer, uint64_t val);

    bool isSeqNumIn(uint32_t group, const ndn::Name& producer, uint64_t val) const;

  private:
    std::shared_ptr<Storage> m_storage;
    SeqNoRecovery m_seqNoRecovery;
    uint32_t m_seqNoBackupFreq;
    uint32_t m_lastSeqNoBackup;

    static const std::string SEQ_NO_BACKUP_KEY;
  public:
    static const uint32_t DAG_SYNC_GROUP = 163;
    static const uint32_t MNEMOSYNE_PS_GROUP = 165;
    static const uint32_t LISTEN_GROUP = 167;
};

}  // namespace mnemosyne

#endif  // MNEMOSYNE_BACKEND_H_
#ifndef MNEMOSYNE_BACKEND_H_
#define MNEMOSYNE_BACKEND_H_

#include "mnemosyne/logger-config.hpp"
#include <ndn-svs/version-vector.hpp>
#include <ndn-cxx/data.hpp>

using namespace ndn;
namespace mnemosyne {

namespace storage {
class Storage;
}

/**
 * Class for providing record and metadata persistence using storage
 */
class Backend {
  public:
    Backend(const LoggerConfig& config);
    Backend(const std::string &storage_type, const std::string &dbDir, uint32_t seqNoBackupFreq = 1);

  public:
    ~Backend() = default;

    // @param the recordName must be a full name (i.e., containing explicit digest component)
    shared_ptr<const Data> getRecord(const Name &recordName) const;

    bool
    putRecord(const shared_ptr<const Data> &recordData);

    void
    deleteRecord(const Name &recordName);

    /**
     *
     * @param prefix
     * @param count = 0 if listing all in prefix=start.
     * @return
     */
    std::list<Name> listRecord(const Name &prefix, uint32_t count = 0) const;

    void seqNumSet(const ndn::Name& producer, uint64_t val);

    const svs::VersionVector& seqNumGet() const;

  private:
    std::shared_ptr<storage::Storage> m_storage;
    ndn::svs::VersionVector m_versionRecovery;
    uint32_t m_seqNoBackupFreq;
    uint32_t m_lastSeqNoBackup;

    static const std::string SEQ_NO_BACKUP_KEY;
};

}  // namespace mnemosyne

#endif  // MNEMOSYNE_BACKEND_H_
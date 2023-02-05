#ifndef MNEMOSYNE_STORAGE_LEVELDB_H_
#define MNEMOSYNE_STORAGE_LEVELDB_H_

#include "storage.h"
#include <ndn-cxx/data.hpp>
#include <leveldb/db.h>

namespace mnemosyne {
namespace storage {

class StorageLevelDb : public Storage {
  public:
    StorageLevelDb(const std::string &dbDir);

  public:
    ~StorageLevelDb() override;

    // @param the recordName must be a full name (i.e., containing explicit digest component)
    std::shared_ptr<const ndn::Data> getRecord(const ndn::Name &recordName) const override;

    bool putRecord(const std::shared_ptr<const ndn::Data> &recordData) override;

    void deleteRecord(const ndn::Name &recordName) override;

    std::list<ndn::Name> listRecord(const ndn::Name &prefix, uint32_t count = 0) const override;

    bool placeMetaData(std::string key, const std::string &value) override;

    std::optional<std::string> getMetaData(const std::string &key) const override;

  private:
    leveldb::DB *m_db;
    const char RECORD_PREFIX_CHAR = '/';
};

}  // namespace storage
}  // namespace mnemosyne

#endif  // MNEMOSYNE_STORAGE_LEVELDB_H_
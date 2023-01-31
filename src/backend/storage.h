#ifndef MNEMOSYNE_STORAGE_H_
#define MNEMOSYNE_STORAGE_H_

#include <ndn-cxx/data.hpp>
#include <leveldb/db.h>

using namespace ndn;
namespace mnemosyne {

class Storage {
  public:
    Storage(const std::string &dbDir);

  public:
    ~Storage();

    // @param the recordName must be a full name (i.e., containing explicit digest component)
    shared_ptr<Data>
    getRecord(const Name &recordName) const;

    bool
    putRecord(const shared_ptr<const Data> &recordData);

    void
    deleteRecord(const Name &recordName);

    std::list<Name>
    listRecord(const Name &prefix) const;

    bool placeMetaData(std::string key, const std::string& value);

    std::optional<std::string> getMetaData(const std::string& key) const;

  private:
    leveldb::DB *m_db;
    const char RECORD_PREFIX_CHAR = '/';
};

}  // namespace mnemosyne

#endif  // MNEMOSYNE_STORAGE_H_
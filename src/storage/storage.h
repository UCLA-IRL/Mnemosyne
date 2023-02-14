#ifndef MNEMOSYNE_STORAGE_H
#define MNEMOSYNE_STORAGE_H

#include <string>
#include <optional>
#include <ndn-cxx/name.hpp>
#include <ndn-cxx/data.hpp>

#include <boost/noncopyable.hpp>

namespace mnemosyne::storage {

class Storage {
  public:
    virtual ~Storage() = default;

    // @param the recordName must be a full name (i.e., containing explicit digest component)
    virtual std::shared_ptr<const ndn::Data> getRecord(const ndn::Name &recordName) const = 0;

    virtual bool putRecord(const std::shared_ptr<const ndn::Data> &recordData) = 0;

    virtual void
    deleteRecord(const ndn::Name &recordName) = 0;

    /**
     * @param prefix
     * @param count = 0 if listing all in prefix=start.
     * @return
     */
    virtual std::list<ndn::Name> listRecord(const ndn::Name &prefix, uint32_t count = 0) const = 0;

    virtual bool placeMetaData(std::string key, const std::string &value) = 0;

    virtual std::optional<std::string> getMetaData(const std::string &key) const = 0;
};

std::unique_ptr<Storage> getStorage(std::string type, const std::string &config);

} // namespace mnemosyne

#endif // MNEMOSYNE_STORAGE_H

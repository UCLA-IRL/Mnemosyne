#include "storage-memory.h"

#include <cassert>
#include <iostream>

using namespace ndn;
namespace mnemosyne::storage {

shared_ptr<const Data> StorageMemory::getRecord(const Name &recordName) const {
    auto it = m_recordStorage.find(recordName);
    if (it == m_recordStorage.end()) return nullptr;
    else return it->second;
}

bool StorageMemory::putRecord(const shared_ptr<const Data> &recordData) {
    m_recordStorage.emplace(recordData->getFullName(), recordData);
    return true;
}

void StorageMemory::deleteRecord(const Name &recordName) {
    auto it = m_recordStorage.find(recordName);
    if (it != m_recordStorage.end()) {
        m_recordStorage.erase(it);
    }
}

std::list<Name> StorageMemory::listRecord(const Name &prefix, uint32_t count) const {
    std::list<Name> names;
    for (auto it = m_recordStorage.lower_bound(prefix);
            it != m_recordStorage.end() && ((prefix.isPrefixOf(it->first) && count == 0) || names.size() < count); it++) {
        names.emplace_back(it->first);
    }
    return names;
}

bool StorageMemory::placeMetaData(std::string k, const std::string &v) {
    m_metaDataStore.emplace(k, v);
    return true;
}

std::optional<std::string> StorageMemory::getMetaData(const std::string &k) const {
    auto it = m_metaDataStore.find(k);
    if (it == m_metaDataStore.end()) return std::nullopt;
    else return it->second;
}

}  // namespace mnemosyne
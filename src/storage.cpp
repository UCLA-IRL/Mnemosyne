#include "storage.h"

#include <cassert>
#include <iostream>

namespace mnemosyne {

Storage::Storage(const std::string &dbDir) {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, dbDir, &m_db);
    if (!status.ok()) {
        std::cerr << "Unable to open/create database " << dbDir << std::endl;
        std::cerr << status.ToString() << std::endl;
        BOOST_THROW_EXCEPTION(std::runtime_error("Unable to open/create database"));
    }
}

Storage::~Storage() {
    delete m_db;
}

shared_ptr<Data>
Storage::getRecord(const Name &recordName) const {
    const auto &nameStr = recordName.toUri();
    leveldb::Slice key = nameStr;
    std::string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
    if (!s.ok()) {
        return nullptr;
    } else {
        ndn::Block block(make_span(reinterpret_cast<const uint8_t*>(value.data()), value.size()));
        return make_shared<Data>(block);
    }
}

bool
Storage::putRecord(const shared_ptr<const Data> &recordData) {
    const auto &nameStr = recordData->getFullName().toUri();
    leveldb::Slice key = nameStr;
    auto recordBytes = recordData->wireEncode();
    leveldb::Slice value((const char *) recordBytes.wire(), recordBytes.size());
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
    if (!s.ok()) {
        return false;
    }
    return true;
}

void
Storage::deleteRecord(const Name &recordName) {
    const auto &nameStr = recordName.toUri();
    leveldb::Slice key = nameStr;
    leveldb::Status s = m_db->Delete(leveldb::WriteOptions(), key);
    if (!s.ok()) {
        std::cerr << "Unable to delete value from database, key: " << nameStr << std::endl;
        std::cerr << s.ToString() << std::endl;
    }
}

std::list<Name>
Storage::listRecord(const Name &prefix) const {
    std::list<Name> names;
    leveldb::Iterator *it = m_db->NewIterator(leveldb::ReadOptions());
    for (it->Seek(prefix.toUri()); it->Valid() && prefix.isPrefixOf(Name(it->key().ToString())); it->Next()) {
        names.emplace_back(it->key().ToString());
    }
    assert(it->status().ok());  // Check for any errors found during the scan
    delete it;
    return std::move(names);
}

bool Storage::placeMetaData(std::string k, const std::string& v) {
    if (k.empty() || k[0] == RECORD_PREFIX_CHAR) return false;
    leveldb::Slice key = k;
    leveldb::Slice value = v;
    leveldb::Status s = m_db->Put(leveldb::WriteOptions(), key, value);
    if (!s.ok()) {
        return false;
    }
    return true;
}

std::optional<std::string> Storage::getMetaData(const std::string& k) const {
    if (k.empty() || k[0] == RECORD_PREFIX_CHAR) return std::nullopt;
    leveldb::Slice key = k;
    std::string value;
    leveldb::Status s = m_db->Get(leveldb::ReadOptions(), key, &value);
    if (!s.ok()) {
        return std::nullopt;
    } else {
        return value;
    }
}

}  // namespace mnemosyne
//
// Created by Tyler on 1/29/23.
//

#include "mnemosyne/backend.hpp"
#include "storage/storage-leveldb.h"
#include <iostream>


mnemosyne::Backend::Backend(const LoggerConfig& config)
    : Backend(config.databaseType, config.databasePath, config.seqNoBackupFreq) {}

mnemosyne::Backend::Backend(const std::string &storage_type, const std::string &dbDir, uint32_t seqNoBackupFreq) :
        m_storage(storage::getStorage(storage_type, dbDir)),
        m_seqNoBackupFreq(seqNoBackupFreq),
        m_lastSeqNoBackup(seqNoBackupFreq)
{
    if (!m_storage) {
        std::cerr << "Backend: bad storage option\n";
        exit(1);
    }
}

shared_ptr<const Data> mnemosyne::Backend::getRecord(const Name &recordName) const {
    return m_storage->getRecord(recordName);
}

bool mnemosyne::Backend::putRecord(const shared_ptr<const Data> &recordData) {
    return m_storage->putRecord(recordData);
}

void mnemosyne::Backend::deleteRecord(const Name &recordName) {
    m_storage->deleteRecord(recordName);
}

std::list<Name> mnemosyne::Backend::listRecord(const Name &prefix, uint32_t count) const {
    return m_storage->listRecord(prefix, count);
}

bool mnemosyne::Backend::placeMetaData(std::string key, const std::string &value) {
    return m_storage->placeMetaData(std::move(key), value);
}

std::optional<std::string> mnemosyne::Backend::getMetaData(const std::string &key) const {
    return m_storage->getMetaData(key);
}

void mnemosyne::Backend::triggerBackup() {
    m_lastSeqNoBackup ++;
    if (m_lastSeqNoBackup >= m_seqNoBackupFreq) { // backup
        for (auto& func : m_backUpCallbacks) {
            if (!func()) {
                std::cerr << "Backend: metadata backup write failed\n";
                exit(1);
            }
        }
        m_lastSeqNoBackup = 0;
    }
}

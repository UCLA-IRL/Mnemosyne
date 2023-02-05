//
// Created by Tyler on 1/29/23.
//

#include "backend.h"
#include "storage/storage-leveldb.h"
#include <iostream>

const std::string mnemosyne::Backend::SEQ_NO_BACKUP_KEY = "SeqNoBackup";

mnemosyne::Backend::Backend(const std::string &storage_type, const std::string &dbDir, uint32_t seqNoBackupFreq) :
        m_storage(storage::getStorage(storage_type, dbDir)),
        m_seqNoBackupFreq(seqNoBackupFreq),
        m_lastSeqNoBackup(seqNoBackupFreq)
{
    if (!m_storage) {
        std::cerr << "Backend: bad storage option\n";
        exit(1);
    }
    //attempt recovery
    auto page = m_storage->getMetaData(SEQ_NO_BACKUP_KEY);
    if (page) {
        try {
            ndn::Block block(make_span(reinterpret_cast<const uint8_t *>(page->data()), page->size()));
            block.parse();
            for (auto &b: block.elements()) {
                m_versionRecovery[b.type()] = svs::VersionVector(b.blockFromValue());
            }
            std::cerr << "Backend: seq no recovery success\n";
        } catch (const std::exception &e) {
            std::cerr << "Backend: seq no recovery failed with exception: " << e.what() << "\n";
            exit(1);
        }
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

void mnemosyne::Backend::seqNumSet(uint32_t group, const ndn::Name& producer, uint64_t val) {
    m_versionRecovery[group].set(producer, val);
    m_lastSeqNoBackup ++;
    if (m_lastSeqNoBackup >= m_seqNoBackupFreq) { // backup
        Block backupPage(1); // type doesn't matter
        for (const auto& [group_id, vv]: m_versionRecovery) {
            backupPage.push_back(encoding::makeBinaryBlock(group_id, vv.encode()));
        }
        backupPage.encode();
        std::string page((const char *)backupPage.wire(), backupPage.size());
        if (m_storage->placeMetaData(SEQ_NO_BACKUP_KEY, page)) {
            std::cerr << "Backend: metadata backup write success\n";
            m_lastSeqNoBackup = 0;
        } else {
            std::cerr << "Backend: metadata backup write failed\n";
            exit(1);
        }
    }
}

const svs::VersionVector& mnemosyne::Backend::seqNumGet(uint32_t group) const {
    static svs::VersionVector EMPTY_VV;
    auto it = m_versionRecovery.find(group);
    if (it == m_versionRecovery.end()) return EMPTY_VV;
    return it->second;
}

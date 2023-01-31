//
// Created by Tyler on 1/29/23.
//

#include "backend.h"
#include "storage.h"
#include <iostream>

const std::string mnemosyne::Backend::SEQ_NO_BACKUP_KEY = "SeqNoBackup";

mnemosyne::Backend::Backend(const std::string &dbDir, uint32_t seqNoBackupFreq) :
        m_storage(std::make_shared<Storage>(dbDir)),
        m_seqNoBackupFreq(seqNoBackupFreq),
        m_lastSeqNoBackup(0)
{
    //attempt recovery
    auto page = m_storage->getMetaData(SEQ_NO_BACKUP_KEY);
    if (page) {
        ndn::Block block(make_span(reinterpret_cast<const uint8_t*>(page->data()), page->size()));
        if(!m_seqNoRecovery.decode(block)) {
            std::cerr << "Backend: seq no recovery failed\n";
            exit(1);
        } else {
            std::cerr << "Backend: seq no recovery success\n";
        }
    }
}

shared_ptr<Data> mnemosyne::Backend::getRecord(const Name &recordName) const {
    return m_storage->getRecord(recordName);
}

bool mnemosyne::Backend::putRecord(const shared_ptr<const Data> &recordData) {
    return m_storage->putRecord(recordData);
}

void mnemosyne::Backend::deleteRecord(const Name &recordName) {
    m_storage->deleteRecord(recordName);
}

std::list<Name> mnemosyne::Backend::listRecord(const Name &prefix) const {
    return m_storage->listRecord(prefix);
}

void mnemosyne::Backend::SeqNumAdd(uint32_t group, const ndn::Name& producer, uint64_t val) {
    m_seqNoRecovery.getStream(group, producer).add(val);
    m_lastSeqNoBackup ++;
    if (m_lastSeqNoBackup == m_seqNoBackupFreq) { // backup
        auto backupPage = m_seqNoRecovery.encode();
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

bool mnemosyne::Backend::isSeqNumIn(uint32_t group, const ndn::Name& producer, uint64_t val) const {
    return m_seqNoRecovery.getStream(group, producer).isIn(val);
}

std::optional<uint64_t> mnemosyne::Backend::SeqNumLastContinuous(uint32_t group, const Name &producer, uint64_t start) const {
    return m_seqNoRecovery.getStream(group, producer).lastContinuous(start);
}

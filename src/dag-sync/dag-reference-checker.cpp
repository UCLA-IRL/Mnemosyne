//
// Created by Tyler on 1/30/23.
//

#include "dag-reference-checker.h"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>

NDN_LOG_INIT(mnemosyne.dagsync.refChecker);

using namespace ndn;
namespace mnemosyne {

void DagReferenceChecker::verifyPreviousRecord(std::unique_ptr<Record> record, const Name &producer, svs::SeqNo seqId) {
    auto backend = m_backend.lock();
    if (!backend) {
        NDN_LOG_ERROR("Backend freed but dag checker called");
        return;
    }
    auto recordName = record->getRecordFullName();
    for (const auto &i: record->getPointersFromHeader()) {
        if (!Record::isRecordName(i) || !i.get(-1).isImplicitSha256Digest()) {
            NDN_LOG_ERROR("Bad preceding record: " << i << " in " << record->getRecordFullName());
            return;
        }
        if (Record::isGenesisRecord(i)) {
            if (i == Record::getGenesisRecordFullName(i.getPrefix(-1))) continue;
            else {
                NDN_LOG_ERROR("Bad genesis preceding record: " << i << " in " << record->getRecordFullName());
                return;
            }
        } else if (m_waitingRecords.count(i) || !backend->getRecord(i)) { //verification failed
            m_targetForWaitingRecords.emplace(i, recordName);
            m_waitingRecords.emplace(recordName, std::tuple(std::move(record), producer, seqId));
            NDN_LOG_TRACE("record " << recordName << " waiting for " << i);
            return;
        }
    }

    //verification success
    NDN_LOG_TRACE("record checked for reference: " << record->getEncodedData()->getFullName());
    m_readyRecordCallback(std::move(record), producer, seqId);

    std::map<Name, std::tuple<std::unique_ptr<Record>, Name, svs::SeqNo>> waitingList;
    if (m_targetForWaitingRecords.count(recordName) > 0) {
        for (auto it = m_targetForWaitingRecords.find(recordName);
             it->first == recordName; m_targetForWaitingRecords.erase(it++)) {
            auto record_it = m_waitingRecords.find(it->second);
            if (record_it == m_waitingRecords.end()) continue;
            waitingList.emplace(record_it->first, std::move(record_it->second));
            m_waitingRecords.erase(record_it);
        }
    }

    for (auto &[name, p]: waitingList) {
        verifyPreviousRecord(std::move(std::get<0>(p)), std::get<1>(p), std::get<2>(p));
    }
}

DagReferenceChecker::DagReferenceChecker(std::weak_ptr<Backend> backend,
                                         std::function<void(std::unique_ptr<Record>, const Name &,
                                                            svs::SeqNo)> readyRecordCallback) :
        m_backend(std::move(backend)),
        m_readyRecordCallback(std::move(readyRecordCallback)) {

}

void DagReferenceChecker::addRecord(std::unique_ptr<Record> record, const Name &name, svs::SeqNo seqId) {
    verifyPreviousRecord(std::move(record), name, seqId);
}

} //namespace mnemosyne

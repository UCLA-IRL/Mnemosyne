//
// Created by Tyler on 1/30/23.
//

#ifndef MNEMOSYNE_DAG_REFERENCE_CHECKER_H
#define MNEMOSYNE_DAG_REFERENCE_CHECKER_H

#include "backend/backend.h"
#include "mnemosyne/record.hpp"

#include <ndn-svs/svsync.hpp>

namespace mnemosyne {

/**
 * Check record references and cache unchecked records.
 */
class DagReferenceChecker {
  public:
    DagReferenceChecker(std::weak_ptr<Backend> backend,
                        std::function<void(std::unique_ptr <Record> , const Name& , svs::SeqNo)> readyRecordCallback);

    void addRecord(std::unique_ptr<Record> record, const Name& name, svs::SeqNo seqId);

  private:
    void verifyPreviousRecord(std::unique_ptr<Record> record, const Name& producer, svs::SeqNo seqId);

  private:
    std::weak_ptr<Backend> m_backend;
    std::function<void(std::unique_ptr <Record> , const Name& , svs::SeqNo)> m_readyRecordCallback;
    std::unordered_map<Name, std::tuple<std::unique_ptr<Record>, Name, svs::SeqNo>> m_waitingRecords;
    std::multimap<Name, Name> m_targetForWaitingRecords;
};

} // namespace mnemosyne

#endif //MNEMOSYNE_DAG_REFERENCE_CHECKER_H

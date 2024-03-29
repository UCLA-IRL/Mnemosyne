//
// Created by Tyler on 2/6/23.
//

#ifndef MNEMOSYNE_REPLICATION_COUNTER_H
#define MNEMOSYNE_REPLICATION_COUNTER_H

#include "mnemosyne/record.hpp"
#include <ndn-cxx/name.hpp>
#include <unordered_map>

namespace mnemosyne::dag {

/**
 * provide a count on the replication location along this logger's chain
 * May not be consistent when the node restart after failure; but will resolve soon with if max count is reasonable
 * However, if there is an output, that
 */
class ReplicationCounter {

  public:
    ReplicationCounter(ndn::Name peerPrefix, uint32_t maxReference);

    std::list<uint64_t> getCounts() const;

    uint64_t getMaxReferenceSeqNo() const;

    void recordUpdate(const Record &record);

  private:
    uint32_t getLocationSize() const;

  private:
    std::map<uint64_t, std::set<ndn::Name>> m_locations;
    std::unordered_map<ndn::Name, std::map<uint64_t, uint64_t>> m_referencePoints;
    ndn::Name m_peerPrefix;
    uint32_t m_maxReference;

    std::map<uint64_t, uint64_t> &getPrunedRefPointSet(const Name &producer);
};

} // namespace mnemosyne::dag

#endif //MNEMOSYNE_REPLICATION_COUNTER_H

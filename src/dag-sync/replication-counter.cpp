//
// Created by Tyler on 2/6/23.
//

#include "replication-counter.h"
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <utility>

NDN_LOG_INIT(mnemosyne.dagsync.replicationCounter);

mnemosyne::dag::ReplicationCounter::ReplicationCounter(ndn::Name peerPrefix, uint32_t maxReference) :
        m_peerPrefix(std::move(peerPrefix)),
        m_maxReference(maxReference) {
}

std::list<uint64_t> mnemosyne::dag::ReplicationCounter::getCounts() const {
    std::list<uint64_t> ans;
    if (m_maxReference == 0) return ans;
    for (const auto &i: m_locations) {
        ans.push_back(i.first);
    }
    return ans;
}

void mnemosyne::dag::ReplicationCounter::recordUpdate(const mnemosyne::Record &record) {
    if (m_maxReference == 0) return;
    auto producer = Record::getProducerPrefix(record.getEncodedData()->getName());
    if (producer == m_peerPrefix) return;
    for (const auto &i: record.getPointersFromHeader()) {
        if (Record::getProducerPrefix(i) == m_peerPrefix) {
            auto seqId = Record::getRecordSeqId(i);
            if (!m_referencePoints.count(producer) || seqId > m_referencePoints.at(producer)) {
                if (m_referencePoints.count(producer)) m_locations.erase({m_referencePoints.at(producer), producer});
                m_referencePoints.emplace(producer, seqId);
                m_locations.emplace(seqId, producer);

                while (m_referencePoints.size() > m_maxReference) {
                    m_referencePoints.erase(m_locations.begin()->second);
                    m_locations.erase(m_locations.begin());
                }

            }
            return;
        }
    }
}

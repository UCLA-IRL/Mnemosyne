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
    if (m_maxReference <= 0) return ans;
    for (const auto& i: m_locations) {
        for (size_t j = i.second.size(); j > 0; j --)
            ans.push_back(i.first);
    }
    assert(ans.size() == getLocationSize());
    while (ans.size() > m_maxReference) {
        ans.pop_front();
    }
    if (ans.front() > m_locations.begin()->first) {
        NDN_LOG_ERROR("Weight counter size error - extra element: " << ans.front() << ">" << m_locations.begin()->first);
        NDN_THROW(std::runtime_error("Weight counter size error"));
    }
    return ans;
}

uint64_t mnemosyne::dag::ReplicationCounter::getMaxReferenceSeqNo() const {
    if (m_maxReference <= 0) return 0;
    auto locationSize = getLocationSize();
    if (locationSize < m_maxReference) return 0;
    if (locationSize - m_locations.begin()->second.size() >= m_maxReference) {
        NDN_LOG_ERROR("Weight counter size error - extra element: " << locationSize << " with first loc size" <<
            m_locations.begin()->second.size());
        NDN_THROW(std::runtime_error("Weight counter size error"));
    }
    return m_locations.begin()->first;
}

void mnemosyne::dag::ReplicationCounter::recordUpdate(const mnemosyne::Record &record) {
    if (m_maxReference == 0) return;
    auto producer = Record::getProducerPrefix(record.getEncodedData()->getName());
    if (producer == m_peerPrefix) return;
    uint64_t pointedTo = 0;
    for (const auto &i: record.getPointersFromHeader()) {
        auto pointedProducer = Record::getProducerPrefix(i);
        if (pointedProducer == m_peerPrefix)
            pointedTo = std::max(pointedTo, Record::getRecordSeqId(i));
        else if (m_referencePoints.count(pointedProducer)) {
            auto indirectSeqId = Record::getRecordSeqId(i);
            const auto& refPointSet = getPrunedRefPointSet(pointedProducer);
            auto it = refPointSet.lower_bound(indirectSeqId);
            if (it == refPointSet.end() || it->first > indirectSeqId) {
                if (it != refPointSet.begin()) it --;
                else continue;
            }
            pointedTo = std::max(pointedTo, it->second);
        }
    }

    if (pointedTo == 0) return;
    if (!m_locations.empty() && pointedTo < m_locations.begin()->first) return;
    auto seqId = Record::getRecordSeqId(record.getRecordFullName());
    auto& refPointSet = getPrunedRefPointSet(producer);
    if (refPointSet.empty()) {
        refPointSet.emplace(seqId, pointedTo);
    } else {
        auto currentTopRef = refPointSet.rbegin()->second;
        if (refPointSet.count(seqId)) {
            if (refPointSet.at(seqId) >= pointedTo) return;
            else refPointSet.erase(seqId);
        }
        auto [it, b] = refPointSet.emplace(seqId, pointedTo);
        assert(b);
        {
            auto it2 = it;
            if (it != refPointSet.begin() && (--it2)->second > pointedTo) {
                refPointSet.erase(it);
                return;
            }
        }
        while (it != refPointSet.end()) {
            auto it2 = it;
            it2 ++;
            if (it2 != refPointSet.end() && it2->second <= pointedTo) {
                refPointSet.erase(it2);
            } else break;
        }
        if (currentTopRef >= pointedTo) return;
        auto location_it = m_locations.find(currentTopRef);
        assert(location_it->second.erase(producer));
        if (location_it->second.empty()) {
            m_locations.erase(location_it);
        }
    }

    m_locations[pointedTo].insert(producer);
    auto locationSize = getLocationSize();
    if (locationSize < m_maxReference) return;
    if (locationSize - m_locations.begin()->second.size() >= m_maxReference) {
        for (const auto& n : m_locations.begin()->second) {
            m_referencePoints.erase(n);
        }
        m_locations.erase(m_locations.begin());
    }
}

std::map<uint64_t, uint64_t> &mnemosyne::dag::ReplicationCounter::getPrunedRefPointSet(const Name &producer) {
    auto& refPointSet = m_referencePoints[producer];
    if (!m_locations.empty()) {
        while(!refPointSet.empty() && refPointSet.begin()->second < m_locations.begin()->first) {
            refPointSet.erase(refPointSet.begin());
        }
    }
    return refPointSet;
}

uint32_t mnemosyne::dag::ReplicationCounter::getLocationSize() const {
    uint64_t s = 0;
    for (const auto& i: m_locations) {
        s += i.second.size();
    }
    return (uint32_t) s;
}

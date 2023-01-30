//
// Created by Tyler on 1/29/23.
//

#include "seq-no-recovery.h"

#include <ndn-cxx/encoding/block-helpers.hpp>


namespace mnemosyne {

const SeqNoRecovery::SegmentAccumulator SeqNoRecovery::EMPTY_ACCUMULATOR;

void SeqNoRecovery::SegmentAccumulator::add(uint64_t val) {
    /*if (right_it == m_segments.end()) {
        printf("No Right\n");
    } else printf("Right %llu %llu\n", right_it->first, right_it->second);*/
    auto left_it = m_segments.lower_bound(val);
    if (left_it != m_segments.end() && left_it->first == val) return;
    bool can_merge_left = false;
    if (left_it != m_segments.begin()) {
        left_it--;
        if (left_it->second >= val) return;
        can_merge_left = left_it->second == val - 1;
    } else {
        // should delete left_it
    }
    auto right_it = m_segments.upper_bound(val);
    bool can_merge_right = right_it != m_segments.end() && right_it->first == val + 1;
    if (can_merge_left) {
        if (can_merge_right) {
            left_it->second = right_it->second;
            m_segments.erase(right_it);
        } else {
            left_it->second = val; // merge into left
        }
    } else {
        if (can_merge_right) {
            auto right_val = right_it->second;
            m_segments.erase(right_it);
            m_segments.emplace(val, right_val);
        } else {
            m_segments.emplace(val, val);
        }
    }
}

bool SeqNoRecovery::SegmentAccumulator::isIn(uint64_t val) const {
    auto it = m_segments.lower_bound(val);
    if (it != m_segments.end() && it->first == val) return true;
    if (it == m_segments.begin()) return false;
    it --;
    return it->second >= val;
}

ndn::Block SeqNoRecovery::SegmentAccumulator::encode() const {
    ndn::Block block(ACCUMULATOR_TLV_TYPE);
    for (const auto& r: m_segments) {
        block.push_back(ndn::encoding::makeNonNegativeIntegerBlock(SEGMENT_END_TLV_TYPE, r.first));
        block.push_back(ndn::encoding::makeNonNegativeIntegerBlock(SEGMENT_END_TLV_TYPE, r.second));
    }
    return block;
}

bool SeqNoRecovery::SegmentAccumulator::decode(const ndn::Block& block) {
    m_segments.clear();
    if (block.type() != ACCUMULATOR_TLV_TYPE) return false;
    block.parse();
    try {
        for (auto it = block.elements_begin(); it != block.elements_end(); it++) {
            if (it->type() != SEGMENT_END_TLV_TYPE) return false;
            auto val1 = ndn::readNonNegativeInteger(*it);
            it++;
            if (it == block.elements_end() || it->type() != SEGMENT_END_TLV_TYPE) return false;
            m_segments.emplace(val1, ndn::readNonNegativeInteger(*it));
        }
    } catch (const ndn::tlv::Error& e) {
        return false;
    }
    return true;
}

ndn::Block SeqNoRecovery::encode() const {
    ndn::Block block(RECOVERY_TLV_TYPE);
    for (const auto& [group, streams]: m_states) {
        ndn::Block groupBlock(group);
        for (const auto& [name, segment]: streams) {
            groupBlock.push_back(name.wireEncode());
            groupBlock.push_back(segment.encode());
        }
    }
    return block;
}

bool SeqNoRecovery::decode(const ndn::Block& block) {
    m_states.clear();
    if (block.type() != RECOVERY_TLV_TYPE) return false;
    block.parse();
    for (const auto& groupBlock : block.elements()) {
        groupBlock.parse();
        try {
            for (auto it = groupBlock.elements_begin(); it != groupBlock.elements_end(); it++) {
                if (it->type() != ndn::tlv::Name) return false;
                auto val1 = ndn::Name(*it);
                it++;
                if (it == block.elements_end() || it->type() != SegmentAccumulator::ACCUMULATOR_TLV_TYPE) return false;
                SegmentAccumulator acc;
                if (!acc.decode(*it)) return false;
                m_states[groupBlock.type()].emplace(val1, std::move(acc));
            }
        } catch (const ndn::tlv::Error& e) {
            return false;
        }
    }
    return true;
}
}

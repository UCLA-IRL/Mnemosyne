//
// Created by Tyler on 1/29/23.
//

#ifndef MNEMOSYNE_SEQ_NO_RECOVERY_H
#define MNEMOSYNE_SEQ_NO_RECOVERY_H

#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/name.hpp>

#include <map>
#include <unordered_map>

namespace mnemosyne {
class SeqNoRecovery {
  public:
    class SegmentAccumulator {
      public:
        void add(uint64_t val);

        bool isIn(uint64_t val) const;

        std::optional<uint64_t> lastContinuous(uint64_t start) const;

        ndn::Block encode() const;

        bool decode(const ndn::Block& block);

      private:
        std::map<uint64_t, uint64_t> m_segments;
      public:
        static const uint8_t SEGMENT_END_TLV_TYPE = 157;
        static const uint8_t ACCUMULATOR_TLV_TYPE = 159;
    };

    SegmentAccumulator& getStream(uint32_t group, const ndn::Name& producer) {
        return m_states[group][producer];
    }

    [[nodiscard]] const SegmentAccumulator& getStream(uint32_t group, const ndn::Name& producer) const {
        auto it = m_states.find(group);
        if (it == m_states.end()) {
            return EMPTY_ACCUMULATOR;
        }
        auto it2 = it->second.find(producer);
        if (it2 == it->second.end()) {
            return EMPTY_ACCUMULATOR;
        }
        return it2->second;
    }

    ndn::Block encode() const;

    bool decode(const ndn::Block& block);

  private:
    std::unordered_map<uint32_t, std::unordered_map<ndn::Name, SegmentAccumulator>> m_states;

    static const SegmentAccumulator EMPTY_ACCUMULATOR;
  public:
    static const uint32_t RECOVERY_TLV_TYPE = 161;
};
} //namespace mnemosyne

#endif //MNEMOSYNE_SEQ_NO_RECOVERY_H

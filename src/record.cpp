#include "mnemosyne/record.hpp"

#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <sstream>
#include <utility>
#include <iostream>

namespace mnemosyne {

bool Record::isRecordName(const Name &recordName) {
    int isFullName = !recordName.empty() && recordName.get(-1).isImplicitSha256Digest();
    if (recordName.size() < 2 + isFullName) return false;
    if (!recordName.get(-1 - isFullName).isNumber()) return false;
    auto s = readString(recordName.get(-2 - isFullName));
    return s == "RECORD";
}

bool Record::isGenesisRecord(const Name &recordName) {
    if (!isRecordName(recordName))
        NDN_THROW(std::runtime_error("Bad record name at isGenesisRecord: " + recordName.toUri()));
    int isFullName = !recordName.empty() && recordName.get(-1).isImplicitSha256Digest();
    return recordName.get(-1 - isFullName).toNumber() == 0;
}

Name Record::getProducerPrefix(const Name &recordName) {
    if (!isRecordName(recordName))
        NDN_THROW(std::runtime_error("Bad record name at getProducerPrefix: " + recordName.toUri()));
    int isFullName = !recordName.empty() && recordName.get(-1).isImplicitSha256Digest();
    return recordName.getPrefix(-2 - isFullName);
}

uint64_t Record::getRecordSeqId(const Name &recordName) {
    if (!isRecordName(recordName))
        NDN_THROW(std::runtime_error("Bad record name at getRecordSeqId: " + recordName.toUri()));
    int isFullName = !recordName.empty() && recordName.get(-1).isImplicitSha256Digest();
    return recordName.get(-1 - isFullName).toNumber();
}

Name Record::getRecordName(Name producerName, const uint64_t seq_id) {
    return std::move(producerName).append("RECORD").appendNumber(seq_id);
}

Name Record::getGenesisRecordFullName(const Name &recordName) {
    Data d(recordName);
    static ndn::KeyChain keychain;
    keychain.sign(d, security::signingWithSha256());
    return d.getFullName();
}

Record::Record(const Data &eventItem, Name eventProducer, uint64_t seqId)
        : m_data(nullptr) {
    setContentData(eventItem);
}

Record::Record(const std::shared_ptr<const Data> &data)
        : m_data(data) {
    if (!isRecordName(data->getName()) || isGenesisRecord(data->getName()))
        NDN_THROW(std::runtime_error("Bad record name"));
    headerWireDecode(m_data->getContent());
    bodyWireDecode(m_data->getContent());
}

Record::Record(ndn::Data data)
        : Record(std::make_shared<ndn::Data>(std::move(data))) {
}

Name
Record::getRecordFullName() const {
    if (m_data != nullptr)
        return m_data->getFullName();
    return Name();
}

const std::list<Name> &
Record::getPointersFromHeader() const {
    return m_recordPointers;
}

void Record::setContentData(const Data &contentItem) {
    if (m_data != nullptr) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Cannot modify built record"));
    }
    m_contentData = contentItem;
}

const optional<Data> &
Record::getContentData() const {
    return m_contentData;
}

bool
Record::isEmpty() const {
    return m_data == nullptr && m_recordPointers.empty() && !m_contentData.has_value();
}

void
Record::addPointer(const Name &pointer) {
    if (m_data != nullptr) {
        BOOST_THROW_EXCEPTION(std::runtime_error("Cannot modify built record"));
    }
    m_recordPointers.push_back(pointer);
}

void
Record::wireEncode(Block &block) const {
    headerWireEncode(block);
    bodyWireEncode(block);
}

void
Record::headerWireEncode(Block &block) const {
    auto header = makeEmptyBlock(T_RecordHeader);
    for (const auto &pointer: m_recordPointers) {
        header.push_back(pointer.wireEncode());
    }
    header.parse();
    block.push_back(header);
    block.parse();
}

void
Record::headerWireDecode(const Block &dataContent) {
    m_recordPointers.clear();
    dataContent.parse();
    const auto &headerBlock = dataContent.get(T_RecordHeader);
    headerBlock.parse();
    Name pointer;
    for (const auto &item: headerBlock.elements()) {
        if (item.type() == tlv::Name) {
            try {
                pointer.wireDecode(item);
            } catch (const tlv::Error &e) {
                std::cout << (e.what());
            }

            m_recordPointers.push_back(pointer);
        } else {
            BOOST_THROW_EXCEPTION(std::runtime_error("Bad header item type"));
        }
    }
}

void
Record::bodyWireEncode(Block &block) const {
    auto body = makeEmptyBlock(T_RecordContent);
    if (m_contentData.has_value())
        body.push_back(m_contentData->wireEncode());
    body.parse();
    block.push_back(body);
    block.parse();
};

void
Record::bodyWireDecode(const Block &dataContent) {
    dataContent.parse();
    const auto &contentBlock = dataContent.get(T_RecordContent);
    m_contentData = Data(contentBlock.blockFromValue());
}

void
Record::checkPointerCount(uint32_t numPointers) const {
    if (getPointersFromHeader().size() < numPointers) {
        throw std::runtime_error("Less preceding record than expected");
    }

    std::set < Name > nameSet;
    for (const auto &pointer: getPointersFromHeader()) {
        nameSet.insert(getProducerPrefix(pointer));
    }
    if (nameSet.size() != numPointers) {
        throw std::runtime_error("Repeated preceding Records");
    }
}

}  // namespace mnemosyne
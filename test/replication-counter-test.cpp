#include "dag-sync/replication-counter.h"
#include <ndn-cxx/name.hpp>
#include <iostream>

using namespace mnemosyne;
using namespace ndn;

Record
makeRecord(const ndn::Name &producer, const ndn::Name &refTo, uint64_t seqId) {
    Record record(Data(), producer);
    record.addPointer(Record::getRecordName(refTo, seqId));
    auto data = make_shared<Data>(Record::getRecordName(producer, 1));
    data->setSignatureInfo(SignatureInfo(tlv::SignatureSha256WithRsa));
    data->setSignatureValue(ndn::encoding::makeEmptyBlock(tlv::SignatureValue).getBuffer());
    data->wireEncode();
    record.setEncodedData(data);
    return record;
}

void printIntList(const std::list<uint64_t>& l) {
    std::cout << "[";
    for (const auto& i : l) {
        std::cout << " " << i;
    }
    std::cout << " ]\n";
}

bool testProducerRef() {
    dag::ReplicationCounter counter("/a", 3);
    if (counter.getCounts().size() != 0) return false;
    counter.recordUpdate(makeRecord("/a", "/a", 1));
    if (counter.getCounts().size() != 0) return false;
    counter.recordUpdate(makeRecord("/b", "/a", 1));
    if (counter.getCounts().size() != 1) return false;
    counter.recordUpdate(makeRecord("/c", "/a", 2));
    if (counter.getCounts().size() != 2) return false;
    counter.recordUpdate(makeRecord("/b", "/a", 3));
    if (counter.getCounts().size() != 2) return false;
    counter.recordUpdate(makeRecord("/d", "/a", 2));
    if (counter.getCounts().size() != 3) return false;
    counter.recordUpdate(makeRecord("/e", "/a", 4));
    if (counter.getCounts().size() != 3) return false;
    counter.recordUpdate(makeRecord("/d", "/a", 5));
    if (counter.getCounts().size() != 3) return false;
    return true;
}

bool testIndirectRef() {
    dag::ReplicationCounter counter("/a", 3);
    if (counter.getCounts().size() != 0) return false;
    counter.recordUpdate(makeRecord("/b", "/a", 1));
    if (counter.getCounts().size() != 1) return false;
    counter.recordUpdate(makeRecord("/c", "/b", 1));
    if (counter.getCounts().size() != 2) return false;
    counter.recordUpdate(makeRecord("/b", "/a", 3));
    if (counter.getCounts().size() != 2) return false;
    counter.recordUpdate(makeRecord("/d", "/a", 2));
    if (counter.getCounts().size() != 3) return false;
    counter.recordUpdate(makeRecord("/e", "/b", 1));
    if (counter.getCounts().size() != 3) return false;
    if (counter.getMaxReferenceSeqNo() != 2) return false;
    counter.recordUpdate(makeRecord("/d", "/b", 1));
    if (counter.getCounts().size() != 3) return false;
    if (counter.getMaxReferenceSeqNo() != 3) return false;
    return true;
}

#define TEST(testName) { auto success = testName(); \
    if (!success) { \
    std::cout << #testName" failed" << std::endl; \
    } else { \
    std::cout << #testName" with no errors" << std::endl; \
    } \
}

int
main(int argc, char **argv) {
    TEST(testProducerRef);
    TEST(testIndirectRef);
    return 0;
}
#include "mnemosyne/mnemosyne-dag-logger.hpp"

#include "backend/backend.h"
#include "dag-sync/dag-reference-checker.h"
#include "dag-sync/replication-counter.h"
#include "dag-sync/record-sync.h"
#include "util.hpp"

#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/time.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <algorithm>
#include <random>
#include <sstream>

NDN_LOG_INIT(mnemosyne.dagsync.impl);

using namespace ndn;
namespace mnemosyne {

MnemosyneDagLogger::MnemosyneDagLogger(const Config &config,
                                       security::KeyChain &keychain,
                                       Face &network,
                                       std::shared_ptr<ndn::security::Validator> recordValidator)
        : m_config(config)
        , m_backend(std::make_shared<Backend>(config.databaseType, config.databasePath, m_config.seqNoBackupFreq))
        , m_dagReferenceChecker(std::make_unique<DagReferenceChecker>(m_backend,
                                std::bind(&MnemosyneDagLogger::addReceivedRecord, this, _1, _2, _3)))
        , m_replicationCounter(std::make_unique<dag::ReplicationCounter>(config.peerPrefix, config.maxReplicationCount))
        , m_dagSync(make_unique<dag::RecordSync>(config.syncPrefix, config.peerPrefix, config.hintPrefix, network,
                    [&](const auto& i){onUpdate(i);},
                    m_backend,
                    getSecurityOption(keychain, recordValidator, config.peerPrefix)))
        , m_randomEngine(std::random_device()())
        , m_KnownSelfSeqId(0)
{
    NDN_LOG_INFO("Mnemosyne Initialization Start");

    if (config.precedingRecordNum <= 1) {
        NDN_THROW(std::runtime_error("Bad config"));
    }

    restoreRecordSyncVersionVector();

    if (m_lastRecordInChains.empty()) {
        addPublicGenesisRecord();
    }

    if (!m_lastRecordInChains.count(m_config.peerPrefix)) {
        m_lastRecordInChains[m_config.peerPrefix] = Record::getGenesisRecordFullName(Record::getRecordName(m_config.peerPrefix, 0));
    }

    NDN_LOG_INFO("Mnemosyne Initialization Succeed");
}

void MnemosyneDagLogger::restoreRecordSyncVersionVector() {
    auto restored_vv = m_backend->seqNumGet(Backend::DAG_SYNC_GROUP);
    if (restored_vv.get(m_config.peerPrefix) == 0)
        restored_vv.set(m_config.peerPrefix, 0);
    for (const auto& [producer, s] : restored_vv) {
        auto seq = s;
        auto listed = m_backend->listRecord(Record::getRecordName(producer, seq), 1);
        if (producer != m_config.peerPrefix && (listed.empty() || !producer.isPrefixOf(*listed.begin()))) {
            NDN_LOG_FATAL("Failed to restore sequenced record");
            exit(1);
        }
        while (true) {
            auto l = m_backend->listRecord(Record::getRecordName(producer, seq + 1), 1);
            if (l.empty()) {
                break;
            }
            else {
                seq ++;
                m_lastRecordInChains[producer] = *l.begin();
            }
        }
        m_dagSync->getCore().updateSeqNo(seq, producer);
        if (seq != s) m_backend->seqNumSet(Backend::DAG_SYNC_GROUP, producer, seq);
        restored_vv.set(producer, seq);

        if (producer == m_config.peerPrefix) {
            m_KnownSelfSeqId = seq;
        }
    }
    NDN_LOG_INFO("STEP 1: attempted restoring sequence id to " << restored_vv.toStr() << " in the Mnemosyne Dag Sync");
}

void MnemosyneDagLogger::addPublicGenesisRecord() {
    //****STEP 2****
    // Make the public genesis data
    int i = 0;
    while (m_lastRecordInChains.size() < m_config.precedingRecordNum - 1) {
        Name tempProducer = Name().appendNumber(i++);
        if (m_lastRecordInChains.count(tempProducer)) continue;
        m_lastRecordInChains.emplace(tempProducer, Record::getGenesisRecordFullName(Record::getRecordName(
                        tempProducer, 0)));
    }
    NDN_LOG_INFO( i << " genesis records have been added to the Mnemosyne");
}

MnemosyneDagLogger::~MnemosyneDagLogger() = default;

ReturnCode MnemosyneDagLogger::createRecord(Record &record) {
    NDN_LOG_INFO("[MnemosyneDagLogger::createRecord] Add new record");

    if (Record::getRecordSeqId(m_lastRecordInChains.at(m_config.peerPrefix)) < m_KnownSelfSeqId) {
        NDN_LOG_WARN("[MnemosyneDagLogger::createRecord] waiting for record discovery: " << m_KnownSelfSeqId);
        return ReturnCode::timingError("Waiting for self record recovery");
    }
    if (m_lastRecordInChains.size() < m_config.precedingRecordNum) {
        NDN_LOG_WARN("[MnemosyneDagLogger::createRecord] Not Enough Tailing Record: " << m_lastRecordInChains.size() << " < " << m_config.precedingRecordNum);
        return ReturnCode::notEnoughTailingRecord();
    }

    record.addPointer(m_lastRecordInChains.at(m_config.peerPrefix));
    m_lastRecordInChains.erase(m_config.peerPrefix);

    // randomly shuffle the tailing record list
    std::vector<std::pair<Name, Name>> recordList;
    std::sample(m_lastRecordInChains.begin(), m_lastRecordInChains.end(),
                std::back_inserter(recordList), m_config.precedingRecordNum - 1, m_randomEngine);

    for (const auto &[p, tailRecord] : recordList) {
        record.addPointer(tailRecord);
        m_lastRecordInChains.erase(Record::getProducerPrefix(tailRecord));
        if (record.getPointersFromHeader().size() >= m_config.precedingRecordNum)
            break;
    }

    //send sync interest
    auto seqId = m_dagSync->publishData(record, time::minutes(5), m_config.peerPrefix, tlv::Data);
    NDN_LOG_INFO("[MnemosyneDagLogger::createRecord] Added a new record:" << record.getRecordFullName().toUri());
    // add new record into the ledger
    addReceivedRecord(std::make_unique<Record>(record), m_config.peerPrefix, seqId);
    return ReturnCode::noError(record.getRecordFullName().toUri());
}

optional<Record> MnemosyneDagLogger::getRecord(const std::string &recordName) const {
    return m_backend->getRecord(recordName);
}

bool MnemosyneDagLogger::hasRecord(const std::string &recordName) const {
    auto list = m_backend->listRecord(Name(recordName), 1);
    if (list.empty()) return false;
    return *list.begin() == recordName;
}

std::list<Name> MnemosyneDagLogger::listRecord(const std::string &prefix) const {
    return m_backend->listRecord(Name(prefix));
}

std::list<uint64_t> MnemosyneDagLogger::getReplicationSeqId() const {
    auto re = m_replicationCounter->getCounts();
    re.push_back(m_KnownSelfSeqId);
    return re;
}

void MnemosyneDagLogger::onUpdate(const std::vector<ndn::svs::MissingDataInfo>& info) {
    for (const auto& stream : info) {
        std::cerr << "Missing Data " << stream.nodeId << " " << stream.low << " " << stream.high << "\n";
        if (stream.nodeId == m_config.peerPrefix) {
            m_KnownSelfSeqId = std::max(m_KnownSelfSeqId, stream.high);
        }
        auto lastNo = m_backend->seqNumGet(Backend::DAG_SYNC_GROUP).get(stream.nodeId);
        if (lastNo >= stream.low) {
            NDN_LOG_INFO("Skipped in-backend item " << stream.nodeId << " " << stream.low);
        } else {
            lastNo = stream.low;
        }

        for (svs::SeqNo i = lastNo; i <= stream.high; i++) {
            NDN_LOG_INFO("Fetching item " << stream.nodeId << " " << i);
            m_dagSync->fetchRecord(stream.nodeId, i, [nodeId=stream.nodeId, i, this](const Data& data){
                auto receivedData = std::make_shared<Data>(data);
                try {
                    auto receivedRecord = make_unique<Record>(receivedData);
                    receivedRecord->checkPointerCount(m_config.precedingRecordNum);
                    m_dagReferenceChecker->addRecord(std::move(receivedRecord), nodeId, i);
                } catch (const std::exception& e) {
                    NDN_LOG_ERROR("bad record received" << receivedData->getFullName() << ": " << e.what());
                }
            }, stream.nodeId == m_config.peerPrefix? 0 : m_config.recordFetchRetries,
                       m_config.hintedFetchRetries,
                       [](const Data& data, const ndn::security::ValidationError& error){
                           NDN_LOG_ERROR("Verification error on Received record " << data.getFullName() << ": " << error.getInfo());
                       }, [nodeId = stream.nodeId, i](auto& ...){
                        NDN_LOG_ERROR("Fetch timeout on Received record " << nodeId << " - Sequence Id " << i);
            });
        }
    }
}

void MnemosyneDagLogger::addReceivedRecord(std::unique_ptr<Record> record, const Name& producer, svs::SeqNo seqId) {
    NDN_LOG_INFO("Add record to ledger: " << record->getRecordFullName());
    const shared_ptr<const Data>& recordData = record->getEncodedData();

    //backend update
    if (m_backend->seqNumGet(Backend::DAG_SYNC_GROUP).get(producer) + 1 != seqId) {
        NDN_LOG_WARN(" - previous version does not have continuous version vector with " << record->getRecordFullName());
    }
    m_backend->seqNumSet(Backend::DAG_SYNC_GROUP, producer, seqId);
    m_backend->putRecord(recordData);

    //local update
    m_lastRecordInChains[Record::getProducerPrefix(recordData->getName())] = recordData->getFullName();
    if (producer == m_config.peerPrefix) {
        m_KnownSelfSeqId = std::max(m_KnownSelfSeqId, Record::getRecordSeqId(record->getRecordFullName()));
    } else {
        m_replicationCounter->recordUpdate(*record);
        if (m_onRecordCallback) {
            m_onRecordCallback(*record);
        }
    }
}

const Name &MnemosyneDagLogger::getPeerPrefix() const {
    return m_config.peerPrefix;
}

ndn::svs::SecurityOptions MnemosyneDagLogger::getSecurityOption(KeyChain& keychain, shared_ptr<ndn::security::Validator> recordValidator, Name peerPrefix) {
    ndn::svs::SecurityOptions option(keychain);
    option.validator = make_shared<::util::cxxValidator>(recordValidator);
    option.encapsulatedDataValidator = make_shared<::util::alwaysFailValidator>();
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(keychain, security::signingByIdentity(peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = std::make_shared<ndn::svs::BaseSigner>();
    return option;
}

}  // namespace mnemosyne
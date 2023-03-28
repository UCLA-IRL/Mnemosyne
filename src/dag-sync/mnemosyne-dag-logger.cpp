#include "mnemosyne/mnemosyne-dag-logger.hpp"

#include "dag-sync/dag-reference-checker.h"
#include "dag-sync/replication-counter.h"
#include "dag-sync/record-sync.h"
#include "util.hpp"

#include <ndn-cxx/encoding/block-helpers.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <utility>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <algorithm>
#include <random>
#include <sstream>

NDN_LOG_INIT(mnemosyne.dagsync.impl);

using namespace ndn;
namespace mnemosyne {

const std::string MnemosyneDagLogger::SEQ_NO_BACKUP_KEY = "SeqNoBackup";

MnemosyneDagLogger::MnemosyneDagLogger(const LoggerConfig &config,
                                       security::KeyChain &keychain,
                                       Face &network,
                                       std::shared_ptr<ndn::security::Validator> recordValidator,
                                       std::function<void(const Record &)> onRecordCallback)
        : m_config(config),
          m_backend(std::make_shared<Backend>(config.databaseType, config.databasePath, m_config.seqNoBackupFreq)),
          m_dagReferenceChecker(std::make_unique<DagReferenceChecker>(m_backend,
                                                                      std::bind(&MnemosyneDagLogger::addReceivedRecord,
                                                                                this, _1, _2, _3))),
          m_replicationCounter(
                  std::make_unique<dag::ReplicationCounter>(config.peerPrefix, config.maxCountedReplication)),
          m_dagSync(make_unique<dag::RecordSync>(config.syncPrefix, config.peerPrefix, config.hintPrefix, network,
                                                 [&](const auto &i) { onUpdate(i); },
                                                 m_backend,
                                                 getSecurityOption(keychain, recordValidator, config.peerPrefix))),
          m_randomEngine(std::random_device()()), m_KnownSelfSeqId(0), m_onRecordCallback(onRecordCallback) {
    NDN_LOG_DEBUG("Mnemosyne Initialization Start");

    if (config.precedingRecordNum <= 1) {
        NDN_THROW(std::runtime_error("Bad config"));
    }

    restoreRecordSyncVersionVector();

    if (m_lastRecordInChains.empty()) {
        addPublicGenesisRecord();
    }

    if (!m_lastRecordInChains.count(m_config.peerPrefix)) {
        m_lastRecordInChains[m_config.peerPrefix] = std::make_pair(Record::getGenesisRecordFullName(
                Record::getRecordName(m_config.peerPrefix, 0)), m_config.maxSelfReRefCount);
    }

    NDN_LOG_DEBUG("Mnemosyne Dag Logger Initialization Succeed");
}

void MnemosyneDagLogger::restoreRecordSyncVersionVector() {
    //attempt recovery
    auto page = m_backend->getMetaData(SEQ_NO_BACKUP_KEY);
    if (page) {
        try {
            ndn::Block block(make_span(reinterpret_cast<const uint8_t *>(page->data()), page->size()));
            m_dagCollectedVersions = svs::VersionVector(block);
            NDN_LOG_DEBUG("Version vector recovery success");
        } catch (const std::exception &e) {
            NDN_LOG_DEBUG("Version vector recovery failed with exception: " << e.what());
            exit(1);
        }
    }

    if (m_dagCollectedVersions.get(m_config.peerPrefix) == 0)
        m_dagCollectedVersions.set(m_config.peerPrefix, 0);
    for (const auto &[producer, s]: m_dagCollectedVersions) {
        auto seq = s;
        auto listed = m_backend->listRecord(Record::getRecordName(producer, seq), 1);
        if (producer != m_config.peerPrefix && listed.empty()) {
            NDN_LOG_FATAL("Failed to restore sequenced record");
            exit(1);
        }
        while (true) {
            auto l = m_backend->listRecord(Record::getRecordName(producer, seq + 1), 1);
            if (l.empty()) {
                break;
            } else {
                if (producer != m_config.peerPrefix && m_onRecordCallback) {
                    m_onRecordCallback(m_backend->getRecord(*l.begin()));
                }
                seq++;
                m_lastRecordInChains[producer] = std::make_pair(*l.begin(), m_config.maxSelfReRefCount);
            }
        }
        m_dagSync->getCore().updateSeqNo(seq, producer);
        m_dagCollectedVersions.set(producer, seq);

        if (producer == m_config.peerPrefix) {
            m_KnownSelfSeqId = seq;
        }
    }
    NDN_LOG_DEBUG("STEP 1: attempted restoring sequence id to " << m_dagCollectedVersions.toStr()
                                                               << " in the Mnemosyne Dag Sync");
    m_backend->addBackupCallback([this]() { return versionBackupCallback(); });
}

void MnemosyneDagLogger::addPublicGenesisRecord() {
    //****STEP 2****
    // Make the public genesis data
    int i = 0;
    while (m_lastRecordInChains.size() < m_config.precedingRecordNum - 1) {
        Name tempProducer = Name().appendNumber(i++);
        if (m_lastRecordInChains.count(tempProducer)) continue;
        m_lastRecordInChains.emplace(tempProducer, std::make_pair(Record::getGenesisRecordFullName(Record::getRecordName(
                tempProducer, 0)), 1));
    }
    NDN_LOG_DEBUG(" - " << i << " genesis records have been added to the Mnemosyne");
}

MnemosyneDagLogger::~MnemosyneDagLogger() = default;

ReturnCode MnemosyneDagLogger::createRecord(Record &record) {
    NDN_LOG_DEBUG("[MnemosyneDagLogger::createRecord] create record called");

    if (Record::getRecordSeqId(m_lastRecordInChains.at(m_config.peerPrefix).first) < m_KnownSelfSeqId) {
        NDN_LOG_WARN("[MnemosyneDagLogger::createRecord] waiting for record discovery: " << m_KnownSelfSeqId);
        return ReturnCode::timingError("Waiting for self record recovery");
    }
    if (m_lastRecordInChains.size() < m_config.precedingRecordNum) {
        NDN_LOG_WARN(
                "[MnemosyneDagLogger::createRecord] Not Enough Tailing Record: " << m_lastRecordInChains.size() << " < "
                                                                                 << m_config.precedingRecordNum);
        return ReturnCode::notEnoughTailingRecord();
    }

    selectAndAddPrecedingRecords(record);

    //send sync interest
    auto seqId = m_dagSync->publishData(record, time::minutes(5), m_config.peerPrefix, tlv::Data);
    NDN_LOG_DEBUG("[MnemosyneDagLogger::createRecord] Added a new record:" << record.getRecordFullName().toUri());
    // add new record into the ledger
    addReceivedRecord(std::make_unique<Record>(record), m_config.peerPrefix, seqId);
    return ReturnCode::noError(record.getRecordFullName().toUri());
}

void MnemosyneDagLogger::selectAndAddPrecedingRecords(Record &record) {
    record.addPointer(m_lastRecordInChains.at(m_config.peerPrefix).first);
    m_lastRecordInChains.erase(m_config.peerPrefix);

    // randomly shuffle the tailing record list
    std::vector<uint32_t> refCountInLastRecord(m_config.maxSelfReRefCount + 1, 0);
    std::vector<std::pair<Name, uint32_t>> candidateList;
    for (const auto& i: m_lastRecordInChains) {
        refCountInLastRecord.at(i.second.second) ++;
        candidateList.push_back(i.second);
    }
    uint32_t neededCounts = m_config.maxSelfReRefCount + 1;
    uint32_t candidates = 0;
    while (candidates < m_config.precedingRecordNum - 1) {
        assert(neededCounts > 0);
        neededCounts --;
        candidates += refCountInLastRecord.at(neededCounts);
    }
    candidateList.erase(std::remove_if(candidateList.begin(), candidateList.end(),
                                       [neededCounts](const auto& i){return i.second < neededCounts; }),
                        candidateList.end());
    assert(candidateList.size() == candidates);

    std::vector<std::pair<Name, uint32_t>> recordList;
    std::sample(candidateList.begin(), candidateList.end(),
                std::back_inserter(recordList), m_config.precedingRecordNum - 1, m_randomEngine);

    for (const auto &[name, count]: recordList) {
        record.addPointer(name);
        auto it = m_lastRecordInChains.find(Record::getProducerPrefix(name));
        assert(it != m_lastRecordInChains.end());
        if (count == 1)
            m_lastRecordInChains.erase(it);
        else it->second.second -= 1;
        if (record.getPointersFromHeader().size() >= m_config.precedingRecordNum)
            break;
    }
}

std::list<uint64_t> MnemosyneDagLogger::getReplicationSeqId() const {
    auto re = m_replicationCounter->getCounts();
    re.push_back(m_KnownSelfSeqId);
    return re;
}

uint64_t MnemosyneDagLogger::getMaxReferenceSeqNo() const {
    return m_replicationCounter->getMaxReferenceSeqNo();
}

void MnemosyneDagLogger::onUpdate(const std::vector<ndn::svs::MissingDataInfo> &info) {
    for (const auto &stream: info) {
        NDN_LOG_DEBUG("Sync discovered Data " << stream.nodeId << " " << stream.low << " - " << stream.high);
        if (stream.nodeId == m_config.peerPrefix) {
            m_KnownSelfSeqId = std::max(m_KnownSelfSeqId, stream.high);
        }
        auto lastNo = m_dagCollectedVersions.get(stream.nodeId);
        if (lastNo >= stream.low) {
            NDN_LOG_DEBUG("Skipped in-backend item " << stream.nodeId << " " << stream.low);
        } else {
            lastNo = stream.low;
        }

        for (svs::SeqNo i = lastNo; i <= stream.high; i++) {
            NDN_LOG_DEBUG("Fetching item " << stream.nodeId << " " << i);
            m_dagSync->fetchRecord(stream.nodeId, i, [nodeId = stream.nodeId, i, this](const Data &data) {
                                       auto receivedData = std::make_shared<Data>(data);
                                       try {
                                           auto receivedRecord = make_unique<Record>(receivedData);
                                           receivedRecord->checkPointerCount(m_config.precedingRecordNum);
                                           m_dagReferenceChecker->addRecord(std::move(receivedRecord), nodeId, i);
                                       } catch (const std::exception &e) {
                                           NDN_LOG_ERROR("bad record received" << receivedData->getFullName() << ": " << e.what());
                                       }
                                   }, stream.nodeId == m_config.peerPrefix ? 0 : m_config.recordFetchRetries,
                                   m_config.hintedFetchRetries,
                                   [](const Data &data, const ndn::security::ValidationError &error) {
                                       NDN_LOG_ERROR(
                                               "Verification error on Received record " << data.getFullName() << ": "
                                                                                        << error.getInfo());
                                   }, [nodeId = stream.nodeId, i](auto &...) {
                        NDN_LOG_ERROR("Fetch timeout on Received record " << nodeId << " - Sequence Id " << i);
                    });
        }
    }
}

void MnemosyneDagLogger::addReceivedRecord(std::unique_ptr<Record> record, const Name &producer, svs::SeqNo seqId) {
    NDN_LOG_DEBUG("Add record to ledger: " << record->getRecordFullName());
    const shared_ptr<const Data> &recordData = record->getEncodedData();

    //backend update
    if (m_dagCollectedVersions.get(producer) + 1 != seqId) {
        NDN_LOG_WARN(
                " - previous version does not have continuous version vector with " << record->getRecordFullName());
    }
    m_dagCollectedVersions.set(producer, seqId);
    m_backend->triggerBackup();
    m_backend->putRecord(recordData);

    //local update
    m_lastRecordInChains[Record::getProducerPrefix(recordData->getName())] = std::make_pair(recordData->getFullName(), m_config.maxSelfReRefCount);
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

bool MnemosyneDagLogger::versionBackupCallback() {
    auto backupPage = m_dagCollectedVersions.encode();
    backupPage.encode();
    std::string page((const char *) backupPage.wire(), backupPage.size());
    if (m_backend->placeMetaData(SEQ_NO_BACKUP_KEY, page)) {
        NDN_LOG_DEBUG("Version Vector backup write success");
        return true;
    } else {
        return false;
    }
}

ndn::svs::SecurityOptions
MnemosyneDagLogger::getSecurityOption(KeyChain &keychain, shared_ptr<ndn::security::Validator> recordValidator,
                                      Name peerPrefix) {
    ndn::svs::SecurityOptions option(keychain);
    option.validator = recordValidator ? make_shared<::util::cxxValidator>(recordValidator) : nullptr;
    option.encapsulatedDataValidator = make_shared<::util::alwaysFailValidator>();
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(keychain,
                                                                       security::signingByIdentity(peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = std::make_shared<ndn::svs::BaseSigner>();
    return option;
}

}  // namespace mnemosyne
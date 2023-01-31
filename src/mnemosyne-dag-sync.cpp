#include "mnemosyne/mnemosyne-dag-sync.hpp"

#include "backend/backend.h"
#include "dag-reference-checker.h"
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

MnemosyneDagSync::MnemosyneDagSync(const Config &config,
                     security::KeyChain &keychain,
                     Face &network,
                     std::shared_ptr<ndn::security::Validator> recordValidator)
        : m_config(config)
        , m_keychain(keychain)
        , m_backend(std::make_shared<Backend>(config.databasePath, m_config.SeqNoBackupFreq))
        , m_dagReferenceChecker(std::make_unique<DagReferenceChecker>(m_backend,
                                std::bind(&MnemosyneDagSync::addReceivedRecord, this, _1, _2, _3)))
        , m_recordValidator(recordValidator)
        , m_dagSync(config.syncPrefix, config.peerPrefix, network,
                    [&](const auto& i){onUpdate(i);},
                    getSecurityOption(keychain, recordValidator, config.peerPrefix))
        , m_randomEngine(std::random_device()())
        , m_lastNameTops(0)
{
    NDN_LOG_INFO("Mnemosyne Initialization Start");


    if (config.numGenesisBlock < config.precedingRecordNum || config.precedingRecordNum <= 1) {
        NDN_THROW(std::runtime_error("Bad config"));
    }

    auto restored_seqNo = m_backend->SeqNumLastContinuous(Backend::DAG_SYNC_GROUP, config.peerPrefix, m_dagSync.getCore().getSeqNo() + 1);
    if (restored_seqNo) {
        m_dagSync.getCore().updateSeqNo(restored_seqNo.value());
        NDN_LOG_INFO("STEP 1" << std::endl
                              << "- Restored sequence id to " << restored_seqNo.value() << " in the Mnemosyne Dag Sync");
    }

    //****STEP 2****
    // Make the genesis data
    for (int i = 0; i < m_config.numGenesisBlock; i++) {
        GenesisRecord genesisRecord(i);
        auto data = make_shared<Data>(genesisRecord.getRecordName());
        auto contentBlock = makeEmptyBlock(tlv::Content);
        genesisRecord.wireEncode(contentBlock);
        data->setContent(contentBlock);
        m_keychain.sign(*data, signingWithSha256());
        genesisRecord.m_data = data;
        m_backend->putRecord(data);
        m_lastNames.push_back(genesisRecord.getRecordFullName());
    }
    NDN_LOG_INFO("STEP 2" << std::endl
                          << "- " << m_config.numGenesisBlock << " genesis records have been added to the Mnemosyne");
    NDN_LOG_INFO("Mnemosyne Initialization Succeed");
}

MnemosyneDagSync::~MnemosyneDagSync() = default;

ReturnCode MnemosyneDagSync::createRecord(Record &record) {
    NDN_LOG_INFO("[MnemosyneDagSync::createRecord] Add new record");

    if (!m_selfLastName.empty()) record.addPointer(m_selfLastName);

    // randomly shuffle the tailing record list
    std::vector<Name> recordList;
    for (const auto& i : m_lastNames) recordList.push_back(i);
    if (recordList.size() < m_config.precedingRecordNum) return ReturnCode::notEnoughTailingRecord();
    std::shuffle(recordList.begin(), recordList.end(), m_randomEngine);

    for (const auto &tailRecord : recordList) {
        record.addPointer(tailRecord);
        if (record.getPointersFromHeader().size() >= m_config.precedingRecordNum)
            break;
    }

    auto data = make_shared<Data>(record.getRecordName());
    auto contentBlock = makeEmptyBlock(tlv::Content);
    record.wireEncode(contentBlock);
    data->setContent(contentBlock);
    data->setFreshnessPeriod(time::minutes(5));

    // sign the packet with peer's key
    try {
        m_keychain.sign(*data, security::signingByIdentity(m_config.peerPrefix));
    }
    catch (const std::exception &e) {
        return ReturnCode::signingError(e.what());
    }
    record.m_data = data;
    NDN_LOG_INFO("[MnemosyneDagSync::createRecord] Added a new record:" << data->getFullName().toUri());

    //send sync interest
    auto seqId = m_dagSync.publishData(data->wireEncode(), data->getFreshnessPeriod(), m_config.peerPrefix, tlv::Data);
    // add new record into the ledger
    addSelfRecord(data, seqId);
    return ReturnCode::noError(data->getFullName().toUri());
}

optional<Record> MnemosyneDagSync::getRecord(const std::string &recordName) const {
    NDN_LOG_DEBUG("getRecord Called on " << recordName);
    return m_backend->getRecord(recordName);
}

bool MnemosyneDagSync::hasRecord(const std::string &recordName) const {
    auto dataPtr = m_backend->getRecord(Name(recordName));
    return dataPtr != nullptr;
}

std::list<Name> MnemosyneDagSync::listRecord(const std::string &prefix) const {
    return m_backend->listRecord(Name(prefix));
}

void MnemosyneDagSync::onUpdate(const std::vector<ndn::svs::MissingDataInfo>& info) {
    for (const auto& stream : info) {
        std::cerr << "Missing Data " << stream.nodeId << " " << stream.low << " " << stream.high << "\n";
        for (svs::SeqNo i = stream.low; i <= stream.high; i++) {
            auto lastNo = m_backend->SeqNumLastContinuous(Backend::DAG_SYNC_GROUP, stream.nodeId, i);
            if (lastNo.has_value()) {
                NDN_LOG_INFO("Skipped in-backend item" << stream.nodeId << " " << i);
                i = lastNo.value();
                continue;
            } else {
                NDN_LOG_INFO("Fetching in-backend item" << stream.nodeId << " " << i);
            }
            m_dagSync.fetchData(stream.nodeId, i, [nodeId=stream.nodeId, i, this](const Data& syncData){
                if (syncData.getContentType() == tlv::Data) {
                    m_recordValidator->validate(Data(syncData.getContent().blockFromValue()), [nodeId, i, this](const Data& data){
                        auto receivedData = std::make_shared<Data>(data);
                        try {
                            auto receivedRecord = make_unique<Record>(receivedData);
                            receivedRecord->checkPointerCount(m_config.precedingRecordNum);
                            m_dagReferenceChecker->addRecord(std::move(receivedRecord), nodeId, i);
                        } catch (const std::exception& e) {
                            NDN_LOG_ERROR("bad record received" << receivedData->getFullName() << ": " << e.what());
                        }
                    }, [](const Data& data, const ndn::security::ValidationError& error){
                        NDN_LOG_ERROR("Verification error on Received record " << data.getFullName() << ": " << error.getInfo());
                    });
                }
            });
        }
    }
}

void MnemosyneDagSync::addSelfRecord(const shared_ptr<Data> &data, svs::SeqNo seqId) {
    NDN_LOG_INFO("Add self record " << data->getFullName());
    m_backend->putRecord(data);
    m_selfLastName = data->getFullName();
    m_backend->SeqNumAdd(Backend::DAG_SYNC_GROUP, m_config.peerPrefix, seqId);
}

void MnemosyneDagSync::addReceivedRecord(std::unique_ptr<Record> record, const Name& producer, svs::SeqNo seqId) {
    NDN_LOG_INFO("Add received record " << record->getRecordFullName());
    const shared_ptr<const Data>& recordData = record->m_data;
    m_backend->SeqNumAdd(Backend::DAG_SYNC_GROUP, producer, seqId);
    m_backend->putRecord(recordData);
    m_lastNames[m_lastNameTops] = recordData->getFullName();
    m_lastNameTops = (m_lastNameTops + 1) % m_lastNames.size();

    if (m_onRecordCallback) {
        m_onRecordCallback(*record);
    }
}

const Name &MnemosyneDagSync::getPeerPrefix() const {
    return m_config.peerPrefix;
}

ndn::svs::SecurityOptions MnemosyneDagSync::getSecurityOption(KeyChain& keychain, shared_ptr<ndn::security::Validator> recordValidator, Name peerPrefix) {
    ndn::svs::SecurityOptions option(keychain);
    option.validator = make_shared<::util::cxxValidator>(recordValidator);
    option.encapsulatedDataValidator = make_shared<::util::alwaysFailValidator>();
    option.dataSigner = std::make_shared<::util::KeyChainOptionSigner>(keychain, security::signingByIdentity(peerPrefix));
    option.interestSigner = option.dataSigner;
    option.pubSigner = std::make_shared<ndn::svs::BaseSigner>();
    return option;
}

}  // namespace mnemosyne
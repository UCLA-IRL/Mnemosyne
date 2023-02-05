//
// Created by Tyler on 2/4/23.
//

#include "record-sync.h"

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <utility>

NDN_LOG_INIT(mnemosyne.dag.RecordSync);

mnemosyne::dag::RecordSync::RecordSync(const ndn::Name &syncPrefix,
                                       const ndn::Name &nodePrefix,
                                       const ndn::Name &hintPrefix,
                                       ndn::Face &face,
                                       const ndn::svs::UpdateCallback &updateCallback,
                                       std::weak_ptr<Backend> backend,
                                       const ndn::svs::SecurityOptions &securityOptions)
        : SVSyncBase(syncPrefix, nodePrefix, nodePrefix,
                     face, updateCallback, securityOptions, make_shared<BackendDataStore>(std::move(backend))),
                     m_face(face),
                     m_hintPrefix(hintPrefix),
                     m_fetcher(face, securityOptions) {
    //TODO fix multicast fetch
    m_registerHintPrefix = m_face.setInterestFilter(hintPrefix,
                                                 std::bind(&RecordSync::onDataInterest, this, _2),
                                                 [] (auto&&...) {});
}

svs::SeqNo mnemosyne::dag::RecordSync::publishData(Record& record, const ndn::time::milliseconds& freshness,
                                                   const svs::NodeID& id, uint32_t contentType) {
    svs::NodeID pubId = id != EMPTY_NODE_ID ? id : m_id;
    svs::SeqNo newSeq = getCore().getSeqNo(pubId) + 1;

    Name dataName = getDataName(pubId, newSeq);

    auto data = make_shared<Data>(dataName);
    auto contentBlock = makeEmptyBlock(tlv::Content);
    record.wireEncode(contentBlock);
    data->setContent(contentBlock);
    data->setFreshnessPeriod(freshness);
    data->setContentType(contentType);
    m_securityOptions.dataSigner->sign(*data);
    record.setEncodedData(data);

    getDataStore().insert(*data);
    getCore().updateSeqNo(newSeq, pubId);
    m_face.put(*data);

    return newSeq;
}

void mnemosyne::dag::RecordSync::fetchRecord(const svs::NodeID &nid, const svs::SeqNo &seq,
                                             const svs::DataValidatedCallback &onValidated, int nRetries,
                                             int forwardingHintRetries,
                                             const svs::DataValidationErrorCallback &onValidationFailed,
                                             const TimeoutCallback &onTimeout) {
    fetchData(nid, seq, onValidated, onValidationFailed,
              [this, nid, seq, onValidated, onValidationFailed, onTimeout, forwardingHintRetries](auto&&...){
                  fetchDataWithHint(nid, seq, onValidated, onValidationFailed, onTimeout, forwardingHintRetries);
              }, nRetries);
}

void mnemosyne::dag::RecordSync::fetchDataWithHint(const svs::NodeID &nid, const svs::SeqNo &seq,
                                                   const svs::DataValidatedCallback &onValidated,
                                                   const svs::DataValidationErrorCallback &onValidationFailed,
                                                   const TimeoutCallback &onTimeout, int nRetries) {
    Name interestName = getDataName(nid, seq);
    Interest interest(interestName);
    interest.setCanBePrefix(true);
    interest.setForwardingHint({m_hintPrefix});
    interest.setInterestLifetime(ndn::time::milliseconds(2000));

    m_fetcher.expressInterest(interest,
                              [this, onValidated](auto &&, auto && PH2) { onDataValidated(std::forward<decltype(PH2)>(PH2), onValidated); },
                              std::bind(onTimeout, _1), // Nack
                              onTimeout, nRetries, onValidationFailed);
}

void mnemosyne::dag::RecordSync::onDataValidated(const Data& data, const svs::DataValidatedCallback& dataCallback) {
    if (shouldCache(data))
        getDataStore().insert(data);

    dataCallback(data);
}

void mnemosyne::dag::RecordSync::onDataInterest(const Interest &interest) {
    NDN_LOG_INFO("Hinted face incoming: " << interest.getName());
    auto data = getDataStore().find(interest);
    if (data != nullptr)
        m_face.put(*data);
}

mnemosyne::dag::RecordSync::BackendDataStore::BackendDataStore(std::weak_ptr<Backend> backend)
        : m_backend(std::move(backend)) {}

shared_ptr<const Data> mnemosyne::dag::RecordSync::BackendDataStore::find(const Interest &interest) {
    auto backend = m_backend.lock();
    auto list = backend->listRecord(interest.getName(), interest.getCanBePrefix()? 1 : 0);
    for (const auto &n: list) {
        if (!interest.getName().isPrefixOf(n)) continue;
        if (!interest.getCanBePrefix() && n.size() > interest.getName().size() + 1) continue;
        return backend->getRecord(n);
    }
    return nullptr;
}

void mnemosyne::dag::RecordSync::BackendDataStore::insert(const Data &data) {
    auto backend = m_backend.lock();
    backend->putRecord(make_shared<Data>(data));
}

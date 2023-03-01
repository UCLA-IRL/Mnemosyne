//
// Created by Tyler on 10/23/21.
//

#include <ndn-svs/svspubsub.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/io.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>
#include <random>

NDN_LOG_INIT(mnemosyne.testClient.sync);

namespace po = boost::program_options;
using namespace ndn;

std::random_device rd;  //Will be used to obtain a seed for the random number engine
std::mt19937 random_gen(rd()); //Standard mersenne_twister_engine seeded with rd()

void
periodicAddRecord(KeyChain &keychain, shared_ptr<svs::SVSPubSub> pubSub, const Name &peerPrefix, Scheduler &scheduler, float freq_mean) {
    std::uniform_int_distribution<int> distribution(0, INT_MAX);
    Data data(Name(peerPrefix).append(std::to_string(distribution(random_gen))));
    data.setContent(makeStringBlock(tlv::Content, std::to_string(distribution(random_gen))));
    keychain.sign(data, signingByIdentity(peerPrefix));

    pubSub->publishPacket(data);
    NDN_LOG_INFO("Adding packet " << data.getFullName());

    // schedule for the next record generation
    std::exponential_distribution<> d(freq_mean);
    auto interval = (long long) (d(random_gen) * 1000000);
    scheduler.schedule(time::microseconds(interval), [&keychain, pubSub, peerPrefix, &scheduler, freq_mean] {
        periodicAddRecord(keychain, pubSub, peerPrefix, scheduler, freq_mean);
    });
}

int main(int argc, char **argv) {
    po::options_description description("Usage for Mnemosyne Test Client");

    description.add_options()
            ("help,h", "Display this help message")
            ("interface-ps-prefix,i", po::value<std::string>()->default_value("/ndn/broadcast/mnemosyne"),
             "The prefix for Interface Pub/Sub")
            ("client-prefix,c", po::value<std::string>(), "The prefix for the client")
            ("frequency,f", po::value<float>()->default_value(0.2), "Mean frequency of sending logs per seconds");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << description;
        return 0;
    }

    if (vm.count("client-prefix") == 0) {
        std::cout << "missing parameter: Client Prefix\n";
        return 2;
    }

    boost::asio::io_service ioService;
    Face face(ioService);
    security::KeyChain keychain;

    std::shared_ptr<svs::SVSPubSub> interfacePS = std::make_shared<svs::SVSPubSub>(
            vm["interface-ps-prefix"].as<std::string>(), vm["client-prefix"].as<std::string>(), face, nullptr);

    Scheduler scheduler(ioService);
    periodicAddRecord(keychain, interfacePS, vm["client-prefix"].as<std::string>(), scheduler, vm["frequency"].as<float>());

    face.processEvents();
    return 0;
}
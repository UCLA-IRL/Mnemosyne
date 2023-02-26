// License: LGPL v3.0

#include "mnemosyne/mnemosyne.hpp"
#include <ndn-cxx/security/validator.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <iostream>

namespace po = boost::program_options;
using namespace ndn;
using namespace mnemosyne;

int main(int argc, char* argv[]) {

    po::options_description description("Usage for Mnemosyne Logger");

    description.add_options()
            ("help,h", "Display this help message")
            ("dag-sync-prefix,m", po::value<std::string>()->default_value("/ndn/broadcast/mnemosyne-dag"), "The prefix for DAG synchronization")
            ("dag-hint-prefix,i", po::value<std::string>()->default_value("/ndn/broadcast/mnemosyne-hint"), "The prefix for DAG recovery hint")
            ("interface-ps-prefix,p", po::value<std::vector<std::string>>()->multitoken()->zero_tokens(), "The prefix for Interface Pub/Sub")
            ("interface-sync-prefix,s", po::value<std::vector<std::string>>()->multitoken()->zero_tokens(), "The prefix for Interface Sync")
            ("logger-prefix,l", po::value<std::string>(), "The prefix for the logger")
            ("trust-anchor,a", po::value<std::string>()->default_value("./mnemosyne-anchor.cert"), "The trust anchor file path for the logger")
            ("database-type,t", po::value<std::string>()->default_value("leveldb"), "The database type for the logger")
            ("database-path,d", po::value<std::string>()->default_value("/tmp/mnemosyne-db/..."), "The database path for the logger");

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(description).run(), vm);
    po::notify(vm);

    if(vm.count("help")){
        std::cout << description << std::endl;
        return 0;
    }

    if (vm.count("logger-prefix") == 0) {
        std::cout << "missing parameter: Logger Prefix\n";
        return 2;
    }


    std::string identity = vm["logger-prefix"].as<std::string>();
    std::string databasePath = vm["database-path"].as<std::string>() == "/tmp/mnemosyne-db/..." ?
        std::string("/tmp/mnemosyne-db/" + identity.substr(identity.rfind('/'))) :
                               vm["database-path"].as<std::string>();

    //interface processing
    std::set<Name> ps_set, sync_set;
    for (const auto& p: vm["interface-ps-prefix"].as<std::vector<std::string>>()) {
        ps_set.emplace(p);
    }
    for (const auto& p: vm["interface-sync-prefix"].as<std::vector<std::string>>()) {
        sync_set.emplace(p);
    }

    boost::asio::io_service ioService;
    Face face(ioService);
    security::KeyChain keychain;
    std::shared_ptr<Config> config;
    try {
        config = std::make_shared<Config>(Name(vm["dag-sync-prefix"].as<std::string>()),
                                          Name(vm["dag-hint-prefix"].as<std::string>()),
                                          Name(identity), ps_set, sync_set);
        config->setDatabase(vm["database-type"].as<std::string>(), databasePath);
        mkdir("/tmp/mnemosyne-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    auto ledger = std::make_shared<Mnemosyne>(*config, keychain, face,
                                              std::make_shared<ndn::security::ValidatorNull>(),
                                                      std::make_shared<ndn::security::ValidatorNull>());

    face.processEvents();
    return 0;
}

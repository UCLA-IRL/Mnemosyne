#include "storage.h"

#include "storage-leveldb.h"
#include "storage-memory.h"

namespace mnemosyne::storage {

std::unique_ptr<Storage> getStorage(std::string type, const std::string &config) {
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type == "leveldb") {
        return std::make_unique<StorageLevelDb>(config);
    }
    if (type == "memory") {
        return std::make_unique<StorageMemory>();
    }
    return nullptr;
}

} // namespace mnemosyne

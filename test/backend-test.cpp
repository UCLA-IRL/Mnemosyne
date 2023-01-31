#include "backend/storage.h"
#include <ndn-cxx/name.hpp>
#include <iostream>

using namespace mnemosyne;

std::shared_ptr<ndn::Data>
makeData(const std::string& name, const std::string& content)
{
  using namespace ndn;
  using namespace std;
  auto data = make_shared<Data>(ndn::Name(name));
  data->setContent(make_span(reinterpret_cast<const uint8_t *>(content.data()), content.size()));
  data->setSignatureInfo(SignatureInfo(tlv::SignatureSha256WithRsa));
  data->setSignatureValue(ndn::encoding::makeEmptyBlock(tlv::SignatureValue).getBuffer());
  data->wireEncode();
  return data;
}

bool
testBackEnd()
{
  Storage backend("/tmp/test.leveldb");
  for (const auto &name : backend.listRecord("")) {
      backend.deleteRecord(name);
  }
  auto data = makeData("/mnemosyne/12345", "content is 12345");
  auto fullName = data->getFullName();

  backend.putRecord(data);

  auto anotherRecord = backend.getRecord(fullName);
  if (data == nullptr || anotherRecord == nullptr) {
      return false;
  }
  return backend.listRecord(Name("/mnemosyne")).size() == 1 && data->wireEncode() == anotherRecord->wireEncode();
}

bool
testBackEndList() {
    Storage backend("/tmp/test-List.leveldb");
    for (const auto &name : backend.listRecord("/")) {
        backend.deleteRecord(name);
    }
    for (int i = 0; i < 10; i++) {
        backend.putRecord(makeData("/mnemosyne/a/" + std::to_string(i), "content is " + std::to_string(i)));
        backend.putRecord(makeData("/mnemosyne/ab/" + std::to_string(i), "content is " + std::to_string(i)));
        backend.putRecord(makeData("/mnemosyne/b/" + std::to_string(i), "content is " + std::to_string(i)));
    }

    //check if there are no preceding slash...
    backend.putRecord(makeData("mnemosyne/a", "content is "));
    backend.putRecord(makeData("mnemosyne/ab", "content is "));
    backend.putRecord(makeData("mnemosyne/b", "content is "));

    assert(backend.listRecord(Name("/mnemosyne")).size() == 33);
    assert(backend.listRecord(Name("/mnemosyne/a")).size() == 11);
    assert(backend.listRecord(Name("/mnemosyne/ab")).size() == 11);
    assert(backend.listRecord(Name("/mnemosyne/b")).size() == 11);
    assert(backend.listRecord(Name("/mnemosyne/a/5")).size() == 1);
    assert(backend.listRecord(Name("/mnemosyne/ab/5")).size() == 1);
    assert(backend.listRecord(Name("/mnemosyne/b/5")).size() == 1);
    assert(backend.listRecord(Name("/mnemosyne/a/55")).empty());
    assert(backend.listRecord(Name("/mnemosyne/ab/55")).empty());
    assert(backend.listRecord(Name("/mnemosyne/b/55")).empty());
    return true;
}

bool testNameGet() {
    if (ndn::Name("").toUri() != "/") return false;
    if (ndn::Name("a/b").toUri() != "/a/b") return false;

    std::string name1 = "name1";
    ndn::Name name2("/mnemosyne/name1/123");
    if (name2.get(-2).toUri() == name1) {
        return true;
    }
    return false;
}

bool testMetaDataStore() {
    Storage backend("/tmp/test-List.leveldb");
    if (backend.placeMetaData("/a", "abc")) return false;
    if (!backend.placeMetaData("a", "abc")) return false;

    if (!backend.getMetaData("a")) return false;
    if (*backend.getMetaData("a") != "abc") return false;
    if (backend.getMetaData("b")) return false;
    return true;
}

int
main(int argc, char** argv)
{
  auto success = testBackEnd();
  if (!success) {
    std::cout << "testBackEnd failed" << std::endl;
  }
  else {
    std::cout << "testBackEnd with no errors" << std::endl;
  }
  success = testBackEndList();
  if (!success) {
    std::cout << "testBackEndList failed" << std::endl;
  }
  else {
    std::cout << "testBackEndList with no errors" << std::endl;
  }
    success = testNameGet();
    if (!success) {
        std::cout << "testNameGet failed" << std::endl;
    }
    else {
        std::cout << "testNameGet with no errors" << std::endl;
    }
    success = testMetaDataStore();
    if (!success) {
        std::cout << "testMetaDataStore failed" << std::endl;
    }
    else {
        std::cout << "testMetaDataStore with no errors" << std::endl;
    }
  return 0;
}
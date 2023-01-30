//
// Created by Tyler on 1/29/23.
//

#include "seq-no-recovery.h"

#include <iostream>

bool accumlator_test() {
    mnemosyne::SeqNoRecovery::SegmentAccumulator acc;
    if (acc.isIn(0)) return false;
    acc.add(3);
    acc.add(2);
    acc.add(1);
    auto b = acc.encode();
    b.parse();
    if (b.elements_size() != 2) return false;
    acc.add(5);
    acc.add(6);
    acc.add(7);
    b = acc.encode();
    b.parse();
    if (b.elements_size() != 4) return false;
    if (acc.isIn(0) || !acc.isIn(1) || !acc.isIn(2) || !acc.isIn(3) ||
        acc.isIn(4) || !acc.isIn(5) || !acc.isIn(6) || !acc.isIn(7) || acc.isIn(8)) return false;
    acc.add(4);
    b = acc.encode();
    b.parse();
    if (b.elements_size() != 2) return false;
    return true;
}

bool accumlator_encoding_test() {
    mnemosyne::SeqNoRecovery::SegmentAccumulator acc;
    acc.add(3);
    acc.add(2);
    acc.add(1);
    acc.add(5);
    acc.add(7);
    acc.add(6);
    auto b = acc.encode();
    b.parse();
    if (b.elements_size() != 4) return false;
    mnemosyne::SeqNoRecovery::SegmentAccumulator acc2;
    if (!acc2.decode(b)) return false;
    if (acc.isIn(0) || !acc.isIn(1) || !acc.isIn(2) || !acc.isIn(3) ||
        acc.isIn(4) || !acc.isIn(5) || !acc.isIn(6) || !acc.isIn(7) || acc.isIn(8)) return false;
    return true;
}

bool recovery_test() {
    mnemosyne::SeqNoRecovery rec;
    rec.getStream(mnemosyne::SeqNoRecovery::DAG_SYNC_GROUP, "/a/b").add(1);
    rec.getStream(mnemosyne::SeqNoRecovery::DAG_SYNC_GROUP, "/a/c").add(2);
    rec.getStream(mnemosyne::SeqNoRecovery::LISTEN_GROUP, "/a/c").add(2);
    auto b = rec.encode();

    mnemosyne::SeqNoRecovery rec2;
    rec2.decode(b);
    return rec.getStream(mnemosyne::SeqNoRecovery::DAG_SYNC_GROUP, "/a/b").isIn(1) &&
           rec.getStream(mnemosyne::SeqNoRecovery::DAG_SYNC_GROUP, "/a/c").isIn(2) &&
           rec.getStream(mnemosyne::SeqNoRecovery::LISTEN_GROUP, "/a/c").isIn(2) &&
           !rec.getStream(mnemosyne::SeqNoRecovery::LISTEN_GROUP, "/a/d").isIn(1);
}

int main() {
    auto success = accumlator_test();
    if (!success) {
        std::cout << "accumlator_test failed" << std::endl;
    }
    else {
        std::cout << "accumlator_test with no errors" << std::endl;
    }
    success = accumlator_encoding_test();
    if (!success) {
        std::cout << "accumlator_encoding_test failed" << std::endl;
    }
    else {
        std::cout << "accumlator_encoding_test with no errors" << std::endl;
    }
    success = recovery_test();
    if (!success) {
        std::cout << "recovery_test failed" << std::endl;
    }
    else {
        std::cout << "recovery_test with no errors" << std::endl;
    }
}
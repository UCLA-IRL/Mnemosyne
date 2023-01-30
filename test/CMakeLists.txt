
add_executable(backend-test backend-test.cpp)
target_include_directories(backend-test PUBLIC ../src)
target_link_libraries(backend-test PUBLIC mnemosyne)

add_executable(seq-no-recovery-test seq-no-recovery-test.cpp)
target_include_directories(seq-no-recovery-test PRIVATE ../src)
target_link_libraries(seq-no-recovery-test PUBLIC mnemosyne)

add_executable(record-test record-test.cpp)
target_link_libraries(record-test PUBLIC mnemosyne)

add_executable(dag-sync-test dag-sync-test.cpp)
target_link_libraries(dag-sync-test PUBLIC mnemosyne)

add_executable(mnemosyne-test-client mnemosyne-test-client.cpp)
target_link_libraries(mnemosyne-test-client PUBLIC mnemosyne)
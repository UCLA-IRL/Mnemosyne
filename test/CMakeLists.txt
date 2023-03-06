
add_executable(backend-test backend-test.cpp)
target_include_directories(backend-test PUBLIC ../src)
target_link_libraries(backend-test PUBLIC mnemosyne)

add_executable(record-test record-test.cpp)
target_link_libraries(record-test PUBLIC mnemosyne)

add_executable(replication-counter-test replication-counter-test.cpp)
target_include_directories(replication-counter-test PUBLIC ../src)
target_link_libraries(replication-counter-test PUBLIC mnemosyne)

add_executable(dag-sync-test dag-sync-test.cpp)
target_link_libraries(dag-sync-test PUBLIC mnemosyne)

add_executable(mnemosyne-test-client mnemosyne-test-client.cpp)
target_compile_options(mnemosyne-test-client PUBLIC ${NDN_CXX_CFLAGS} ${NDN_SVS_CFLAGS})
target_link_libraries(mnemosyne-test-client PUBLIC ${NDN_CXX_LIBRARIES} ${NDN_SVS_LIBRARIES})

add_executable(mnemosyne-test-client-sync mnemosyne-test-client-sync.cpp)
target_compile_options(mnemosyne-test-client-sync PUBLIC ${NDN_CXX_CFLAGS} ${NDN_SVS_CFLAGS})
target_link_libraries(mnemosyne-test-client-sync PUBLIC ${NDN_CXX_LIBRARIES} ${NDN_SVS_LIBRARIES})
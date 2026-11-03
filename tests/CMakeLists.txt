# tests/CMakeLists.txt

# Memastikan Google Test tersedia
find_package(GTest REQUIRED)

# Definisi executable untuk test packet generation
add_executable(test_packet_gen test_packet_gen.cpp)
target_include_directories(test_packet_gen PRIVATE ..) # Akses ke ../core/PacketGenerator.h
target_link_libraries(test_packet_gen 
    GTest::gtest_main 
    gtest 
    # Tambahkan library lain jika PacketGenerator membutuhkan logika tambahan
    # pthread 
)
# Jika projectmu menggunakan C++17/20
target_compile_features(test_packet_gen PRIVATE cxx_std_17)

# Definisi executable untuk test socket manager
add_executable(test_socket_manager test_socket_manager.cpp)
target_include_directories(test_socket_manager PRIVATE ..) # Akses ke ../network/SocketManager.h
target_link_libraries(test_socket_manager 
    GTest::gtest_main 
    gtest
    # SocketManager mungkin butuh linking library network standar
    # -lpthread -lsocket -lnsl 
)
target_compile_features(test_socket_manager PRIVATE cxx_std_17)

# Opsi untuk menjalankan test saat build selesai (opsional)
# enable_testing()
# add_test(NAME PacketGenTests COMMAND test_packet_gen)
# add_test(NAME SocketMgrTests COMMAND test_socket_manager)

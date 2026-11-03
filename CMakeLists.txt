cmake_minimum_required(VERSION 3.15)
project(BrutalDDoS VERSION 1.0.0 LANGUAGES CXX)

# Set standar C++ ke C++17 (diperlukan untuk fitur modern seperti filesystem dan structured bindings)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Konfigurasi Compiler Flags ---
# -Wall -Wextra: Warning lengkap
# -O2: Optimasi performa (penting untuk DDoS tool)
# -pthread: Support multithreading
add_compile_options(-Wall -Wextra -O2 -pthread)

# --- Include Directories ---
# Menambahkan path header lokal dan third_party agar bisa #include "nama_file.h" tanpa prefix path panjang
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/third_party)

# --- Kumpulkan Semua File Sumber (.cpp) Secara Otomatis ---
# Ini akan mencari semua file .cpp di subfolder src/ secara rekursif
file(GLOB_RECURSE SRC_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# Tambahkan juga file main.cpp jika belum tercover oleh glob (biasanya glob mencakupnya, tapi untuk safety)
list(APPEND SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Hapus duplikat jika ada
list(REMOVE_DUPLICATES SRC_FILES)

# --- Target Eksekutable Utama ---
add_executable(brutaldDos ${SRC_FILES})

# --- Link Library Wajib ---
# pthread untuk threading, socket untuk networking (Linux)
target_link_libraries(brutaldDos PRIVATE pthread)

# --- Dependensi Third Party (JSON Parser) ---
# Jika menggunakan nlohmann/json.hpp, biasanya header-only jadi tidak perlu link library tambahan
# Tapi pastikan header tersebut tersedia di third_party/

# --- Opsi Build Tambahan (Opsional) ---
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Install Rules (Opsional) ---
install(TARGETS brutaldDos DESTINATION bin)

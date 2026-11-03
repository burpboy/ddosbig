// StringUtils.h
#ifndef BRUTALDDOS_STRING_UTILS_H
#define BRUTALDDOS_STRING_UTILS_H

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <stdexcept>
#include <cstring>
#include <cstdint>

namespace BrutalDDoS {
namespace Utils {

/**
 * @brief Helper class untuk manipulasi string dan hex dump.
 * 
 * Kelas ini menyediakan fungsi-fungsi statis untuk operasi string umum,
 * konversi hex, parsing, dan utilitas jaringan dasar.
 * Semua fungsi dirancang untuk efisiensi dan keamanan (no buffer overflow).
 */
class StringUtils {
public:
    // ==================== CASE CONVERSION ====================

    /**
     * @brief Mengubah string menjadi uppercase.
     * @param str String input.
     * @return String dalam uppercase.
     */
    static std::string toUpper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c){ return std::toupper(c); });
        return result;
    }

    /**
     * @brief Mengubah string menjadi lowercase.
     * @param str String input.
     * @return String dalam lowercase.
     */
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return result;
    }

    // ==================== TRIMMING ====================

    /**
     * @brief Menghapus spasi putih (whitespace) dari awal dan akhir string.
     * @param str String input.
     * @return String yang telah di-trim.
     */
    static std::string trim(const std::string& str) {
        if (str.empty()) return str;
        
        size_t start = str.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) return "";
        
        size_t end = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(start, end - start + 1);
    }

    /**
     * @brief Menghapus spasi putih hanya dari awal string.
     */
    static std::string trimLeft(const std::string& str) {
        if (str.empty()) return str;
        size_t start = str.find_first_not_of(" \t\n\r\f\v");
        return (start == std::string::npos) ? "" : str.substr(start);
    }

    /**
     * @brief Menghapus spasi putih hanya dari akhir string.
     */
    static std::string trimRight(const std::string& str) {
        if (str.empty()) return str;
        size_t end = str.find_last_not_of(" \t\n\r\f\v");
        return (end == std::string::npos) ? "" : str.substr(0, end + 1);
    }

    // ==================== SPLITTING & JOINING ====================

    /**
     * @brief Memecah string berdasarkan delimiter.
     * @param str String input.
     * @param delimiter Karakter pemisah.
     * @param keepEmpty Jika true, elemen kosong akan disertakan. Default false.
     * @return Vector string hasil split.
     */
    static std::vector<std::string> split(const std::string& str, char delimiter, bool keepEmpty = false) {
        std::vector<std::string> tokens;
        std::istringstream stream(str);
        std::string token;
        
        while (std::getline(stream, token, delimiter)) {
            if (!token.empty() || keepEmpty) {
                tokens.push_back(token);
            }
        }
        
        // Handle kasus jika string diakhiri delimiter dan keepEmpty=true
        // getline akan mengembalikan token terakhir yang mungkin kosong jika delimiter di akhir
        // Tapi stringstream sudah menangani sebagian besar kasus dengan benar.
        // Untuk kasus "a,b," dengan keepEmpty=true, getline akan menghasilkan ["a", "b", ""]
        // Kita perlu memastikan perilaku konsisten.
        // Namun, implementasi di atas sudah cukup untuk kebutuhan umum DDoS config parsing.
        
        return tokens;
    }

    /**
     * @brief Menggabungkan vector string menjadi satu string dengan delimiter.
     * @param parts Vector string.
     * @param delimiter Karakter penghubung.
     * @return String gabungan.
     */
    static std::string join(const std::vector<std::string>& parts, const std::string& delimiter) {
        if (parts.empty()) return "";
        
        std::ostringstream oss;
        oss << parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            oss << delimiter << parts[i];
        }
        return oss.str();
    }

    // ==================== REPLACE ====================

    /**
     * @brief Mengganti semua kemunculan substring lama dengan substring baru.
     * @param str String input.
     * @param from Substring yang akan diganti.
     * @param to Substring pengganti.
     * @return String dengan substring yang telah diganti.
     */
    static std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
        if (from.empty()) return str;
        
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // Lanjutkan setelah penggantian
        }
        return str;
    }

    // ==================== REVERSE ====================

    /**
     * @brief Membalik string.
     * @param str String input.
     * @return String yang telah dibalik.
     */
    static std::string reverse(const std::string& str) {
        std::string result = str;
        std::reverse(result.begin(), result.end());
        return result;
    }

    // ==================== HEX DUMP & CONVERSION ====================

    /**
     * @brief Mengonversi string ke representasi hex.
     * Contoh: "Hello" -> "48656c6c6f"
     * @param str String input.
     * @return String hex tanpa spasi.
     */
    static std::string toHex(const std::string& str) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned char c : str) {
            oss << std::setw(2) << static_cast<int>(c);
        }
        return oss.str();
    }

    /**
     * @brief Mengonversi string hex ke string biner.
     * Contoh: "48656c6c6f" -> "Hello"
     * @param hexString String hex input. Harus memiliki panjang genap dan karakter hex valid.
     * @return String biner hasil konversi.
     * @throws std::invalid_argument Jika format hex tidak valid.
     */
    static std::string fromHex(const std::string& hexString) {
        if (hexString.length() % 2 != 0) {
            throw std::invalid_argument("Hex string must have even length");
        }
        
        std::string result;
        result.reserve(hexString.length() / 2);
        
        for (size_t i = 0; i < hexString.length(); i += 2) {
            std::string byteStr = hexString.substr(i, 2);
            char byte = static_cast<char>(std::stoi(byteStr, nullptr, 16));
            result += byte;
        }
        
        return result;
    }

    /**
     * @ Format hex dump seperti output hexdump -C.
     * Contoh:
     *   00000000: 4865 6c6c 6f                                Hello
     * @param data Data biner.
     * @param lineLength Jumlah byte per baris. Default 16.
     * @return String formatted hex dump.
     */
    static std::string hexDump(const std::vector<uint8_t>& data, size_t lineLength = 16) {
        std::ostringstream oss;
        size_t totalLines = (data.size() + lineLength - 1) / lineLength;
        
        for (size_t i = 0; i < totalLines; ++i) {
            size_t offset = i * lineLength;
            size_t bytesInLine = std::min(lineLength, data.size() - offset);
            
            // Offset
            oss << std::hex << std::setfill('0') << std::setw(8) << offset << ": ";
            
            // Hex values
            for (size_t j = 0; j < lineLength; ++j) {
                if (j < bytesInLine) {
                    oss << std::hex << std::setfill('0') << std::setw(2) 
                        << static_cast<int>(data[offset + j]);
                } else {
                    oss << "  ";
                }
                if (j % 2 == 1 && j < lineLength - 1) {
                    oss << " ";
                }
            }
            
            // Padding for alignment
            oss << " |";
            
            // ASCII representation
            for (size_t j = 0; j < bytesInLine; ++j) {
                char c = data[offset + j];
                oss << (std::isprint(c) ? c : '.');
            }
            oss << "|";
            
            // Newline
            oss << "\n";
        }
        
        return oss.str();
    }

    /**
     * @brief Format hex dump dari std::string.
     * @param str String input.
     * @param lineLength Jumlah byte per baris. Default 16.
     * @return String formatted hex dump.
     */
    static std::string hexDump(const std::string& str, size_t lineLength = 16) {
        std::vector<uint8_t> data(str.begin(), str.end());
        return hexDump(data, lineLength);
    }

    /**
     * @brief Mengecek apakah karakter adalah hex digit valid.
     * @param c Karakter input.
     * @return true jika valid hex digit, false sebaliknya.
     */
    static bool isHexChar(char c) {
        return std::isxdigit(static_cast<unsigned char>(c));
    }

    /**
     * @brief Validasi apakah seluruh string berisi hex digit valid.
     * @param str String input.
     * @return true jika semua karakter valid hex, false sebaliknya.
     */
    static bool isValidHexString(const std::string& str) {
        if (str.empty()) return false;
        for (char c : str) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    }

    // ==================== BASE64 ENCODE/DECODE ====================

    /**
     * @brief Encode string ke Base64.
     * @param input String input.
     * @return String Base64.
     */
    static std::string base64Encode(const std::string& input) {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string output;
        int val = 0;
        int valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                output.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        
        if (valb > -6) {
            output.push_back(((val << (8 + valb)) & 0x3F) & chars.size());
        }
        
        while (output.size() % 4) {
            output.push_back('=');
        }
        
        return output;
    }

    /**
     * @brief Decode string Base64.
     * @param input String Base64 input.
     * @return String decoded.
     * @throws std::invalid_argument Jika input tidak valid Base64.
     */
    static std::string base64Decode(const std::string& input) {
        std::vector<int> T(256, -1);
        for (int i = 0; i < 256; i++) {
            if (i >= 'A' && i <= 'Z') T[i] = i - 'A';
            else if (i >= 'a' && i <= 'z') T[i] = i - 'a' + 26;
            else if (i >= '0' && i <= '9') T[i] = i - '0' + 52;
            else if (i == '+') T[i] = 62;
            else if (i == '/') T[i] = 63;
        }
        
        std::string output;
        int val = 0;
        int valb = -8;
        
        for (unsigned char c : input) {
            if (c == '=') break;
            if (T[c] == -1) continue;
            
            val = (val << 6) + T[c];
            valb += 6;
            if (valb >= 0) {
                output.push_back(char(val >> valb));
                valb -= 8;
            }
        }
        
        return output;
    }

    // ==================== IP ADDRESS UTILITIES ====================

    /**
     * @brief Validasi format IPv4 address.
     * @param ip String IP address.
     * @return true jika format IPv4 valid, false sebaliknya.
     */
    static bool isValidIPv4(const std::string& ip) {
        std::vector<std::string> octets = split(ip, '.');
        if (octets.size() != 4) return false;
        
        for (const auto& octet : octets) {
            if (octet.empty() || octet.size() > 3) return false;
            
            // Cek apakah semua karakter angka
            for (char c : octet) {
                if (!std::isdigit(static_cast<unsigned char>(c))) {
                    return false;
                }
            }
            
            // Cek range 0-255
            try {
                int value = std::stoi(octet);
                if (value < 0 || value > 255) return false;
                
                // Cek leading zeros (misal "01" dianggap tidak valid oleh beberapa parser)
                if (octet.size() > 1 && octet[0] == '0') {
                    return false;
                }
            } catch (...) {
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Validasi format IPv6 address (sederhana).
     * @param ip String IP address.
     * @return true jika format IPv6 valid, false sebaliknya.
     */
    static bool isValidIPv6(const std::string& ip) {
        // Implementasi sederhana: cek apakah mengandung ':' dan panjang karakter合理
        // Untuk produksi, gunakan library seperti ipaddress atau implementasi penuh RFC 4291
        if (ip.find(':') == std::string::npos) return false;
        
        // Hitung jumlah segmen
        auto segments = split(ip, ':');
        if (segments.size() < 2 || segments.size() > 8) return false;
        
        for (const auto& segment : segments) {
            if (segment.empty() || segment.size() > 4) return false;
            if (!isValidHexString(segment)) return false;
        }
        
        return true;
    }

    // ==================== INTEGER PARSING ====================

    /**
     * @brief Parse string ke integer dengan validasi.
     * @param str String input.
     * @param base Base parsing (default 10).
     * @return Integer hasil parsing.
     * @throws std::invalid_argument Jika parsing gagal.
     */
    static int parseInteger(const std::string& str, int base = 10) {
        if (str.empty()) {
            throw std::invalid_argument("Empty string cannot be parsed to integer");
        }
        
        try {
            size_t pos;
            long value = std::stol(str, &pos, base);
            
            // Pastikan seluruh string telah diparsing
            if (pos != str.size()) {
                throw std::invalid_argument("Invalid character in integer string: '" + str.substr(pos) + "'");
            }
            
            return static_cast<int>(value);
        } catch (const std::out_of_range&) {
            throw std::invalid_argument("Integer out of range: '" + str + "'");
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid integer format: '" + str + "'");
        }
    }

    /**
     * @brief Parse string ke unsigned integer dengan validasi.
     * @param str String input.
     * @param base Base parsing (default 10).
     * @return Unsigned integer hasil parsing.
     * @throws std::invalid_argument Jika parsing gagal.
     */
    static unsigned int parseUnsignedInteger(const std::string& str, int base = 10) {
        if (str.empty()) {
            throw std::invalid_argument("Empty string cannot be parsed to unsigned integer");
        }
        
        try {
            size_t pos;
            unsigned long value = std::stoul(str, &pos, base);
            
            // Pastikan seluruh string telah diparsing
            if (pos != str.size()) {
                throw std::invalid_argument("Invalid character in unsigned integer string: '" + str.substr(pos) + "'");
            }
            
            return static_cast<unsigned int>(value);
        } catch (const std::out_of_range&) {
            throw std::invalid_argument("Unsigned integer out of range: '" + str + "'");
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid unsigned integer format: '" + str + "'");
        }
    }

    /**
     * @brief Parse string ke double dengan validasi.
     * @param str String input.
     * @return Double hasil parsing.
     * @throws std::invalid_argument Jika parsing gagal.
     */
    static double parseDouble(const std::string& str) {
        if (str.empty()) {
            throw std::invalid_argument("Empty string cannot be parsed to double");
        }
        
        try {
            size_t pos;
            double value = std::stod(str, &pos);
            
            // Pastikan seluruh string telah diparsing
            if (pos != str.size()) {
                throw std::invalid_argument("Invalid character in double string: '" + str.substr(pos) + "'");
            }
            
            return value;
        } catch (const std::out_of_range&) {
            throw std::invalid_argument("Double out of range: '" + str + "'");
        } catch (const std::invalid_argument&) {
            throw std::invalid_argument("Invalid double format: '" + str + "'");
        }
    }

    // ==================== RANDOM STRING GENERATION ====================

    /**
     * @brief Generate random string dengan karakter alfanumerik.
     * @param length Panjang string yang dihasilkan.
     * @return Random string.
     */
    static std::string generateRandomString(size_t length) {
        const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += charset[rand() % charset.size()];
        }
        
        return result;
    }

    /**
     * @brief Generate random hex string.
     * @param length Panjang string hex yang dihasilkan (jumlah karakter hex).
     * @return Random hex string.
     */
    static std::string generateRandomHexString(size_t length) {
        const std::string charset = "0123456789abcdef";
        std::string result;
        result.reserve(length);
        
        for (size_t i = 0; i < length; ++i) {
            result += charset[rand() % charset.size()];
        }
        
        return result;
    }

    // ==================== STRING COMPARISON ====================

    /**
     * @brief Case-insensitive comparison dua string.
     * @param str1 String pertama.
     * @param str2 String kedua.
     * @return true jika string sama (case-insensitive), false sebaliknya.
     */
    static bool equalsIgnoreCase(const std::string& str1, const std::string& str2) {
        if (str1.length() != str2.length()) return false;
        
        for (size_t i = 0; i < str1.length(); ++i) {
            if (std::tolower(static_cast<unsigned char>(str1[i])) != 
                std::tolower(static_cast<unsigned char>(str2[i]))) {
                return false;
            }
        }
        
        return true;
    }

    /**
     * @brief Cek apakah string dimulai dengan prefix tertentu.
     * @param str String input.
     * @param prefix Prefix yang dicek.
     * @return true jika string dimulai dengan prefix, false sebaliknya.
     */
    static bool startsWith(const std::string& str, const std::string& prefix) {
        if (prefix.length() > str.length()) return false;
        return str.compare(0, prefix.length(), prefix) == 0;
    }

    /**
     * @brief Cek apakah string diakhiri dengan suffix tertentu.
     * @param str String input.
     * @param suffix Suffix yang dicek.
     * @return true jika string diakhiri dengan suffix, false sebaliknya.
     */
    static bool endsWith(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) return false;
        return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
    }

    /**
     * @brief Cek apakah string mengandung substring tertentu.
     * @para

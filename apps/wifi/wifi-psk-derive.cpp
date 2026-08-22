#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
    if (argc != 2) return 64;
    const std::string ssid(argv[1]);
    if (ssid.empty() || ssid.size() > 32) return 65;

    std::string passphrase;
    if (!std::getline(std::cin, passphrase)) return 65;
    if (!passphrase.empty() && passphrase.back() == '\r') passphrase.pop_back();
    const bool printable = passphrase.size() >= 8 && passphrase.size() <= 63
        && std::all_of(passphrase.begin(), passphrase.end(), [](unsigned char c) {
            return c >= 0x20 && c <= 0x7e;
        });
    if (!printable) {
        OPENSSL_cleanse(passphrase.data(), passphrase.size());
        return 65;
    }

    std::array<unsigned char, 32> key{};
    const int derived = PKCS5_PBKDF2_HMAC(passphrase.data(), static_cast<int>(passphrase.size()),
        reinterpret_cast<const unsigned char *>(ssid.data()), static_cast<int>(ssid.size()),
        4096, EVP_sha1(), static_cast<int>(key.size()), key.data());
    OPENSSL_cleanse(passphrase.data(), passphrase.size());
    if (derived != 1) return 69;

    std::cout << std::hex << std::setfill('0');
    for (const unsigned char byte : key) std::cout << std::setw(2) << static_cast<unsigned int>(byte);
    std::cout << '\n';
    OPENSSL_cleanse(key.data(), key.size());
    return std::cout.good() ? 0 : 69;
}

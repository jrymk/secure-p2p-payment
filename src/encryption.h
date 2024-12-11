#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <string>
#include <vector>
#include <stdexcept>

void generateKeys(const std::string &private_key_file, const std::string &public_key_file) {
    RSA *rsa = RSA_generate_key(2048, RSA_F4, nullptr, nullptr);

    FILE *private_file = fopen(private_key_file.c_str(), "wb");
    PEM_write_RSAPrivateKey(private_file, rsa, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(private_file);

    FILE *public_file = fopen(public_key_file.c_str(), "wb");
    PEM_write_RSAPublicKey(public_file, rsa);
    fclose(public_file);

    RSA_free(rsa);
}

std::string encrypt(RSA *publicKey, const std::string &message) {
    std::vector<unsigned char> encrypted(RSA_size(publicKey));
    int encrypted_length = RSA_public_encrypt(message.size(), reinterpret_cast<const unsigned char *>(message.c_str()),
                                              encrypted.data(), publicKey, RSA_PKCS1_OAEP_PADDING);

    if (encrypted_length == -1)
        throw std::runtime_error("Encryption failed: " + std::string(ERR_error_string(ERR_get_error(), nullptr)));
    return std::string(encrypted.begin(), encrypted.end());
}

std::string decrypt(RSA *privateKey, const std::string &encrypted_message) {
    std::vector<unsigned char> decrypted(RSA_size(privateKey));
    int decrypted_length = RSA_private_decrypt(encrypted_message.size(), reinterpret_cast<const unsigned char *>(encrypted_message.c_str()),
                                               decrypted.data(), privateKey, RSA_PKCS1_OAEP_PADDING);

    if (decrypted_length == -1)
        throw std::runtime_error("Decryption failed: " + std::string(ERR_error_string(ERR_get_error(), nullptr)));
    return std::string(decrypted.begin(), decrypted.begin() + decrypted_length);
}

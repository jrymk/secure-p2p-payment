#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <openssl/aes.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>

#define PUBLIC_KEY_FILE "public.pem"
#define PRIVATE_KEY_FILE "private.pem"

// check if file exists
bool fileExists(const std::string &file) {
    std::ifstream fs(file.c_str());
    return fs.good();
}

// Error handling function
void handleOpenSSLError() {
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
}

void generateKeyPair(const std::string &privateKeyFile, const std::string &publicKeyFile) {
    RSA *rsa = RSA_new();
    BIGNUM *bne = BN_new();

    if (!BN_set_word(bne, RSA_F4)) { // RSA_F4 is the common exponent 65537
        std::cerr << "Error setting BIGNUM word: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        BN_free(bne);
        RSA_free(rsa);
        return;
    }

    if (!bne || !RSA_generate_key_ex(rsa, 2048, bne, NULL)) {
        std::cerr << "Error generating RSA key pair: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return;
    }

    // Save private key
    FILE *privFile = fopen(privateKeyFile.c_str(), "wb");
    if (privFile) {
        PEM_write_RSAPrivateKey(privFile, rsa, NULL, NULL, 0, NULL, NULL);
        fclose(privFile);
    } else {
        std::cerr << "Error saving private key to file." << std::endl;
    }

    // Save public key
    FILE *pubFile = fopen(publicKeyFile.c_str(), "wb");
    if (pubFile) {
        PEM_write_RSA_PUBKEY(pubFile, rsa);
        fclose(pubFile);
    } else {
        std::cerr << "Error saving public key to file." << std::endl;
    }

    RSA_free(rsa);
    BN_free(bne);
}

std::string loadKeyFromFile(const std::string &keyFile) {
    std::ifstream file(keyFile);
    if (!file.is_open()) {
        std::cerr << "Error opening key file." << std::endl;
        return "";
    }

    std::string key((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return key;
}

// try generate pair if not exist
void checkKeyFiles() {
    if (fileExists(PUBLIC_KEY_FILE) && fileExists(PRIVATE_KEY_FILE))
        return;
    std::remove(PUBLIC_KEY_FILE);
    std::remove(PRIVATE_KEY_FILE);
    generateKeyPair(PRIVATE_KEY_FILE, PUBLIC_KEY_FILE);

    std::cout << loadKeyFromFile(PUBLIC_KEY_FILE) << std::endl;
}

std::string keyToString(EVP_PKEY *pkey, bool isPrivate) {
    BIO *bio = BIO_new(BIO_s_mem());
    if (isPrivate) {
        PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL);
    } else {
        PEM_write_bio_PUBKEY(bio, pkey);
    }

    char *data;
    long len = BIO_get_mem_data(bio, &data);
    std::string key(data, len);
    BIO_free(bio);
    return key;
}

EVP_PKEY *stringToKey(const std::string &keyString, bool isPrivate) {
    BIO *bio = BIO_new_mem_buf(keyString.data(), keyString.size());
    EVP_PKEY *pkey = nullptr;
    if (isPrivate) {
        pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    } else {
        pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    }
    BIO_free(bio);
    return pkey;
}

std::string base64Encode(const std::vector<unsigned char> &input) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, input.data(), input.size());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string base64Str(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return base64Str;
}

std::vector<unsigned char> base64Decode(const std::string &input) {
    BIO *bio, *b64;
    char *buffer = new char[input.size()];
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(input.data(), input.size());
    bio = BIO_push(b64, bio);
    int decodedSize = BIO_read(bio, buffer, input.size());
    std::vector<unsigned char> decodedData(buffer, buffer + decodedSize);
    BIO_free_all(bio);
    delete[] buffer;
    return decodedData;
}

std::string encryptMessage(EVP_PKEY *publicKey, const std::string &message) {
    std::cerr << "Encrypting message: " << message << std::endl;

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(publicKey, NULL);
    if (!ctx) {
        std::cerr << "Error creating context for encryption: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return "";
    }

    if (EVP_PKEY_encrypt_init(ctx) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        std::cerr << "Error initializing encryption or setting padding: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    size_t outLen;
    std::vector<unsigned char> encryptedMessage(EVP_PKEY_size(publicKey));

    if (EVP_PKEY_encrypt(ctx, encryptedMessage.data(), &outLen, reinterpret_cast<const unsigned char *>(message.c_str()), message.length()) <= 0) {
        std::cerr << "Encryption failed: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    EVP_PKEY_CTX_free(ctx);
    encryptedMessage.resize(outLen); // Adjust size to actual encrypted output
    return base64Encode(encryptedMessage);
}

std::string decryptMessage(EVP_PKEY *privateKey, const std::string &encryptedMessage64) {
    // std::cerr << "Raw decrypt message with length " << encryptedMessage64.size() << ": ";
    // for (char c : encryptedMessage64) {
    //     if (c == '\r')
    //         std::cerr << "\\r";
    //     else if (c == '\n')
    //         std::cerr << "\\n";
    //     else
    //         std::cerr << c;
    // }
    // std::cerr << std::endl;

    std::vector<unsigned char> encryptedMessage(base64Decode(encryptedMessage64));

    // std::cerr << "Decrypting message: " << encryptedMessage64 << " with decoded size " << encryptedMessage.size() << std::endl;

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(privateKey, NULL);
    if (!ctx) {
        std::cerr << "Error creating context for decryption: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        return "";
    }

    if (EVP_PKEY_decrypt_init(ctx) <= 0 || EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
        std::cerr << "Error initializing decryption or setting padding: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    size_t outLen;
    std::vector<unsigned char> decryptedMessage(EVP_PKEY_size(privateKey));

    if (EVP_PKEY_decrypt(ctx, decryptedMessage.data(), &outLen, encryptedMessage.data(), encryptedMessage.size()) <= 0) {
        std::cerr << "Decryption failed: " << ERR_error_string(ERR_get_error(), NULL) << std::endl;
        EVP_PKEY_CTX_free(ctx);
        return "";
    }

    EVP_PKEY_CTX_free(ctx);
    decryptedMessage.resize(outLen); // Adjust size to actual decrypted output
    return std::string(decryptedMessage.begin(), decryptedMessage.end());
}

#endif // ENCRYPTION_H
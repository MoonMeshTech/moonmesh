#ifndef KEY_EXCHANGE_HEADER_GUARD
#define KEY_EXCHANGE_HEADER_GUARD

#include <map>
#include <memory>
#include <cstdint>
#include <mutex>
#include "utils/crypto.h"
#include "node.hpp"
#include "./msg_queue.h"
#include "key_exchange.pb.h"
// Struct definitions
struct ownkey_s {
    uint8_t ec_pub_key[EC_PUBLIC_KEY_LENGTH];
    uint8_t ec_priv_key[EC_PRIVATE_KEY_LENGTH];
    uint8_t salt[CRYPTO_SALT_LEN]; // Just used in key exchange
};

struct peerkey_s {
    uint8_t ec_pub_key[EC_PUBLIC_KEY_LENGTH];
    uint8_t aes_key[CRYPTO_AES_KEY_LEN];
    uint8_t salt[CRYPTO_SALT_LEN]; // Just used in key exchange
};

struct EcdhKey {
    ownkey_s own_key;
    peerkey_s peer_key;
    uint64_t timeout;
};

class KeyExchangeManager {
public:
    // Typedef for keyExchangeMap entry
    using KeyPtr = std::shared_ptr<EcdhKey>;

    // Interface functions
    void addKey(int fd, const EcdhKey& key);
    void removeKey(int fd);
    KeyPtr getKey(int fd);
    bool hasKey(int fd);

    bool key_calculate(const ownkey_s &ownkey, peerkey_s &peerkey);

    int keyExchangeRequest(Node &dest);
    int handleKeyExchangeRequest(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from);
    int processKeyExchangeAcknowledgment(const std::shared_ptr<KeyExchangeResponse> &keyExchangeAcknowledgementAck, const MsgData &from);

private:
    std::map<int, KeyPtr> keyExchangeMap;
    mutable std::shared_mutex mtx; // Mutex for protecting concurrent access to keyExchangeMap
};

int handleKeyExchangeRequest(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from);
int handleKeyExchangeAcknowledgment(const std::shared_ptr<KeyExchangeResponse> &keyExchangeAcknowledgementAck, const MsgData &from);

bool encrypt_plaintext(const peerkey_s &peerkey, const std::string &str_plaintext, Ciphertext &ciphertext);
bool decrypt_ciphertext(const peerkey_s &peerkey, const Ciphertext &ciphertext, std::string &plaintext);
bool verify_token(const uint8_t ecdh_pub_key[EC_PUBLIC_KEY_LENGTH], const Token &token);
bool generate_token(const uint8_t ecdh_pub_key[EC_PUBLIC_KEY_LENGTH], Token &token);

#endif // KEY_EXCHANGE_HEADER_GUARD
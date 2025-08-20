#include "key_exchange.h"
#include "peer_node.h"
#include "../include/logging.h"
#include "utils/magic_singleton.h"
#include "utils/hex_code.h"
#include "common/global_data.h"
#include "api.h"

bool decrypt_ciphertext(const peerkey_s &peerkey, const Ciphertext &ciphertext, std::string &plaintext)
{
    std::string str_plaintext(ciphertext.ciphertext_nbytes().size(), '\0');
    bool ret = crypto::aes_decrypt((unsigned char*)ciphertext.ciphertext_nbytes().data(),
                        ciphertext.ciphertext_nbytes().size(),
                        (unsigned char*)ciphertext.aes_tag_16bytes().data(),
                        peerkey.aes_key,
                        (unsigned char*)ciphertext.aes_iv_12bytes().data(),
                        (unsigned char*)&str_plaintext[0]);
    if (!ret) return false;

    plaintext = std::move(str_plaintext);
    return true;
}

bool encrypt_plaintext(const peerkey_s &peerkey, const std::string &str_plaintext, Ciphertext &ciphertext)
{
    std::string ciphertextStr(str_plaintext.size(), '\0');
    uint8_t rand_iv[CRYPTO_AES_IV_LEN];
    uint8_t aes_tag[AES_TAG_LENGTH];


    if (!crypto::rand_salt(rand_iv, CRYPTO_AES_IV_LEN))
    {
        return false;
    }

    bool ret = crypto::aes_encrypt((unsigned char *)str_plaintext.data(), str_plaintext.size(),
                                   peerkey.aes_key, rand_iv, (unsigned char *)&ciphertextStr[0], aes_tag);
    if (!ret)
    {
        return false;
    }

    ciphertext.set_cipher_version(CRYPTO_VERSION);
    ciphertext.set_aes_iv_12bytes(rand_iv, CRYPTO_AES_IV_LEN);
    ciphertext.set_aes_tag_16bytes(aes_tag, AES_TAG_LENGTH);
    ciphertext.set_ciphertext_nbytes(std::move(ciphertextStr));

    return true;
}

bool verify_token(const uint8_t ecdh_pub_key[EC_PUBLIC_KEY_LENGTH], const Token &token)
{
    uint8_t hmac_256[CRYPTO_HMAC_SHA256];

    bool ret = crypto::hmac_sha256(hmac_256,
         (uint8_t *)token.salt_3bytes().data(), token.salt_3bytes().size(),
         ecdh_pub_key, EC_PUBLIC_KEY_LENGTH);
    if (!ret)
    {
        ERRORLOG("hmac calculation error.");
        return false;
    }

    if (0 != memcmp(token.hmac_3bytes().data(), hmac_256, 3))
    {
        ERRORLOG("Token check failed.");
        return false;
    }

    return true;
}

bool generate_token(const uint8_t ecdh_pub_key[EC_PUBLIC_KEY_LENGTH], Token &token)
{
    uint8_t random_digit[3];
    uint8_t hmac_256[CRYPTO_HMAC_SHA256];


    if (!crypto::rand_salt(random_digit, 3))
    {
        ERRORLOG("random digit generation error.");
        return false;
    }

    if (!crypto::hmac_sha256(hmac_256, random_digit, 3, ecdh_pub_key, EC_PUBLIC_KEY_LENGTH))
    {
        ERRORLOG("hmac calculation error.");
        return false;
    }

    token.set_salt_3bytes(random_digit, 3);
    token.set_hmac_3bytes(hmac_256, 3);

    return true;
}


bool KeyExchangeManager::key_calculate(const ownkey_s &ownkey, peerkey_s &peerkey)
{
    /* XOR the ownkey and peerkey to one array */
    uint8_t saltXorValue[CRYPTO_SALT_LEN];
    if (!crypto::bytes_xor(ownkey.salt, sizeof(ownkey_s::salt), peerkey.salt, sizeof(peerkey_s::salt), saltXorValue))
    {
        ERRORLOG("xor calculation error.");
        return false;
    }

    /* Calculate the shared key using own public and private keys and the public key of the other party */
    uint8_t shared_key[ECDH_SHARED_KEY_LENGTH];
    if (!crypto::calculate_ecdh_shared_key(ownkey.ec_pub_key, ownkey.ec_priv_key, peerkey.ec_pub_key, shared_key))
    {
        ERRORLOG("shared key calculation error.");
        return false;
    }

    /* Using HKDF to calculate the final AES key */
    if (!crypto::createHkdfBytes(shared_key, saltXorValue, (uint8_t*)CRYPTO_KEY_INFO, strlen(CRYPTO_KEY_INFO), peerkey.aes_key))
    {
        ERRORLOG("hkdf calculation error.");
        return false;
    }
    return true;
}

void KeyExchangeManager::addKey(int fd, const EcdhKey& key) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    KeyPtr keyPtr = std::make_shared<EcdhKey>(key);
    keyExchangeMap[fd] = keyPtr;
}

void KeyExchangeManager::removeKey(int fd) {
    std::unique_lock<std::shared_mutex> lock(mtx);
    keyExchangeMap.erase(fd);
}

KeyExchangeManager::KeyPtr KeyExchangeManager::getKey(int fd) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    if (hasKey(fd)) {
        return keyExchangeMap[fd];
    }
    return nullptr;
}

bool KeyExchangeManager::hasKey(int fd) {
    std::shared_lock<std::shared_mutex> lock(mtx);
    return keyExchangeMap.find(fd) != keyExchangeMap.end();
}

int KeyExchangeManager::keyExchangeRequest(Node &dest)
{
    EcdhKey key;

    // connect
    int connectReturn = MagicSingleton<PeerNode>::GetInstance()->ConnectNode(dest);
	if (connectReturn != 0)
	{
		ERRORLOG(" connect_node error ip:{} ret{}", IpPort::IpSz(dest.publicIp),connectReturn);
        return -1;
	}

    if (!crypto::generate_ecdh_keys(key.own_key.ec_pub_key, key.own_key.ec_priv_key))
    {
        ERRORLOG("ECDH-KEY generation failed.");
        return -2;
    }
    if (!crypto::rand_salt(key.own_key.salt, sizeof(ownkey_s::salt)))
    {
        ERRORLOG("Random salt generation failed.");
        return -3;
    }

    KeyExchangeRequest request;
    KeyInfo           *key_info = new KeyInfo();

    key_info->set_ec_public_key_65bytes(key.own_key.ec_pub_key, sizeof(ownkey_s::ec_pub_key));
    key_info->set_salt_32bytes(key.own_key.salt, sizeof(ownkey_s::salt));

    std::string str_request;
    std::string str_response;
    request.set_allocated_key_info(key_info);

    std::string msg_id;
    if (!dataMgrPtr.CreateWait(3, 1, msg_id))
    {
        return -4;
    }

    request.set_msg_id(msg_id);

    std::string ipPort = std::to_string(dest.publicIp) + ":" + std::to_string(dest.publicPort);
    if(!dataMgrPtr.AddResNode(msg_id, ipPort))
	{
		return -5;
	}

    net_com::sendEcdhMessageRequest(dest, request);

    std::vector<std::string> ret_datas;
    if (!dataMgrPtr.WaitData(msg_id, ret_datas))
    {
        if (ret_datas.empty())
        {
            ERRORLOG("wait startRegistrationNode time out send:{} recv:{}", 1, ret_datas.size());
            return -6;
        }
    }

    if(ret_datas.size() > 1)
    {
        return -7;
    }

    KeyExchangeResponse Response;
    if (!Response.ParseFromString(ret_datas.front()))
    {
        return -8;
    }

    if ((Response.key_info().salt_32bytes().size() != 32) || (Response.key_info().ec_public_key_65bytes().size() != 65))
    {
        ERRORLOG("Key length does not match.");
        return -9;
    }

    memcpy(key.peer_key.ec_pub_key, Response.key_info().ec_public_key_65bytes().data(), Response.key_info().ec_public_key_65bytes().size());
    memcpy(key.peer_key.salt, Response.key_info().salt_32bytes().data(), Response.key_info().salt_32bytes().size());

    if (!key_calculate(key.own_key, key.peer_key))
    {
        return -10;
    }

    addKey(dest.fd, key);
    return 0;
}
int KeyExchangeManager::handleKeyExchangeRequest(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from)
{
    if ((keyExchangeReq->key_info().ec_public_key_65bytes().size() != EC_PUBLIC_KEY_LENGTH)
        || (keyExchangeReq->key_info().salt_32bytes().size() != CRYPTO_SALT_LEN))
    {
        return -1;
    }

    EcdhKey key;

    memcpy(key.peer_key.ec_pub_key,
            keyExchangeReq->key_info().ec_public_key_65bytes().data(),
            keyExchangeReq->key_info().ec_public_key_65bytes().size());
    memcpy(key.peer_key.salt,
            keyExchangeReq->key_info().salt_32bytes().data(),
            keyExchangeReq->key_info().salt_32bytes().size());

    KeyInfo *key_info = new KeyInfo();

    crypto::generate_ecdh_keys(key.own_key.ec_pub_key, key.own_key.ec_priv_key);

    crypto::rand_salt(key.own_key.salt, sizeof(ownkey_s::salt));

    if (!key_calculate(key.own_key, key.peer_key))
    {
        return -2;
    }

    addKey(from.fd, key);
    
    key_info->set_ec_public_key_65bytes(key.own_key.ec_pub_key, EC_PUBLIC_KEY_LENGTH);
    key_info->set_salt_32bytes(key.own_key.salt, CRYPTO_SALT_LEN);

    KeyExchangeResponse key_exchange_response;
    key_exchange_response.set_allocated_key_info(key_info);
    key_exchange_response.set_msg_id(keyExchangeReq->msg_id());
    net_com::SendMessage(from, key_exchange_response, net_com::Compress::COMPRESS_TRUE, net_com::Encrypt::ENCRYPT_FALSE, net_com::Priority::kHighPriorityLevel2);
    return 0;
}

int handleKeyExchangeRequest(const std::shared_ptr<KeyExchangeRequest> &keyExchangeReq, const MsgData &from)
{
    MagicSingleton<KeyExchangeManager>::GetInstance()->handleKeyExchangeRequest(keyExchangeReq, from);
    return 0;
}

int handleKeyExchangeAcknowledgment(const std::shared_ptr<KeyExchangeResponse> &keyExchangeAcknowledgementAck, const MsgData &from)
{
    std::string ipPort = std::to_string(from.ip) + ":" + std::to_string(from.port);
    dataMgrPtr.waitDataToAdd(keyExchangeAcknowledgementAck->msg_id(), ipPort, keyExchangeAcknowledgementAck->SerializeAsString());
    return 0;
}
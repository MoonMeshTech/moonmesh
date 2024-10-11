#include "utxo_core.h"
#include "algorithm.h"
#include "block_helper.h"
#include "check_error.h"
#include "common/global_data.h"
#include "db_api.h"
#include "google/protobuf/util/json_util.h"
#include "net/peer_node.h"
#include "sync_block.h"
#include "transaction.h"
#include "transaction.pb.h"
#include "transaction/expected.h"
#include "transaction/utxo_error.h"
#include "txhelper.h"
#include "utils/account_manager.h"
#include "utils/tmp_log.h"

namespace mmc {

mmc::Expected<bool> UtxoCore::signUtxo(const mmc::CowString &addr,
                                       CTxUtxos *utxo) {
  mmc::CowString serializeTransactionUtxoRequest =
      Getsha256hash(utxo->SerializeAsString());

  mmc::Expected<SingRe> singre = sign(addr, serializeTransactionUtxoRequest);

  if (singre.is_error()) {
    return UtxoError(UE_SING_UTXO_FAIL)
        .trace(singre.get_error(), EMATE("addr:%s", addr.str()));
  }
  SingRe ms = singre.unwrap();
  CSign *multiSign = utxo->add_multisign();
  multiSign->set_sign(ms.signature.str());
  multiSign->set_pub(ms.pubkey.str());
  return true;
}

void UtxoCore::print_debug_tx(CTransaction *tx_) {
  if (tx_ == nullptr) {
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;
    options.preserve_proto_field_names = true;
    std::string jsonString;
    auto status =
        google::protobuf::util::MessageToJsonString(tx, &jsonString, options);
    if (!status.ok()) {
      ERRORLOG("Failed to convert protobuf message to JSON: {}",
               status.ToString());
      return;
    }
    std::cout << "Transaction JSON:\n" << jsonString << std::endl;
  } else {
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true;
    options.always_print_primitive_fields = true;
    options.preserve_proto_field_names = true;
    std::string jsonString;
    auto status =
        google::protobuf::util::MessageToJsonString(*tx_, &jsonString, options);
    if (!status.ok()) {
      ERRORLOG("Failed to convert protobuf message to JSON: {}",
               status.ToString());
      return;
    }
    std::cout << "Transaction JSON:\n" << jsonString << std::endl;
  }
}

void UtxoCore::createUtxoTransferTo(CTxUtxos *utxo,
                                    const TransferToParam &param) {
  CTxOutput *output = utxo->add_vout();
  output->set_addr(param.to.str());
  output->set_value(param.amount);
}

mmc::Expected<int64_t>
UtxoCore::createUtxoTransferFrom(CTxUtxos *utxo, const TransferFromParam &param,
                                 int64_t sq) {
  FindUtxoParam findParam;
  findParam.addr = param.from;
  findParam.asset_type = param.asset_type;
  findParam.need_amount = param.amount;

  uint64_t balance = 0;

  utxo->set_assettype(param.asset_type.str());

  std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator>
      outputUtxosSet;

  std::multimap<std::string, std::string> fromAddr_assetType;
  fromAddr_assetType.insert({param.from.str(), param.asset_type.str()});
  int ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, balance,outputUtxosSet, param.is_find_utxo);
  if (ret != 0) {
    ERRORLOG( "FindUtxo failed! The error code is {} " , ret);
    std::cout << Sutil::Format("addr:%s,assettype:%s,ret:%s,is_find_utxo:%s", param.from.str(),param.asset_type.str(),ret,param.is_find_utxo) << std::endl;
    return RE(UtxoError,UE_FIND_UTXO_FAIL,"addr:%s,asset_type:%s",param.from.str(),param.asset_type.str());
  }



  if (outputUtxosSet.empty()) {
    ERRORLOG( "Utxo is empty!" );
     return RE(UtxoError,UE_FIND_UTXO_FAIL);
  }
  utxo->set_assettype(param.asset_type.str());
  //  Fill Vin
  std::set<std::string> setTxOwners;
  for (auto &utxo : outputUtxosSet) {
    setTxOwners.insert(utxo.addr);
  }

  if (setTxOwners.size() != 1) {
      ERRORLOG( "Tx owner is invalid!" );
      return RE(UtxoError,UE_FIND_UTXO_FAIL);
  }

  for (auto &owner : setTxOwners) {
    utxo->add_owner(owner);
    uint32_t n = sq;
    CTxInput *vin = utxo->add_vin();
    for (auto &utxo : outputUtxosSet) {
      if (owner == utxo.addr) {
        CTxPrevOutput *prevOutput = vin->add_prevout();
        prevOutput->set_hash(utxo.hash);
        prevOutput->set_n(utxo.n);
      }
    }
    vin->set_sequence(n++);

 
  }

  
  return balance;
}

mmc::Expected<bool> UtxoCore::signTx(const mmc::CowString &addr) {
  for (int i = 0; i < tx.utxos_size(); ++i) {
    int index = 0;
    auto utxo = tx.mutable_utxos(i);
    auto vin = utxo->mutable_vin();
    for (auto &owner : utxo->owner()) {
      // vin sign

      if (utxo->vin_size() == 0) {
         // std::cout << "vin_size is 0" << std::endl;
          continue;
      }
      auto vin_t = vin->Mutable(index);
      if (!vin_t->contractaddr().empty()) {
          index++;
          std::cout << "contractaddr is not empty" << std::endl;
          continue;
      }

      std::string vinHashSerialized = Getsha256hash(vin_t->SerializeAsString());
      std::string signature;
      std::string pub;
      auto ret = sign(owner, vinHashSerialized);
     // std::cout << "signature12333:" << ret.unwrap().signature.str() << std::endl;
      if (ret.is_error()) {
        return ret.get_error();
      }

      CSign *vinSign = vin_t->mutable_vinsign();
      vinSign->set_sign(ret.unwrap().signature.str());
      vinSign->set_pub(ret.unwrap().pubkey.str());
      index++;
    }
    /// utxo sign
    CTxUtxos copyTransactionUtxo = *utxo;
    copyTransactionUtxo.clear_multisign();
    std::set<std::string> setTxOwners;


    for( auto &owner : utxo->owner()) {
      setTxOwners.insert(owner);
    }

    for (auto &owner : setTxOwners) {
      {
        std::string serializeTransactionUtxoRequest =
            Getsha256hash(copyTransactionUtxo.SerializeAsString());
        std::string signature;
        std::string pub;
        auto ret = sign(owner, serializeTransactionUtxoRequest);
        if (ret.is_error()) {
          return ret.get_error();
        }
        CSign *multiSign = utxo->add_multisign();
        multiSign->set_sign(ret.unwrap().signature.str());
        multiSign->set_pub(ret.unwrap().pubkey.str());
      }
    }


  }
  tx.clear_hash();
  tx.clear_signature();
  std::string txHash = Getsha256hash(tx.SerializeAsString());
  tx.set_hash(txHash);
  return true;
}

mmc::Expected<int64_t> UtxoCore::createUtxoGas(CTransaction *tx,
                                               const UtxoGasParam &param) {
  CTxUtxos *u = tx->add_utxos();
  TransferFromParam VinParam;
  VinParam.from = param.from;
  VinParam.amount = param.gas;
  VinParam.asset_type = param.asset_type;
  auto BalnaceRet = createUtxoTransferFrom(u, VinParam, 0);
  if (BalnaceRet.is_error()) {
    return BalnaceRet.get_error();
  }

  u->set_gas(global::ca::UtxoGasType::UTXO_GAS_TYPE_TRUE); 
  tx->set_independence(global::ca::TX_INDEPENDENCE_TRUE);  

  TransferToParam to_self;
  TransferToParam to_burn;

  to_self.amount = BalnaceRet.unwrap() - param.gas;
  to_self.to = param.from;

  to_burn.amount = param.gas;
  to_burn.to = global::ca::VIRTUAL_BURN_GAS_ADDR;

  createUtxoTransferTo(u, to_self);
  createUtxoTransferTo(u, to_burn);

  // auto singre = signUtxo(param.from, u);

  // if (singre.is_error()) {
  //   return singre.get_error();
  // }
  return 0;
}


mmc::Expected<int64_t> UtxoCore::createUtxoGasBylameda(CTransaction *tx,
                                               const UtxoGasParam &param,const std::function<uint64_t(CTxUtxos *u,int index)> & gasLamda) {
  CTxUtxos *u = tx->add_utxos();
  TransferFromParam VinParam;
  VinParam.from = param.from;
  //VinParam.amount = param.gas;
  VinParam.asset_type = param.asset_type;
  auto BalnaceRet = createUtxoTransferFrom(u, VinParam, 0);
  if (BalnaceRet.is_error()) {
    return BalnaceRet.get_error();
  }

  u->set_gas(global::ca::UtxoGasType::UTXO_GAS_TYPE_TRUE); 
  tx->set_independence(global::ca::TX_INDEPENDENCE_TRUE);  

  TransferToParam to_self;
  TransferToParam to_burn;


  uint64_t gas=gasLamda(u,0);

  to_self.amount = BalnaceRet.unwrap() - gas;
  to_self.to = param.from;

  to_burn.amount =gas;
  to_burn.to = global::ca::VIRTUAL_BURN_GAS_ADDR;

  createUtxoTransferTo(u, to_self);
  createUtxoTransferTo(u, to_burn);


  return 0;
}


mmc::Expected<int64_t>
UtxoCore::getStakeAmount(const mmc::CowString &addr,
                         const mmc::CowString &assetType) {
  DBReader dbReader;
  std::vector<std::string> utxos;
  auto status = dbReader.getStakeUtxoByAddr(addr.str(), assetType.str(), utxos);
  //dbReader.getStakeUtxoByAddr(const std::string &address, const std::string &assetType, std::vector<std::string> &utxos)
  if (DBStatus::DB_SUCCESS != status) {
    return 0;
  }
  int64_t total = 0;
  for (auto &item : utxos) {
    std::string txRawData;
    if (DBStatus::DB_SUCCESS !=
        dbReader.getTransactionByHash(item, txRawData)) {
      continue;
    }
    CTransaction utxoTx;
    utxoTx.ParseFromString(txRawData);

    nlohmann::json data = nlohmann::json::parse(utxoTx.data());
    nlohmann::json txInfo = data["txinfo"].get<nlohmann::json>();
    std::string txStakeTypeNet = txInfo["StakeType"].get<std::string>();

    if (txStakeTypeNet != global::ca::STAKE_TYPE_NET) {
      continue;
    }

    for (auto &utxo : utxoTx.utxos()) {
      for (int i = 0; i < utxo.vout_size(); i++) {
        CTxOutput txout = utxo.vout(i);
        if (txout.addr() == global::ca::kTargetAddress) {
          total += txout.value();
        }
      }
    }
  }
  return total;
}

mmc::Expected<std::vector<TxHelper::Utxo>>
UtxoCore::findUtxoByAddrAndCheckBalance(const FindUtxoParam &param,
                                        int64_t &_total) {

  DBReader dbReader;
  std::vector<TxHelper::Utxo> Utxos;
  std::vector<TxHelper::Utxo> retUtxos;
  if (!isValidAddress(param.addr.str())) {
    return CheckError(CHECK_ERROR_NOT_VALID_ADDR)
        .trace(EMATE("addr:%s", param.addr.str()));
  }
  std::vector<std::string> utxoHashesList;
  if (DBStatus::DB_SUCCESS !=
          dbReader.getUtxoHashsByAddress(
              param.addr.str(), param.asset_type.str(), utxoHashesList) ||
      utxoHashesList.empty()) {
    return UtxoError(UE_ASSET_NOT_EXIST).trace(EMATE("k"));
  }

  std::sort(utxoHashesList.begin(), utxoHashesList.end());
  utxoHashesList.erase(
      std::unique(utxoHashesList.begin(), utxoHashesList.end()),
      utxoHashesList.end());

  for (const auto &hash : utxoHashesList) {
    TxHelper::Utxo utxo;
    utxo.hash = hash;
    utxo.addr = param.addr.str();
    utxo.value = 0;
    utxo.n = 0; //	At present, the n of utxo is all 0

    std::string balance;
    if (dbReader.getUtxoValueByUtxoHashes(
            hash, param.addr.str(), param.asset_type.str(), balance) != 0) {
      UtxoError(UE_NOT_FIND_UTXO_BY_HASH).trace(EMATE("hash:%s", hash));
      continue;
    }

    utxo.value = TxHelper::calculate_utxo_value(balance);
    Utxos.push_back(utxo);
  }

  int total = 0;

  if (retUtxos.size() < UTxoRulers::max_vin_size) {
    //  Fill other positions with non-0
    auto it = Utxos.begin();
    while (it != Utxos.end()) {
      if (retUtxos.size() == UTxoRulers::max_vin_size) {
        break;
      }
      total += it->value;

      if (total >= param.need_amount) {
        break;
      }

      retUtxos.push_back(*it);
      ++it;
    }
  }

  if (total < param.need_amount) {
    return UtxoError(UE_INSUFFICIENT_ASSET_TYPE)
        .trace(EMATE("amout:%s", param.need_amount));
  }
  // const std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator>
  // outputUtxosSet; if(param.needWidUtxo)
  // {
  // 	int ret = sendConfirmUtxoHashRequest(outputUtxosSet);
  // 	if(ret != 0)
  // 	{
  // 		ERRORLOG("sendConfirmUtxoHashRequest error ret : {}", ret);
  // 		return -2;
  // 	}
  // }

  _total = total;
  return retUtxos;
}

mmc::Expected<UtxoCore::SingRe> UtxoCore::sign(const mmc::CowString &addr,
                                               const mmc::CowString &data) {
  UtxoCore::SingRe retS;
  std::string signature;
  std::string pub;
 // std::cout << "addr ____ :" << addr.c_str() << std::endl;
  int ret = TxHelper::Sign(addr.c_str(), data.c_str(), signature, pub);
  if (ret != 0) {
    return UtxoError(UE_SIGN_VIN_FAIL).trace(EMATE("k"));
  }
 // std::cout << "signature ____ :" << signature << std::endl;
  retS.signature = signature;
  retS.pubkey = pub;
  return retS;
}
std::string UtxoCore::calculate_all_hash(CTransaction *tx) {
  std::string allHash;
  for (const auto &txUtxo : tx->utxos()) {
    for (int i = 0; i < txUtxo.vin_size(); i++) {
      auto vin = txUtxo.vin(i);
      for (int j = 0; j < vin.prevout_size(); j++) {
        auto txPrevOutput = vin.prevout(j);
        allHash += txPrevOutput.hash();
      }
    }
  }
  allHash += std::to_string(tx->time());
  return allHash;
}

mmc::Expected<uint64_t> UtxoCore::transactionDiscoveryHeight() {
  uint64_t top;
  if (!BlockHelper::getChainHeight(top)) {
    ERRORLOG("getChainHeight get top failed!!")
    return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-1"));
  }

  DEBUGLOG("current top: {}", top);
  DBReader dbReader;
  uint64_t nodeSelfHeight = 0;
  auto status = dbReader.getBlockTop(nodeSelfHeight);
  if (DBStatus::DB_SUCCESS != status) {
    DEBUGLOG("GetBlockTop fail!!!");
    return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-2"));
  }

  if (nodeSelfHeight >= top) {
    return nodeSelfHeight;
  }

  std::vector<std::string> pledgeAddr;

  std::vector<Node> nodelist =
      MagicSingleton<PeerNode>::GetInstance()->GetNodelist();
  for (const auto &node : nodelist) {
    bool canReward = VerifyBonusAddr(node.address);

    // int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(
    //     node.address, global::ca::StakeType::STAKE_TYPE_NODE);
    if (canReward) {
      pledgeAddr.push_back(node.address);
    }
  }

  uint64_t endSyncHeight_ = nodeSelfHeight - 3;
  std::string msgId;
  if (!dataMgrPtr.CreateWait(90, pledgeAddr.size() * 0.8, msgId)) {
    return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-3"));
    return -3;
  }
  for (auto &nodeId : pledgeAddr) {
    if (!dataMgrPtr.AddResNode(msgId, nodeId)) {
      ERRORLOG("transactionDiscoveryHeight AddResNode error");
      return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-4"));
    }
    DEBUGLOG("Find the transaction height to {}", nodeId);
    sendSyncGetHeightHashRequest(nodeId, msgId, nodeSelfHeight, endSyncHeight_);
  }
  std::vector<std::string> retDatas;
  if (!dataMgrPtr.WaitData(msgId, retDatas)) {
    if (retDatas.size() < pledgeAddr.size() * 0.5) {
      ERRORLOG("wait sync block hash time out send:{} recv:{}",
               pledgeAddr.size(), retDatas.size());
      return UtxoError(UE_GET_TOP_FAIL)
          .trace(EMATE("wait sync block hash time out send:%s recv:%s -5",
                       pledgeAddr.size(), retDatas.size()));
    }
  }

  uint64_t successCounter = 0;
  SyncGetHeightHashAck ack;
  std::map<std::string, std::set<std::string>> syncBlockHashList;
  for (auto &retData : retDatas) {
    ack.Clear();
    if (!ack.ParseFromString(retData)) {
      continue;
    }
    if (ack.code() != 0) {
      continue;
    }
    successCounter++;
    for (auto &key : ack.block_hashes()) {
      auto it = syncBlockHashList.find(key);
      if (syncBlockHashList.end() == it) {
        syncBlockHashList.insert(std::make_pair(key, std::set<std::string>()));
      }
      auto &value = syncBlockHashList.at(key);
      value.insert(ack.self_node_id());
    }
  }

  if (successCounter < (size_t)(retDatas.size() * 0.66)) {
    ERRORLOG("ret data error successCounter:{}, (uint32_t)(retDatas * "
             "0.66):{}, retDatas.size():{}",
             successCounter, (size_t)(retDatas.size() * 0.66), retDatas.size());
    return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-6"));
  }

  size_t verifyNum = successCounter / 5 * 4;
  std::vector<std::string> hashes;
  std::string strblock;
  CBlock block;
  uint64_t highestHeight = 0;
  for (auto &syncBlockHash : syncBlockHashList) {
    strblock.clear();
    auto res = dbReader.getBlockByBlockHash(syncBlockHash.first, strblock);
    if (DBStatus::DB_NOT_FOUND != res) {
      continue;
    }
    if (syncBlockHash.second.size() < verifyNum) {
      continue;
    }

    hashes.push_back(syncBlockHash.first);

    if (!block.ParseFromString(strblock)) {
      return UtxoError(UE_GET_TOP_FAIL).trace(EMATE("-7"));
    }
    highestHeight =
        highestHeight > block.height() ? highestHeight : block.height();
  }
  return highestHeight;
}

int64_t UtxoCore::GetChainId() {
  static int64_t chainId = 0;
  if (chainId != 0) {
    return chainId;
  }

  std::string blockHash;
  DBReader dbReader;
  if (DBStatus::DB_SUCCESS !=
      dbReader.getBlockHashByBlockHeight(0, blockHash)) {
    ERRORLOG("fail to read genesis block hash")
    return -1;
  }

  std::string genesisBlockPrefix = blockHash.substr(0, 8);
  chainId = StringUtil::StringToNumber(genesisBlockPrefix);
  return chainId;
}

int UtxoCore::checkGasAssetsQualification(
    const std::pair<std::string, std::string> &gasAssets,
    const uint64_t &height) {
  std::vector<std::string> gasTradeFromAddress;
  gasTradeFromAddress.push_back(gasAssets.first);
  int ret = TxHelper::Check(gasTradeFromAddress, gasAssets.second, height);
  if (ret != 0) {
    ERRORLOG("Check parameters failed! The error code is {}.", ret);
    ret -= 600;
    return ret;
  }
  return 0;
}
// namespace mmc
} // namespace mmc  
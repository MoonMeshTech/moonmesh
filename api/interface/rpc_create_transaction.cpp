#include "rpc_create_transaction.h"

#include "api/interface/rpc_tx.h"
#include "api/rpc_error.h"

#include "ca/global.h"
#include "ca/txhelper.h"
#include "ca/contract.h"
#include "ca/algorithm.h"
#include "ca/transaction.h"
#include "ca/block_helper.h"
#include "ca/double_spend_cache.h"
#include "ca/block_helper.h"
#include "ca/test.h"
#include <nlohmann/json.hpp>
#include "utils/console.h"
#include "utils/tmp_log.h"
#include "utils/time_util.h"
#include "utils/string_util.h"
#include "utils/bench_mark.h"
#include "utils/account_manager.h"
#include "utils/magic_singleton.h"
#include "utils/contract_utils.h"
#include "utils/hex_code.h"

#include "include/logging.h"

#include "db/db_api.h"
#include "ca/evm/evmEnvironment.h"
#include "ca/evm/evm_manager.h"
#include "ca.h"


int CheckFromaddr(const std::vector<std::string>& fromAddr)
{

	if(fromAddr.empty())
	{
		ERRORLOG("Fromaddr is empty!");		
		return -1;
	}

	std::vector<std::string> source_addr_temp = fromAddr;
	std::sort(source_addr_temp.begin(),source_addr_temp.end());
	auto iter = std::unique(source_addr_temp.begin(),source_addr_temp.end());
	source_addr_temp.erase(iter,source_addr_temp.end());
	if(source_addr_temp.size() != fromAddr.size())
	{
		ERRORLOG("Fromaddr have duplicate elements!");		
		return -2;
	}

	DBReader dbReader;
	std::map<std::string, std::vector<std::string>> identities;

	for(auto& from : fromAddr)
	{
		if (!isValidAddress(from))
		{
			ERRORLOG("Fromaddr is a non  address! from:{}", from);
			return -3;
		}
	}

	return 0;
}


void createOrReplaceTransaction(const std::vector<TxHelper::TransTable> txAsset,const std::pair<std::string,std::string>& gasTrade,
                                bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	MagicSingleton<Benchmark>::GetInstance()->increase_transaction_initiate_amount();
	std::vector<TxHelper::Utxo> allsetOutUtxos;
	DBReader dbReader;
    uint64_t height = 0;
	CTransaction outTx;
	uint64_t totalGas = 0;
	std::vector<std::string> doubleSpendUtxos;
	if(encodedInfo.size() > 1024)
	{
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-7;
		ERRORLOG("The information entered exceeds the specified length");
		return;
	}

    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message=" db get top failed!!";
		ackT->code=-1;
		ERRORLOG("db get top failed!!");
        return; 
    }
	//  Check parameters
	height += 1;

	for(const auto & i : txAsset)
	{
		std::string tempstring = i.fromAddr[0];
		tempstring += i.assetType;
		doubleSpendUtxos.push_back(tempstring);
		int ret = TxHelper::Check(i.fromAddr, i.assetType, height);
		if (ret != 0)
		{
			ackT->message="Check parameters failed!";
			ackT->code=-2;
			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			ERRORLOG("Check parameters failed! The error code is " + std::to_string(ret-100));
			return; 
		}

		if(i.toAddrAmount.empty())
		{
			ackT->message="to addr is empty";
			ackT->code=-3;
			ERRORLOG("to addr is empty");
			return;
		}	

		
		for (auto &addr : i.toAddrAmount)
		{
			if (!isValidAddress(addr.first))
			{
				ackT->message="To address is not  address!";
				ackT->code=-4;
				ERRORLOG("To address is not  address!");
				return;
			}

			for (auto& from : i.fromAddr)
			{
				if (addr.first == from)
				{
					ackT->message="from address is same with toaddr";
					ackT->code=-5;
					ERRORLOG("from address is same with toaddr");
					return;
				}
			}
			
			if (addr.second <= 0)
			{
				ackT->message="Value is zero!";
				ackT->code=-6;
				ERRORLOG("Value is zero!");
				return;
			}
		}

		

		uint64_t amount = 0;// Transaction fee
		for (auto& i : i.toAddrAmount)
		{
			amount += i.second;    
		}
		uint64_t expend = amount;

		//  Find utxo
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;

		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({i.fromAddr[0], i.assetType});

		ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);

		if (ret != 0)
		{
			ackT->message="FindUtxo failed!";
			ackT->code=-8;
			ERRORLOG("FindUtxo failed! The error code is " + std::to_string(ret-200));
			return; 
		}
		if (outputUtxosSet.empty())
		{
			ackT->message="Utxo is empty!";
			ackT->code=-9;
			ERRORLOG("Utxo is empty!");
			return;
		}
		for(auto& utxo: outputUtxosSet)
		{
			allsetOutUtxos.push_back(utxo);
		}
		
		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(i.assetType);
		
		//  Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ackT->message="Tx owner is empty!";
			ackT->code=-10;
			ERRORLOG("Tx owner is empty!");
			return;
		}

		uint32_t n = 0;
		for (auto & owner : setTxOwners)
		{
			transactionUtxo1->add_owner(owner);
			CTxInput * vin = transactionUtxo1->add_vin();
			for (auto & utxo : outputUtxosSet)
			{
				if (owner == utxo.addr)
				{
					CTxPrevOutput * prevOutput = vin->add_prevout();
					prevOutput->set_hash(utxo.hash);
					prevOutput->set_n(utxo.n);
				}
			}
			vin->set_sequence(n++);
		}

		outTx.set_data("");
		outTx.set_info(encodedInfo);
		outTx.set_type(global::ca::kTxSign);

		uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
		uint64_t gas = 0;

		std::map<std::string, int64_t> targetAddrs = i.toAddrAmount;
		targetAddrs.insert(make_pair(*(i.fromAddr.rbegin()), total - expend));
		targetAddrs.insert(make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
		if (GenerateGas(*transactionUtxo1, global::ca::kTxSign, targetAddrs.size(), gasSize, gas) != 0)
		{
			ackT->message="Generate Gas failed!";
			ackT->code=-11;
			ERRORLOG("Generate Gas failed!");
			return;
		}
		if(global::ca::GetInitAccountAddr() == i.fromAddr[0]){
        	gas = 0;
    	}

		if (isGasTrade)
		{
			totalGas += gas;
		}
		else
		{
			expend += gas;
		}
		ackT->gas += std::to_string(gas);
		//Judge whether utxo is enough
		if(total < expend)
		{
			ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend)+"gas is " +std::to_string(gas)+"toAddr amount is "+std::to_string(amount);
			ackT->code=-12;
			ERRORLOG("{}", ackT->message);
			return; 
		}
		// fill vout
		for (auto &to : i.toAddrAmount)
		{
			CTxOutput * vout = transactionUtxo1->add_vout();
			vout->set_addr(to.first);
			vout->set_value(to.second);
		}
		CTxOutput * voutSourceAddress = transactionUtxo1->add_vout();
		voutSourceAddress->set_addr(*(i.fromAddr.rbegin()));
		voutSourceAddress->set_value(total - expend);
		
		if (isGasTrade)
		{
			CTxOutput *voutBurnAmount = transactionUtxo1->add_vout();
			voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
			voutBurnAmount->set_value(0);
		}
		else
		{
			CTxOutput *voutBurnAmount = transactionUtxo1->add_vout();
			voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
			voutBurnAmount->set_value(gas);
		}

	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ackT->code = -13;
			ackT->message = "Check parameters failed! The error code is" + std::to_string(ret);	
			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ackT->code = -14;
			ackT->message = "FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ackT->code = -15;
			ackT->message = "Utxo is empty!";;	
			return ;
		}

		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ackT->code = -16;
			ackT->message = "Tx owner is empty!";
			return ;
		}
		for(auto& utxo: outputUtxosSet)
		{
			allsetOutUtxos.push_back(utxo);
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}

	TxHelper::vrfAgentType type;
	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ackT->time = std::to_string(currentTime);
	DEBUGLOG("getTransactionStartIdentity tx time = {}, package = {}", outTx.time(), outTx.identity());
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	DEBUGLOG("getTransactionStartIdentity currentTime = {} type = {}",currentTime ,type);

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION); 
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::TX_TYPE_TX);
	// Determine whether dropshipping is default or local dropshipping

	
	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	auto ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);
	
	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->code = 0;
	ackT->message="success";
	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);


	return;
}


void stakeTransactionUpdater(const std::string & fromAddr, uint64_t stakeAmount,  int32_t stakeType,const std::pair<std::string,std::string>& gasTrade,
                                bool isGasTrade, txAck* ack, double commissionRate, bool isFindUtxo, const std::string& encodedInfo)
{
	txAck *ack_t = ack;
	uint64_t totalGas = 0;
	DBReader dbReader;
    uint64_t height = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ack_t->code = -2;
        ack_t->message = "Failed to get the current block height";
		return;
    }

	//  Check parameters
	height += 1;
	std::vector<std::string> addressVector;

	addressVector.push_back(fromAddr);

	std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
		ack_t->code = -21;
        ack_t->message = "Get Can BeRevoke AssetType fail!";
    }

	ret = TxHelper::Check(addressVector, assetType, height);
	if(ret != 0)
	{
		ack_t->code = -3;
        ack_t->message = "Check parameters failed! The error code is " + std::to_string(ret-100);
		if(ret == -4)
		{
			ack_t->message="This account does not have any assets of the specified type!";
			ack_t->code = -4;
		}
		return;
	}

	if (!isValidAddress(fromAddr)) 
	{
		ack_t->code = -4;
        ack_t->message = "From address invlaid!";
		return;
	}

	if(stakeAmount == 0 )
	{
		ack_t->code = -5;
        ack_t->message = "Stake amount is zero !";
		return;	
	}
	
	if(stakeAmount < global::ca::MIN_STAKE_AMOUNT)
	{
		ack_t->code = -6;
        ack_t->message = "The pledge amount must be greater than " + std::to_string(global::ca::MIN_STAKE_AMOUNT);
		return;
	}

	TxHelper::stakeType pledgeType = (TxHelper::stakeType)stakeType;
	std::string stakeTypeStr;
	if (pledgeType == TxHelper::stakeType::STAKE_TYPE_NODE)
	{
		stakeTypeStr = global::ca::STAKE_TYPE_NET;
	}
	else
	{
		ack_t->code = -7;
        ack_t->message = "Unknown stake type!";
		return;
	}

	std::vector<std::string> stake_unspent_outputs;
    auto dbret = dbReader.getStakeAddrUtxo(fromAddr,assetType,stake_unspent_outputs);
	if(dbret == DBStatus::DB_SUCCESS)
	{
		ack_t->code = -8;
        ack_t->message = "There has been a pledge transaction before !";
		return;
	}

	if(encodedInfo.size() > 1024){
		ack_t->code = -9;
        ack_t->message = "The information entered exceeds the specified length";
		return;
	}

	uint64_t expend = stakeAmount;

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert({fromAddr, assetType});
	ret =TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
		ack_t->code = -10;
        ack_t->message = "FindUtxo failed! The error code is " + std::to_string(ret-200);
		return; 
	}

	if (outputUtxosSet.empty())
	{
		ack_t->code = -11;
        ack_t->message = "Utxo is empty!";
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
	CTransaction outTx;
	outTx.Clear();

	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	
	if (setTxOwners.size() != 1)
	{
		ack_t->code = -12;
        ack_t->message = "Tx owner is invalid!";
		return;
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["StakeType"] = stakeTypeStr;
	txInfo["StakeAmount"] = stakeAmount;
	if(commissionRate < global::ca::MIN_COMMISSION_RATE || commissionRate > global::ca::MAX_COMMISSION_RATE)
    {
		ack_t->code = -13;
        ack_t->message = "The commission ratio is not in the range";

		return;
    }
	txInfo["CommissionRate"] = commissionRate;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	uint64_t gas = 0;
	//  Calculate total expenditure
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::kTargetAddress, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total - expend));
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	
	if(GenerateGas(*txUtxo, outTx.type(), toAddr.size(), gasSize, gas) != 0)
	{
		ack_t->code = -14;
        ack_t->message = "The gas charge cannot be 0";
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ack_t->time = std::to_string(currentTime);

	if(isGasTrade)
	{
		totalGas += gas;
	}
	else
	{
		expend +=  gas; 
	}
	ack_t->gas = std::to_string(gas);

	//Judge whether utxo is enough
	if(total < expend)
	{
		ack_t->code = -15;
        ack_t->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return; 
	}

	CTxOutput * vout = txUtxo->add_vout(); 
	vout->set_addr(global::ca::kTargetAddress);
	vout->set_value(stakeAmount);

	CTxOutput * voutSourceAddress = txUtxo->add_vout();
	voutSourceAddress->set_addr(fromAddr);
	voutSourceAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}
	else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ack_t->code =  -16;
			ack_t->message = "Check parameters failed! The error code is" + std::to_string(ret);	
			if(ret == -4)
			{
				ack_t->message="This account does not have any assets of the specified type!";
				ack_t->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ack_t->code = -17;
			ack_t->message = "FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ack_t->code = -18;
			ack_t->message = "Utxo is empty!";	
			return ;
		}

		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ack_t->code = -19;
			ack_t->message = "Tx owner is empty!";
			return ;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}

	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTransactionTypeStake);	
	//Determine whether dropshipping is default or local dropshipping
	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ack_t->message = "fetchIdentityNodes fail!!!";
		ack_t->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);


	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrf_json_string;
	ack_t->code=0;
	ack_t->message = "success";
	ack_t->height=std::to_string(height-1);
	ack_t->txType=std::to_string((int)type);

	return;
}

void ReplaceCreateUnstakeTransaction(const std::string& fromAddr, const std::string& utxoHash,const std::pair<std::string,std::string>& gasTrade,
                                                        bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ack_t = ack;
	DBReader dbReader;
    uint64_t height = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ack_t->code = -1;
		ack_t->message = "Failed to get the current highest block height";
        return; 
    }

	height += 1;
	//  Check parameters
	std::vector<std::string> addressVector;
	addressVector.push_back(fromAddr);

	std::string assetType;
    int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
    if(ret != 0){
		ack_t->code = -15;
		ack_t->message = "Get Can BeRevoke AssetType fail!";
    }

	ret = TxHelper::Check(addressVector, assetType, height);
	if(ret != 0)
	{
		ack_t->code = -2;
		ack_t->message = "-2 Check parameters failed! The error code is " + std::to_string(ret-100);
		if(ret == -4)
		{
			ack_t->message="This account does not have any assets of the specified type!";
			ack_t->code = -4;
		}
        return; 
	}

	if (!isValidAddress(fromAddr))
	{
		ack_t->code = -3;
		ack_t->message = "FromAddr is not normal addr.";
		return;
	}

	uint64_t stakeAmount = 0;
	ret = IsQualifiedToUnstake(fromAddr, utxoHash, stakeAmount,assetType);
	if(ret != 0)
	{
		auto error = GetRpcError();
        ack_t->code = std::stoi(error.first);
        ack_t->message = error.second;
        RpcErrorClear();
		return;
	}	

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	//  The number of utxos to be searched here needs to be reduced by 1 \
	because a VIN to be redeem is from the pledged utxo, so just look for 99
	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert(std::make_pair(fromAddr, assetType));
	ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE - 1, total, outputUtxosSet, isFindUtxo); 
	if (ret != 0)
	{
		ack_t->code = -4;
		ack_t->message = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		return;
	}

	if (outputUtxosSet.empty())
	{
		ack_t->code = -5;
		ack_t->message = "Utxo is empty";
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
	CTransaction outTx;
	outTx.Clear();
	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ack_t->code = -6;
		ack_t->message = "Tx owner is empty";
		return;
	}

	if(encodedInfo.size() > 1024){
		ack_t->code = -7;
		ack_t->message = "The information entered exceeds the specified length";
		return;
	}

	{
		//  Fill vin
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 1;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["UnstakeUtxo"] = utxoHash;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	// 	The filled quantity only participates in the calculation and does not affect others
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::kTargetAddress, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total));
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	
	
	if(GenerateGas(*txUtxo,global::ca::kTxSign, toAddr.size(), gasSize, gas) != 0)
	{
		ack_t->code = -8;
		ack_t->message = "The gas charge cannot be 0";
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	//  Calculate total expenditure


	uint64_t expend = 0;
	uint64_t totalGas = 0;

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ack_t->time = std::to_string(currentTime);
	if(isGasTrade)
	{
		totalGas =  gas;
	}else
	{
		expend += gas;
	}

	ack_t->gas = std::to_string(gas);
	

	//	Judge whether there is enough money
	if(total < expend)
	{
		ack_t->code = -9;
		ack_t->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return;
	}

	//  Fill vout
	CTxOutput* outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);      	//  Release the pledge to my account number
	outputAddress->set_value(stakeAmount);

	outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);  		//  Give myself the rest
	outputAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}
	else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ack_t->code = -10;
			ack_t->message = "Check parameters failed! The error code is" + std::to_string(ret);	
			
			if(ret == -4)
			{
				ack_t->message="This account does not have any assets of the specified type!";
				ack_t->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ack_t->code = -11;
			ack_t->message = "FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ack_t->code = -12;
			ack_t->message = "Utxo is empty ";	
			return ;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ack_t->code = -13;
			ack_t->message = "Tx owner is empty!";
			return ;
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeUnstake_);

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ack_t->message = "fetchIdentityNodes fail!!!";
		ack_t->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::string txJsonString;
	std::string vrf_json_string;
	Vrf information;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrf_json_string;
	ack_t->code=0;
	ack_t->message = "success";
	ack_t->height=std::to_string(height-1);
	ack_t->txType=std::to_string((int)type);

}

void ReplaceCreateDelegatingTransaction(const std::string & fromAddr,const std::string assetType, const std::string& toAddr,
					uint64_t delegateAmount, int32_t delegateType,const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	TxHelper::vrfAgentType type;
	RpcErrorClear();
	DBReader dbReader;
    uint64_t height = 0;
	uint64_t totalGas = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->code = -1;
		ackT->message = "Failed to get the current highest block height";
        return; 

    }

	height+=1;
	//  Check parameters
	std::vector<std::string> addressVector;
	addressVector.push_back(fromAddr);
	int ret = TxHelper::Check(addressVector,assetType,height);
	if(ret != 0)
	{	
		ackT->code = -2;
		ackT->message = "Check parameters failed";

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;

	}

	//  Neither fromaddr nor toaddr can be a virtual account
	if (!isValidAddress(fromAddr))
	{
		ackT->code = -3;
		ackT->message = "The initiator address is incorrect";
		return;

	}

	if (!isValidAddress(toAddr))
	{
		ackT->code = -4;
		ackT->message = "The receiver address is incorrect";
		return;
	}

	if(delegateAmount < global::ca::kMinDelegatingAmt){
        ackT->code = -5;
		ackT->message = "Delegating amount less 3500 ";
		return;
	}

	ret = CheckDelegatingQualification(fromAddr, toAddr,assetType,delegateAmount);
	if(ret != 0)
	{
		auto error = GetRpcError();
        ackT->code = std::stoi(error.first) - 100;
        ackT->message = error.second;
        RpcErrorClear();
        return;
	}	
	std::string delegateTypeName;
	TxHelper::delegateType locadelegateType = (TxHelper::delegateType)delegateType;
	if (locadelegateType ==  TxHelper::delegateType::kdelegateType_NetLicence)
	{
		delegateTypeName = global::ca::kdelegateTypeNormal;
	}
	else
	{
		ackT->code = -6;
		ackT->message = "Unknown delegating type";
		return;
	}

	if(encodedInfo.size() > 1024){
		ackT->code = -7;
		ackT->message = "The information entered exceeds the specified length";
		return;
	}
	
	//  Find utxo
	uint64_t total = 0;
	uint64_t expend = delegateAmount;

	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multimap<std::string, std::string>	fromAddr_assetType;
	fromAddr_assetType.insert(std::make_pair(fromAddr, assetType));
	ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
	    ackT->code = -8;
		ackT->message = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		return;
	}

	if (outputUtxosSet.empty())
	{
		ackT->code = -9;
		ackT->message = "Utxo is empty";
		return;

	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
	CTransaction outTx;
	outTx.Clear();
	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->code = -10;
		ackT->message = "Tx owner is empty";
		return;

	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["delegateType"] = delegateTypeName;
	txInfo["BonusAddr"] = toAddr;
	txInfo["delegateAmount"] = delegateAmount;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	uint64_t gas = 0;	
	//  Calculate total expenditure
	std::map<std::string, int64_t> toAddrs;
	toAddrs.insert(std::make_pair(global::ca::kTargetAddress, delegateAmount));
	toAddrs.insert(std::make_pair(fromAddr, total - expend));
	toAddrs.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	
	
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	if(GenerateGas(*txUtxo,global::ca::kTxSign, toAddrs.size(),gasSize,gas) != 0)
	{
		ackT->code = -11;
		ackT->message = "gas cannot be 0";
		return;

	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ackT->time = std::to_string(currentTime);
	if(isGasTrade)
	{
		totalGas += gas;
	}
	else
	{
		expend +=  gas; 
	}
	ackT->gas = std::to_string(gas);
	
	

	if(total < expend)
	{
		ackT->code = -12;
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		return; 
	}

	CTxOutput * vout = txUtxo->add_vout(); 
	vout->set_addr(global::ca::kVirtualDelegatingAddr);
	vout->set_value(delegateAmount);

	CTxOutput * voutSourceAddress = txUtxo->add_vout();
	voutSourceAddress->set_addr(fromAddr);
	voutSourceAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}
	else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ackT->code = -13;
			ackT->message = "Check parameters failed! The error code is" + std::to_string(ret);	

			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ackT->code = -14;
			ackT->message = "FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ackT->code = -15;
			ackT->message = "Utxo is empty ";	
			return ;
		}

		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ackT->code = -16;
			ackT->message = "Tx owner is empty!";
			return ;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}
	
	
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeDelegate);

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

}


void ReplaceCreateUndelegatingTransaction(const std::string& fromAddr,const std::string &assetType, const std::string& toAddr, const std::string& utxoHash,
                        const std::pair<std::string,std::string>& gasTrade,bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT = ack;
	TxHelper::vrfAgentType type;
	CTransaction outTx;
    Vrf information;
	RpcErrorClear();
	DBReader dbReader;
    uint64_t height = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message = "Failed to get the current highest block height";
        ackT->code = -1;
        return;
    }

	height += 1;
	//  Check parameters
	std::vector<std::string> addressVector;
	addressVector.push_back(fromAddr);
	int ret = TxHelper::Check(addressVector,assetType,height);
	if(ret != 0)
	{
		ackT->message = "Check parameters failed The error code is " + std::to_string(ret-100);
        ackT->code = -2;

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;

	}

	if (!isValidAddress(fromAddr))
	{
		ackT->message = "FromAddr is not normal addr.";
        ackT->code = -3;	
		return;

	}

	if (!isValidAddress(toAddr))
	{
		ackT->message = "To address is not normal addr.";
        ackT->code = -4;		
		return;

	}

	uint64_t delegatingedAmount = 0;
	if(IsQualifiedToUndelegating(fromAddr, assetType,toAddr, utxoHash, delegatingedAmount) != 0)
	{
		auto error = GetRpcError();
        ackT->code = std::stoi(error.first) - 100;
        ackT->message = error.second;
        RpcErrorClear();
		return;
	}
	if(encodedInfo.size() > 1024){
		ackT->message = "The information entered exceeds the specified length";
	    ackT->code = -5;
		return;
	}

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert(std::make_pair(fromAddr, assetType));
	//  The utxo quantity sought here needs to be reduced by 1
	ret =TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE - 1, total, outputUtxosSet, isFindUtxo); 
	if (ret != 0)
	{
		ackT->message = "FindUtxo failed The error code is " + std::to_string(ret-300);
        ackT->code = -6;		
		return;

	}
	if (outputUtxosSet.empty())
	{
		ackT->message = "Utxo is empty";
        ackT->code = -7;	
		return;

	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
	outTx.Clear();
	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->message = "Tx owner is empty";
        ackT->code = -8;	
		return;

	}

	{
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 1;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["BonusAddr"] = toAddr;
	txInfo["UndelegatingUtxo"] = utxoHash;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	//  Calculate total expenditure
	std::map<std::string, int64_t> targetAddrs;
	targetAddrs.insert(std::make_pair(global::ca::kTargetAddress, delegatingedAmount));
	targetAddrs.insert(std::make_pair(fromAddr, total ));
	targetAddrs.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas ));
	
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	if(GenerateGas(*txUtxo,global::ca::kTxSign, targetAddrs.size(),gasSize,gas) != 0)
	{
		ackT->message = "gas cannot be 0";
        ackT->code = -9;	
		return;

	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ackT->time = std::to_string(currentTime);

	uint64_t expend = 0;
	uint64_t totalGas = 0;
	if(isGasTrade)
	{
		totalGas = gas;
	}else
	{
		expend += gas;
	}
	ackT->gas = std::to_string(gas);
	
	
	if(total < expend)
	{
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
        ackT->code = -10;	
		return;
	}	

	// Fill vout
	CTxOutput* outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);      //  Give my account the money I withdraw
	outputAddress->set_value(delegatingedAmount);

	outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);  	  //  Give myself the rest
	outputAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ackT->code = -11;
			ackT->message = "Check parameters failed! The error code is" + std::to_string(ret);	

			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ackT->code = -12;
			ackT->message = "FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ackT->code = -13;
			ackT->message = "Utxo is empty";	
			return ;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ackT->code = -14;
			ackT->message = "Tx owner is empty!";
			return ;
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}

	outTx.set_time(currentTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeUndelegate);

	// // Determine whether dropshipping is default or local dropshipping
	// if(type == TxHelper::vrfAgentType_defalut || type == TxHelper::vrfAgentType_local)
	// {
	// 	outTx.set_identity(TxHelper::GetEligibleNodes());
	// }
	// else
	// {	
	// 	// Select dropshippers
	// 	std::string allUtxos = utxoHash;
	// 	for(auto & utxo:allsetOutUtxos){
	// 		allUtxos+=utxo.hash;
	// 	}
	// 	allUtxos += std::to_string(currentTime);
		
	// 	std::string id;
    // 	int ret= get_block_packager(id, allUtxos, outTx.time());
    // 	if(ret!=0){
	// 		ackT->message = "get_block_packager error";
    //     	ackT->code = -15;	
	// 		return;

    // 	}
	// 	outTx.set_identity(id);
	// }

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(information,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

}

void updateTransactionRequest(const std::string &addr, std::string assetType,std::pair<std::string, std::string> gasTrade, bool isGasTrade, bool isFindUtxo, const std::string &encodedInfo, txAck *ack)
{
	txAck *ackT = ack;
	uint64_t height;
	int retNum = transactionDiscoveryHeight(height);
    if(retNum != 0)
	{
		ackT->code = -1;	
		ackT->message = "Failed to get the current highest block height";
        return;
	}

	//Determine if this address is the first time to claim a reward
	int64_t balance = 0;
	DBReader dbReader;
	auto dbret = dbReader.getBalanceByAddr(addr, global::ca::ASSET_TYPE_VOTE, balance);
	if (dbret != DBStatus::DB_SUCCESS && dbret != DBStatus::DB_NOT_FOUND)
	{
		ackT->code = -2;
		ackT->message = "Description Failed to obtain the balance of type Vote from the database based on the current type";
		return;
	}
	bool firstChoose = false;
	if (balance == 0 && dbret == DBStatus::DB_NOT_FOUND)
	{
		std::cout << "You get zero Vote cannot pay by Vote" << std::endl;
		isGasTrade = "1";
		int ret = ca_algorithm::GetCanBeRevokeAssetType(assetType);
		if (ret != 0)
		{
			ackT->code = -3;
			ackT->message = "Description Failed to obtain the jump asset type";
			return;
		}
		if (global::ca::ASSET_TYPE_VOTE == gasTrade.second)
		{
			ackT->code = -4;
			ackT->message = "The asset type involved in the transaction cannot be used to pay gas alone!";
			return;
		}
		int64_t gasBalance = 0;
		auto dbret = dbReader.getBalanceByAddr(addr, gasTrade.second, gasBalance);
		if (dbret != DBStatus::DB_SUCCESS)
		{
			ackT->code = -5;
			ackT->message = "Get bonus gas asset error.";
			return;
		}
		if (gasBalance <= 0)
		{
			ackT->code = -6;
			ackT->message = "Get bonus gas asset less than 0.";
			return;
		}
		firstChoose = true;
	}
	else
	{
		assetType = global::ca::ASSET_TYPE_VOTE;
	}
	std::vector<std::string> addressVector;
	addressVector.push_back(addr);
	std::cout << addr << std::endl;
	int ret = TxHelper::Check(addressVector, assetType, height);
	if(ret != 0)
	{
		ackT->code = -7;	
		ackT->message = "Check parameters failed"+std::to_string(ret);

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;
	}

	if (!isValidAddress(addr))
	{
		ackT->code = -8;	
		ackT->message = "The bonus address is incorrect";
		return;
	}

	if(encodedInfo.size() > 1024){
		ackT->code = -9;	
		ackT->message = "The information entered exceeds the specified length";
		return;
	}

	uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ret = evaluateBonusEligibility(addr, curTime);
	if(ret != 0)
	{
        auto error = GetRpcError();
        ackT->code = std::stoi(error.first);
        ackT->message = error.second;
        RpcErrorClear();
		return;
	}

	std::map<std::string, uint64_t> companyDividend;
    ret = ca_algorithm::CalcBonusValue(curTime, addr, companyDividend);
	if(ret < 0)
	{
		ackT->code = -10;	
		ackT->message = "Failed to obtain the amount claimed by the delegatingor, ret"+std::to_string(ret);
		return;
	}

	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert(std::make_pair(addr, assetType));
	ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE - 1, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{	
		ackT->code = -11;	
		ackT->message = "FindUtxo failed"+std::to_string(ret);
		return;
	}
	if (outputUtxosSet.empty())
	{
		ackT->code = -12;	
		ackT->message = "Utxo is zero";
		return;		
	}

	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
	CTransaction outTx;
	outTx.Clear();
	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(global::ca::ASSET_TYPE_VOTE);

	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->code = -13;	
		ackT->message = "Tx owner is empty";
		return;		
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	// Fill data
	double temp_commission_rate;
	int rt = ca_algorithm::GetCommissionPercentage(addr, temp_commission_rate);
	if(rt != 0)
	{
		ackT->code = -14;	
		ackT->message = "GetCommissionPercentage error"+std::to_string(rt);
		return;	
	}
	uint64_t totalClaimTemp = 0;
	uint64_t node_dividend_temp = 0;
	uint64_t temp_cost = 0;
	for(auto company : companyDividend)
	{
		temp_cost=company.second*temp_commission_rate+0.5;
		node_dividend_temp+=temp_cost;
		std::string addr = company.first;
		uint64_t award = company.second - temp_cost;
		totalClaimTemp+=award;		
	}
	totalClaimTemp += node_dividend_temp;

	nlohmann::json txInfo;
	txInfo["BonusAmount"] = totalClaimTemp;
	txInfo["BonusAddrList"] = companyDividend.size() + 2;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	// calculation gas
	uint64_t gas = 0;
    uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	std::map<std::string, int64_t> toAddrs;
	for(const auto & item : companyDividend)
	{
		toAddrs.insert(make_pair(item.first, item.second));
	}
	toAddrs.insert(std::make_pair(global::ca::kTargetAddress, total));
	toAddrs.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));

	if(GenerateGas(*txUtxo,global::ca::kTxSign, toAddrs.size(),gasSize,gas) != 0)
	{
		ackT->code = -15;	
		ackT->message = "The gas charge cannot be 0";
		return;
	}
	if(global::ca::GetInitAccountAddr()== addr){
        gas = 0;
    }
	uint64_t expend = 0;
	uint64_t totalGas = 0;
	if(isGasTrade)
	{
		totalGas = gas;
	}else
	{
		expend = gas;
	}
	if(total < expend)
	{
		ackT->code = -16;	
		ackT->message = "The total cost = {} is less than the cost = {}"+std::to_string(total);
		return;
	}
	ackT->gas = std::to_string(gas);

	//fill time
	auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ackT->time = std::to_string(currentTime);
	outTx.set_time(currentTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::TX_TYPE_BONUS);

	// Fill vout
	uint64_t costo=0;
	uint64_t nodeDividend=0;
	uint64_t totalClaim=0;
	for(auto company : companyDividend)
	{
		costo=company.second*temp_commission_rate+0.5;
		nodeDividend+=costo;
		uint64_t award = company.second - costo;
		totalClaim+=award;
	}
	std::cout << addr << ":" << nodeDividend << std::endl;
	totalClaim += nodeDividend;
	if (totalClaim == 0)
	{
		ackT->code = -17;
		ackT->message = "The claim amount is 0";
		return;
	}
	for (auto company : companyDividend)
	{
		costo = company.second * temp_commission_rate + 0.5;
		std::string addr = company.first;
		uint64_t award = company.second - costo;
		CTxOutput *outputAddress = txUtxo->add_vout();
		outputAddress->set_addr(addr);
		outputAddress->set_value(award);
		DEBUGLOG("addr {}: award{}", company.first, company.second);
	}

	CTxOutput *outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(addr);
	if (firstChoose)
	{
		outputAddress->set_value(nodeDividend);
	}
	else
	{
		outputAddress->set_value(total - expend + nodeDividend);
	}

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ackT->code = -18;
			ackT->message = "Extra Gas Trade Check parameters failed! The error code is" + std::to_string(ret);	

			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			return ;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ackT->code = -19;
			ackT->message = "Extra Gas Trade FindUtxo failed! The error code is" + std::to_string(ret);	
			return ;
		}
		if (outputUtxosSet.empty())
		{
			ackT->code = -20;
			ackT->message = "Utxo is empty ";	
			return ;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ackT->code = -21;
			ackT->message = "Tx owner is empty!";
			return ;
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}


	TxHelper::vrfAgentType type = TxHelper::vrfAgentType_vrf;
	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::string txJsonString;
	std::string vrf_json_string;
	Vrf information;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(information,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string(1);

}

int SendMessage(CTransaction & outTx,int height,Vrf &info,TxHelper::vrfAgentType type){
	std::string txHash = Getsha256hash(outTx.SerializeAsString());
	outTx.set_hash(txHash);


	TxMsgReq txMsg;
	txMsg.set_version(global::GetVersion());
	TxMsgInfo* txMsgInfo = txMsg.mutable_txmsginfo();
	txMsgInfo->set_type(0);
	txMsgInfo->set_tx(outTx.SerializeAsString());
	txMsgInfo->set_nodeheight(height);

    uint64_t txUtxoLocalHeight;
	int ret;
	if (global::ca::TxType::kTXTypeFund == (global::ca::TxType)outTx.txtype())
	{
		ret = transactionDiscoveryHeight(txUtxoLocalHeight);
	}
	else
	{
		ret = TxHelper::get_tx_utxo_height(outTx, txUtxoLocalHeight);
	}

    if(ret != 0)
    {
        ERRORLOG("get_tx_utxo_height fail!!! ret = {}", ret);
        return -1;
    }

    txMsgInfo->set_txutxoheight(txUtxoLocalHeight);
	auto msg = std::make_shared<TxMsgReq>(txMsg);
	std::string defaultAddr = MagicSingleton<AccountManager>::GetInstance()->GetDefaultAddr();
	if (type == TxHelper::vrfAgentType::vrfAgentType_vrf && outTx.identity() != defaultAddr)
	{
		ret = DropShippingTransaction(msg, outTx,outTx.identity());
	}
	else
	{
		ret = DropShippingTransaction(msg, outTx);
	}
	
	return ret;
}


void ReplaceCreateLockTransaction(const std::string & fromAddr, uint64_t lockAmount, int32_t lockType, const std::pair<std::string,std::string>& gasTrade, 
                                                    bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	txAck *ack_t = ack;
	uint64_t totalGas = 0;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ack_t->message = " db get top failed!!";
		ack_t->code = -1;
        return; 
    }

	//  Check parameters
	height += 1;
	std::vector<std::string> addressVector;
	//TODO::: need read from database
	std::string assetType = global::ca::ASSET_TYPE_VOTE;

	addressVector.push_back(fromAddr);
	int ret = TxHelper::Check(addressVector, assetType, height);
	if(ret != 0)
	{
		ack_t->message="Check parameters failed!";
		ack_t->code = -2;

		if(ret == -4)
		{
			ack_t->message="This account does not have any assets of the specified type!";
			ack_t->code = -4;
		}
		return;
	}


	if (!isValidAddress(fromAddr)) 
	{
		ack_t->message="From address invlaid!";
		ack_t->code = -3;
		return;
	}

	if(lockAmount < global::ca::LOCK_AMOUNT * global::ca::kDecimalNum)
	{
		ack_t->message="Lock amount must be greater than or equal " + std::to_string(global::ca::LOCK_AMOUNT);
		ack_t->code= -4;
		return;
	}

	TxHelper::LockType lockType_ = (TxHelper::LockType)lockType;
	std::string strLockType;
	if (lockType_ == TxHelper::LockType::kLock_Node)
	{
		strLockType = global::ca::kLockTypeNet;
	}
	else
	{
		ack_t->message="Unknown lock type! ";
		ack_t->code = -5;
		return;
	}

	if(encodedInfo.size() > 1024){
		ack_t->message="The information entered exceeds the specified length";
		ack_t->code = -6;
		return;
	}
	std::vector<std::string> stake_unspent_outputs;
    auto dbret = dbReader.getLockAddrUtxo(fromAddr, global::ca::ASSET_TYPE_VOTE ,stake_unspent_outputs);
	if(dbret == DBStatus::DB_SUCCESS)
	{
		ack_t->message="There has been a lock transaction before !";
		ack_t->code = -7;
		return;
	}

	uint64_t expend = lockAmount;

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert({fromAddr, assetType});
	ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
		ack_t->message = "FindUtxo failed! The error code is " + std::to_string(ret-200);
		ack_t->code = -8;
		return; 
	}

	if (outputUtxosSet.empty())
	{
		ack_t->message = "Utxo is empty! ";
		ack_t->code = -9;
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

	CTransaction outTx;
	outTx.Clear();

	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	
	if (setTxOwners.size() != 1)
	{
		ack_t->message = "Tx owner is invalid! " ;
		ack_t->code = -10;
		return;
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 0;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["LockType"] = strLockType;
	txInfo["LockAmount"] = lockAmount;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	//  Calculate total expenditure
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_LOCK_ADDRESS, lockAmount));
	toAddr.insert(std::make_pair(fromAddr, total - expend));
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	
	if(GenerateGas(*txUtxo, outTx.type(), toAddr.size(), gasSize, gas) != 0)
	{
		ack_t->message = " gas = 0 !" ;
		ack_t->code = -11;
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ack_t->time = std::to_string(currentTime);

	if(isGasTrade)
	{
		totalGas += gas;
	}
	else
	{
		expend +=  gas; 
	}

	//Judge whether utxo is enough
	if(total < expend)
	{
		ack_t->message = " The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		ack_t->code = 12;
		return; 
	}

	CTxOutput * vout = txUtxo->add_vout(); 
	vout->set_addr(global::ca::VIRTUAL_LOCK_ADDRESS);
	vout->set_value(lockAmount);

	CTxOutput * voutSourceAddress = txUtxo->add_vout();
	voutSourceAddress->set_addr(fromAddr);
	voutSourceAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}
	else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}


	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
			ack_t->message = "Check parameters failed! The error code is {}." + std::to_string(ret);
			ack_t->code -= -13;

			if(ret == -4)
			{
				ack_t->message="This account does not have any assets of the specified type!";
				ack_t->code = -4;
			}
			return;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
			ack_t->message = "FindUtxo failed! The error code is {}." + std::to_string(ret);
			ack_t->code -= -14;
			return;
		}
		if (outputUtxosSet.empty())
		{
			ERRORLOG(RED "Gas utxo is empty!" RESET);
			ack_t->message = "Gas utxo is empty!";
			ack_t->code = -15;
			return;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());
		
		CTxUtxos * transactionUtxo1 = outTx.add_utxos();
		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ERRORLOG(RED "Tx owner is empty!" RESET);
			ack_t->message = "Tx owner is empty!";
			ack_t->code = -16;
			return;
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);


		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}

	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeLock);	
	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ack_t->message = "fetchIdentityNodes fail!!!";
		ack_t->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);


	std::cout << "----------id: " << outTx.identity() << std::endl;


	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ack_t->txJson=txJsonString;
	ack_t->vrfJson=vrf_json_string;
	ack_t->code=0;
	ack_t->message = "success";
	ack_t->height=std::to_string(height-1);
	ack_t->txType=std::to_string((int)type);

	return;
}


void ReplaceCreateUnLockTransaction(const std::string& fromAddr, const std::string& utxoHash,const std::pair<std::string,std::string>& gasTrade,
                                                    bool isGasTrade, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	txAck* ackT = ack;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message=" db get top failed!!";
		ackT->code=-1;
        return; 
    }
	if(encodedInfo.size() > 1024){
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-2;
		return;
	}

	height += 1;
	//  Check parameters
	std::vector<std::string> addressVector;
	addressVector.push_back(fromAddr);


	//TODO:: need read from database
	std::string assetType = global::ca::ASSET_TYPE_VOTE;

	int ret = TxHelper::Check(addressVector, assetType, height);
	if(ret != 0)
	{
		ackT->message=" Check parameters failed! The error code is " + std::to_string(ret-100);
		ackT->code=-3;

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;
	}

	if (!isValidAddress(fromAddr))
	{
		ackT->message=" FromAddr is not normal  addr.";
		ackT->code=-4;
		return;
	}

	uint64_t stakeAmount = 0;
	ret = IsQualifiedToUnLock(fromAddr, utxoHash, stakeAmount,global::ca::ASSET_TYPE_VOTE);
	if(ret != 0)
	{
		//ackT->message="FromAddr is not qualified to unlock! The error code is " + std::to_string(ret-200);
		auto error = GetRpcError();
		ackT->message = error.second;
		ackT->code=std::stoi(error.first);
		RpcErrorClear();
		return;
	}	

	//  Find utxo
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	//  The number of utxos to be searched here needs to be reduced by 1 \
	because a VIN to be redeem is from the pledged utxo, so just look for 99

	std::multimap<std::string, std::string> fromAddr_assetType;
	fromAddr_assetType.insert(std::make_pair(fromAddr, assetType));
	ret = TxHelper::FindUtxo(fromAddr_assetType, TxHelper::MAX_VIN_SIZE - 1, total, outputUtxosSet, isFindUtxo); 
	if (ret != 0)
	{
		ackT->message="FindUtxo failed! The error code is " + std::to_string(ret-300);
		ackT->code=-6;
		return;
	}

	if (outputUtxosSet.empty())
	{
		ackT->message=" Utxo is empty!";
		ackT->code=-7;
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

	CTransaction outTx;
	outTx.Clear();
	CTxUtxos * txUtxo = outTx.add_utxos();
	txUtxo->set_assettype(assetType);
	//  Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->message=" Tx owner is empty!";
		ackT->code=-8;
		return;
	}

	{
		//  Fill vin
		txUtxo->add_owner(fromAddr);
		CTxInput* txin = txUtxo->add_vin();
		txin->set_sequence(0);
		CTxPrevOutput* prevout = txin->add_prevout();
		prevout->set_hash(utxoHash);
		prevout->set_n(1);
	}

	for (auto & owner : setTxOwners)
	{
		txUtxo->add_owner(owner);
		uint32_t n = 1;
		CTxInput * vin = txUtxo->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (owner == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);
	}

	nlohmann::json txInfo;
	txInfo["UnLockUtxo"] = utxoHash;

	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();
	// 	The filled quantity only participates in the calculation and does not affect others
	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_LOCK_ADDRESS, stakeAmount));
	toAddr.insert(std::make_pair(fromAddr, total));
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	
	if(GenerateGas(*txUtxo,global::ca::kTxSign, toAddr.size(), gasSize, gas) != 0)
	{
		ackT->message=" gas = 0 !";
		ackT->code=-9;
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	//  Calculate total expenditure
	
	uint64_t expend = 0;
	uint64_t totalGas = 0;


	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	TxHelper::vrfAgentType type;
	TxHelper::getTransactionStartIdentity(height,currentTime,type);
	ackT->time = std::to_string(currentTime);
	if(isGasTrade)
	{
		totalGas =  gas;
	}else
	{
		expend += gas;
	}

	//	Judge whether there is enough money
	if(total < expend)
	{
		ackT->message=" The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(expend);
		ackT->code=-10;
		return;
	}

	//  Fill vout
	CTxOutput* outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);      	//  Release the pledge to my account number
	outputAddress->set_value(stakeAmount);

	outputAddress = txUtxo->add_vout();
	outputAddress->set_addr(fromAddr);  		//  Give myself the rest
	outputAddress->set_value(total - expend);

	if(isGasTrade)
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(0);
	}
	else
	{
		CTxOutput * voutBurnAmount = txUtxo->add_vout();
		voutBurnAmount->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		voutBurnAmount->set_value(gas);
	}

	if(isGasTrade)
	{
		std::vector<std::string> fromaddr;
		fromaddr.push_back(gasTrade.first);
		int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
		if(ret != 0)
		{
			ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
			ackT->message="Check parameters failed! The error code is " + std::to_string(ret);
			ackT->code=-11;

			if(ret == -4)
			{
				ackT->message="This account does not have any assets of the specified type!";
				ackT->code = -4;
			}
			return;
		}
		
		uint64_t total = 0;
		std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
		
		std::multimap<std::string, std::string> fromAddr_assetType;
		fromAddr_assetType.insert({gasTrade.first,gasTrade.second});
		ret = TxHelper::FindUtxo(fromAddr_assetType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
		if (ret != 0)
		{
			ERRORLOG(RED "FindUtxo failed! The error code is {}." RESET, ret);
			ackT->message="FindUtxo failed! The error code is " + std::to_string(ret);
			ackT->code=-12;
			return;
		}
		if (outputUtxosSet.empty())
		{
			ERRORLOG(RED "Utxo is empty!" RESET);
			ackT->message="Utxo is empty!";
			ackT->code=-13;
			return;
		}
		allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

		CTxUtxos * transactionUtxo1 = outTx.add_utxos();

		transactionUtxo1->set_assettype(gasTrade.second);
		transactionUtxo1->set_gasutxo(1);
		outTx.set_gastx(1);
		//Fill Vin
		std::set<std::string> setTxOwners;
		for (auto & utxo : outputUtxosSet)
		{
			setTxOwners.insert(utxo.addr);
		}
		if (setTxOwners.empty())
		{
			ERRORLOG(RED "Tx owner is empty!" RESET);
			ackT->message="Tx owner is empty!";
			ackT->code=-14;
			return;
		}

		uint32_t n = 0;

		transactionUtxo1->add_owner(gasTrade.first);
		CTxInput * vin = transactionUtxo1->add_vin();
		for (auto & utxo : outputUtxosSet)
		{
			if (gasTrade.first == utxo.addr)
			{
				CTxPrevOutput * prevOutput = vin->add_prevout();
				prevOutput->set_hash(utxo.hash);
				prevOutput->set_n(utxo.n);
			}
		}
		vin->set_sequence(n++);

		CTxOutput * vout = transactionUtxo1->add_vout();
		vout->set_addr(gasTrade.first);
		vout->set_value(total - totalGas);

		CTxOutput * burnOutput = transactionUtxo1->add_vout();
		burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
		burnOutput->set_value(totalGas);

	}


	outTx.set_time(currentTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTxTypeUnLock);

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return;
}

void ReplaceCreateProposalTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade,  std::string& identifier, std::string& title,
                                                    uint64_t beinTime, uint64_t endTime, std::string assetName, std::string rate, std::string contractAddr, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT= ack;
	TxHelper::vrfAgentType type;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message = "db get top failed!!";
        ackT->code = -1;
		ERRORLOG("db get top failed!!");
        return; 

    }
	if(encodedInfo.size() > 1024){
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-2;
		return;
	}
	height+=1;

	if (!isValidAddress(fromAddr)) 
	{
		ackT->message = "FromAddr is not normal addr.";
        ackT->code = -3;
		ERRORLOG("FromAddr is not normal  addr.");
		return;
	}

	if(fromAddr != global::ca::GetInitAccountAddr())
    {
		ackT->message = "The default account is not the genesis account";
        ackT->code = -4;
		ERRORLOG("The default account is not the genesis account");
        return;
    }
	
	ca_algorithm::Trim(assetName);
	int ret = ca_algorithm::CheckProposaInfo(rate, contractAddr, minVote, assetName);
	if(ret != 0)
	{
		ackT->message = "CheckProposaInfo error " + std::to_string(ret);
        ackT->code = -5;
		ERRORLOG("CheckProposaInfo error: {}", ret);
		return;
	}

	std::vector<std::string> fromaddr;
	fromaddr.push_back(gasTrade.first);
	ret = TxHelper::Check(fromaddr,gasTrade.second,height);
	if(ret != 0)
	{
		std::string strError = "Check parameters failed! " + std::to_string(ret);;
		ackT->message = "Check parameters failed!";
        ackT->code = -6;
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;
	}
	
	
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	std::multimap<std::string, std::string> fromAddr_currencyType;
	fromAddr_currencyType.insert({gasTrade.first,gasTrade.second});
	ret = TxHelper::FindUtxo(fromAddr_currencyType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
		std::string strError = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		ackT->message = "FindUtxo failed!";
	    ackT->code = -7;
		ERRORLOG(strError);
		return;
	}
	if (outputUtxosSet.empty())
	{
		ackT->message = "Utxo is empty!";
        ackT->code = -8;
		ERRORLOG(RED "Utxo is empty!" RESET);
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

	CTransaction outTx;
	CTxUtxos * transactionUtxo1 = outTx.add_utxos();

	transactionUtxo1->set_assettype(gasTrade.second);
	transactionUtxo1->set_gasutxo(1);
	outTx.set_gastx(1);
	//Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->message = "Tx owner is empty!";
        ackT->code = -9;
		ERRORLOG("Tx owner is empty!");
		return;
	}

	uint32_t n = 0;

	transactionUtxo1->add_owner(gasTrade.first);
	CTxInput * vin = transactionUtxo1->add_vin();
	for (auto & utxo : outputUtxosSet)
	{
		if (gasTrade.first == utxo.addr)
		{
			CTxPrevOutput * prevOutput = vin->add_prevout();
			prevOutput->set_hash(utxo.hash);
			prevOutput->set_n(utxo.n);
		}
	}
	vin->set_sequence(n++);

	int isEmpty = ca_algorithm::AssetTypeListIsEmpty();
	if(isEmpty < 0)
	{
		ERRORLOG("AssetTypeListIsEmpty error: {}", isEmpty);
		ackT->message = "Get AssetType error";
        ackT->code = -11;
		return;
	}

	nlohmann::json txInfo;
	txInfo["Version"] = global::ca::KProposalInfoVersion;
	txInfo["BeginTime"] = beinTime;
	txInfo["EndTime"] = endTime;
	txInfo["ExpirationDate"] = UINT64_MAX;
	txInfo["ExchangeRate"] = rate;
	txInfo["ContractAddr"] = contractAddr;
	txInfo["canBeStake"] = isEmpty;
	txInfo["MIN_VOTE_NUM"] = minVote;

	std::string base64string;
    Base64 base;
    base64string = base.Encode((const unsigned char *)assetName.c_str(),assetName.size());
	std::string encodeTitle = base.Encode((const unsigned char *)title.c_str(),title.size());
	std::string encodeIdentifier = base.Encode((const unsigned char *)identifier.c_str(),identifier.size());
	if(encodeTitle.size() > 1024 || encodeIdentifier.size() > 1024)
	{
		ackT->message = "Title or Identifier is too long!";
		ackT->code = -14;
		ERRORLOG("Title or Identifier is too long!");
		return;
	}
	txInfo["Name"] = base64string;
	txInfo["Title"] = encodeTitle;
	txInfo["Identifier"] = encodeIdentifier;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();

	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	toAddr.insert(std::make_pair(fromAddr, 0));
	if(GenerateGas(*transactionUtxo1,global::ca::kTxSign, toAddr.size(),gasSize, gas) != 0)
	{
		ERRORLOG("GenerateGas fail gas : {}", gas);
		ackT->message = "gas = 0 !";
        ackT->code = -10;
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	if(total < gas)
	{
		ERRORLOG("The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas));
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas);
        ackT->code = -11;
		return; 
	}

	CTxOutput * vout = transactionUtxo1->add_vout();
	vout->set_addr(gasTrade.first);
	vout->set_value(total - gas);

	CTxOutput * burnOutput = transactionUtxo1->add_vout();
	burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
	burnOutput->set_value(gas);

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	if(currentTime - global::ca::KVrfVoteBeginTime > beinTime)
	{
		ackT->message = "begin time error";
        ackT->code = -12;
		ERRORLOG("begin time error");
		return;
	}

	TxHelper::getTransactionStartIdentity(height,currentTime,type);

	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::KTXTypeProposal);


	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);
	std::cout << "----------id: " << outTx.identity() << std::endl;

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return;
}

void ReplaceCreateRevokeProposalTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade,
                                                    uint64_t beinTime, uint64_t endTime, std::string proposalHash, uint64_t minVote, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT=ack;
	TxHelper::vrfAgentType type;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message = "db get top failed!!";
        ackT->code = -1;
		ERRORLOG("db get top failed!!");
        return; 
    }
	if(encodedInfo.size() > 1024){
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-2;
		return;
	}

	height+=1;

	if (!isValidAddress(fromAddr)) 
	{
		ackT->message = "FromAddr is not normal addr.";
        ackT->code = -3;
		ERRORLOG("FromAddr is not normal  addr.");
		return;
	}

	if(fromAddr != global::ca::GetInitAccountAddr())
    {
		ackT->message = "The default account is not the genesis account";
        ackT->code = -4;
		ERRORLOG("The default account is not the genesis account");
        return;
    }

	std::vector<std::string> fromaddr;
	fromaddr.push_back(gasTrade.first);
	int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
	if(ret != 0)
	{
		std::string strError = "Check parameters failed! " + std::to_string(ret);;
		ackT->message = "Check parameters failed!";
        ackT->code = -5;
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;
	}
	
	ret = ca_algorithm::GetAssetTypeIsValid(proposalHash);
	if(ret != 0)
	{
		std::string strError = "Check parameters failed! " + std::to_string(ret);;
		ackT->message = "Check parameters failed!";
        ackT->code = -6;
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);
		return;
	}

	ret = ca_algorithm::canBeRevoked(proposalHash);
	if(ret != 0)
	{
		ERRORLOG("canBeRevoked error, error num:{}", ret);
		std::string strError = "canBeRevoked error, error num " + std::to_string(ret);;
		ackT->message = strError;
        ackT->code = -7;
		return;
	}
	
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	std::multimap<std::string, std::string> fromAddr_currencyType;
	fromAddr_currencyType.insert({gasTrade.first,gasTrade.second});
	ret = TxHelper::FindUtxo(fromAddr_currencyType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
		std::string strError = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		ackT->message = "FindUtxo failed!";
	    ackT->code = -8;
		ERRORLOG(strError);
		return;
	}
	if (outputUtxosSet.empty())
	{
		ackT->message = "Utxo is empty!";
        ackT->code = -9;
		ERRORLOG(RED "Utxo is empty!" RESET);
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

	CTransaction outTx;
	CTxUtxos * transactionUtxo1 = outTx.add_utxos();

	transactionUtxo1->set_assettype(gasTrade.second);
	transactionUtxo1->set_gasutxo(1);
	outTx.set_gastx(1);
	//Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->message = "Tx owner is empty!";
        ackT->code = -10;
		ERRORLOG("Tx owner is empty!");
		return;
	}

	uint32_t n = 0;

	transactionUtxo1->add_owner(gasTrade.first);
	CTxInput * vin = transactionUtxo1->add_vin();
	for (auto & utxo : outputUtxosSet)
	{
		if (gasTrade.first == utxo.addr)
		{
			CTxPrevOutput * prevOutput = vin->add_prevout();
			prevOutput->set_hash(utxo.hash);
			prevOutput->set_n(utxo.n);
		}
	}
	vin->set_sequence(n++);

	nlohmann::json txInfo;
	txInfo["Version"] = global::ca::KRevokeProposalInfoVersion;
	txInfo["BeginTime"] = beinTime;
	txInfo["EndTime"] = endTime;
	txInfo["ProposalHash"] = proposalHash;
	txInfo["MIN_VOTE_NUM"] = minVote;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();

	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	toAddr.insert(std::make_pair(fromAddr, 0));
	if(GenerateGas(*transactionUtxo1,global::ca::kTxSign, toAddr.size(),gasSize, gas) != 0)
	{
		ERRORLOG("GenerateGas fail gas : {}", gas);
		std::cout << "GenerateGas gas = " << gas << std::endl;
		ackT->message = "gas = 0 !";
        ackT->code = -11;
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	if(total < gas)
	{
		std::string strError = "-12 The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas);
		ERRORLOG("The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas));
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas);
        ackT->code = -12;
		return; 
	}

	CTxOutput * vout = transactionUtxo1->add_vout();
	vout->set_addr(gasTrade.first);
	vout->set_value(total - gas);

	CTxOutput * burnOutput = transactionUtxo1->add_vout();
	burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
	burnOutput->set_value(gas);

	auto currentTime=MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	if(currentTime - global::ca::KVrfVoteBeginTime > beinTime)
	{
		ackT->message = "begin time error";
        ackT->code = -13;
		ERRORLOG("begin time error");
		return;
	}

	TxHelper::getTransactionStartIdentity(height,currentTime,type);

	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::KTXTyRevokeProposal);

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::cout << "----------id: " << outTx.identity() << std::endl;

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return;
}

void ReplaceCreateVoteTransaction(const std::string& fromAddr, const std::pair<std::string,std::string>& gasTrade,
                                                    const std::string& voteHash, const int pollType, const global::ca::TxType& voteTxType, bool isFindUtxo, const std::string& encodedInfo, txAck* ack)
{
	txAck *ackT=ack;
	TxHelper::vrfAgentType type;
	DBReader dbReader;
    uint64_t height = 0;
    if (DBStatus::DB_SUCCESS != dbReader.getBlockTop(height))
    {
		ackT->message = "db get top failed!!";
        ackT->code = -1;
		ERRORLOG("db get top failed!!");
        return; 

    }
	height+=1;

	if(encodedInfo.size() > 1024){
		ackT->message="The information entered exceeds the specified length";
		ackT->code=-2;
		return;
	}

	if (!isValidAddress(fromAddr)) 
	{
		ackT->message = "FromAddr is not normal addr.";
        ackT->code = -3;
		ERRORLOG("FromAddr is not normal  addr.");
		return;
	}

	std::vector<std::string> fromaddr;
	fromaddr.push_back(gasTrade.first);
	int ret = TxHelper::Check(fromaddr,gasTrade.second,height);
	if(ret != 0)
	{
		std::string strError = "Check parameters failed! " + std::to_string(ret);;
		ackT->message = "Check parameters failed!";
        ackT->code = -4;
		ERRORLOG(RED "Check parameters failed! The error code is {}." RESET, ret);

		if(ret == -4)
		{
			ackT->message="This account does not have any assets of the specified type!";
			ackT->code = -4;
		}
		return;
	}

	DBReadWriter db;
    std::vector<std::string> assertTypes;
    auto dbRet = db.getAllAssetType(assertTypes);
	if(dbRet != DBStatus::DB_SUCCESS)
    {
        ERRORLOG("VoteCache error -1, getAllAssetType error, error num:{}", dbRet);
		std::string strError = "getAllAssetType error, ret " + std::to_string(dbRet);
		ackT->message = "get all asset type error!";
		ackT->code = -16;
        return;
    }

	uint64_t lockValue = 0;
    if(assertTypes.size() == 1 && fromAddr == global::ca::GetInitAccountAddr())
    {
        lockValue = 1;
    }
    else 
	{
		ret = ca_algorithm::GetLockValue(fromAddr, lockValue);
		if(ret != 0)
		{
			std::string strError = "GetLockValue error, ret " + std::to_string(ret);
			ackT->message = "FromAddr is not qualified to unlock";
			ackT->code = -5;
			ERRORLOG("GetLockValue error, ret: {}", ret);
			return;
		}
	}

	auto currentTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	ret = ca_algorithm::CheckVoteTxInfo(voteHash, (uint32_t)voteTxType, pollType, currentTime);
	if(ret != 0)
	{
		ackT->message = "Check vote tx info error";
        ackT->code = -6;
		ERRORLOG("CheckVoteTxInfo error, ret: {}", ret);
		return;
	}


	
	uint64_t total = 0;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> outputUtxosSet;
	std::multiset<TxHelper::Utxo, TxHelper::UnspentTxOutputComparator> allsetOutUtxos;
	std::multimap<std::string, std::string> fromAddr_currencyType;
	fromAddr_currencyType.insert({gasTrade.first,gasTrade.second});
	ret = TxHelper::FindUtxo(fromAddr_currencyType,TxHelper::MAX_VIN_SIZE, total, outputUtxosSet, isFindUtxo);
	if (ret != 0)
	{
		std::string strError = "FindUtxo failed! The error code is " + std::to_string(ret-300);
		ackT->message = "FindUtxo failed!";
	    ackT->code = -7;
		ERRORLOG(strError);
		return;
	}
	if (outputUtxosSet.empty())
	{
		ackT->message = "Utxo is empty!";
        ackT->code = -8;
		ERRORLOG(RED "Utxo is empty!" RESET);
		return;
	}
	allsetOutUtxos.insert(outputUtxosSet.begin(),outputUtxosSet.end());

	CTransaction outTx;
	CTxUtxos * transactionUtxo1 = outTx.add_utxos();

	transactionUtxo1->set_assettype(gasTrade.second);
	transactionUtxo1->set_gasutxo(1);
	outTx.set_gastx(1);
	//Fill Vin
	std::set<std::string> setTxOwners;
	for (auto & utxo : outputUtxosSet)
	{
		setTxOwners.insert(utxo.addr);
	}
	if (setTxOwners.empty())
	{
		ackT->message = "Tx owner is empty!";
        ackT->code = -9;
		ERRORLOG("Tx owner is empty!");
		return;
	}

	uint32_t n = 0;

	transactionUtxo1->add_owner(gasTrade.first);
	CTxInput * vin = transactionUtxo1->add_vin();
	for (auto & utxo : outputUtxosSet)
	{
		if (gasTrade.first == utxo.addr)
		{
			CTxPrevOutput * prevOutput = vin->add_prevout();
			prevOutput->set_hash(utxo.hash);
			prevOutput->set_n(utxo.n);
		}
	}
	vin->set_sequence(n++);

	nlohmann::json txInfo;
	txInfo["VoteHash"] = voteHash;
	txInfo["VoteTxType"] = (int)voteTxType;
	txInfo["VoteType"] = pollType;
	txInfo["VoteNumber"] = lockValue;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);	

	uint64_t gas = 0;
	uint64_t gasSize = outTx.data().size() + outTx.type().size() + outTx.info().size() + outTx.reserve0().size() + outTx.reserve1().size();

	std::map<std::string, int64_t> toAddr;
	toAddr.insert(std::make_pair(global::ca::VIRTUAL_BURN_GAS_ADDR, gas));
	toAddr.insert(std::make_pair(fromAddr, 0));
	if(GenerateGas(*transactionUtxo1,global::ca::kTxSign, toAddr.size(),gasSize, gas) != 0)
	{
		ERRORLOG("GenerateGas fail gas : {}", gas);
		std::cout << "GenerateGas gas = " << gas << std::endl;
		ackT->message = "gas = 0 !";
        ackT->code = -10;
		return;
	}
	if(global::ca::GetInitAccountAddr()== fromAddr){
        gas = 0;
    }
	if(total < gas)
	{
		ERRORLOG("The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas));
		ackT->message = "The total cost = " + std::to_string(total) + " is less than the cost = {}" + std::to_string(gas);
        ackT->code = -11;
		return; 
	}

	CTxOutput * vout = transactionUtxo1->add_vout();
	vout->set_addr(gasTrade.first);
	vout->set_value(total - gas);

	CTxOutput * burnOutput = transactionUtxo1->add_vout();
	burnOutput->set_addr(global::ca::VIRTUAL_BURN_GAS_ADDR);
	burnOutput->set_value(gas);

	TxHelper::getTransactionStartIdentity(height,currentTime,type);

	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_time(currentTime);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::KTXTyVote);

	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), currentTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	DEBUGLOG("Tx identity:{}", identity);
	outTx.set_identity(identity);

	std::cout << "----------id: " << outTx.identity() << std::endl;

	std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	Vrf info;
	status=google::protobuf::util::MessageToJsonString(info,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message="success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return;
}


void ReplaceCreateFundTransaction(const std::string& addr, const std::string& encodedInfo,txAck* ack){

	Vrf information;
	DBReader dbReader;
	txAck *ackT = ack;
	CTransaction outTx;
	TxHelper::vrfAgentType type;

	uint64_t height = 0;
	int retNum = transactionDiscoveryHeight(height);
	if (retNum != 0)
	{
		ackT->code = -1;
		ackT->message = "db get top failed!!";
		ERRORLOG("transactionDiscoveryHeight error {}", retNum);
		return;
	}

	if(encodedInfo.size() > 1024){
		ackT->code=-2;
		ackT->message="The information entered exceeds the specified length";
		return;
	}

	height+=1;

	if (!isValidAddress(addr))
	{
		ERRORLOG(RED "Default is not normal  addr." RESET);
		ackT->code = -3;	
		ackT->message = "Default is not normal  addr.";
		return;
	}

	std::map<std::string, TxHelper::ProposalInfo> assetMap;
    int ret = ca_algorithm::fetchAvailableAssetType(assetMap);
	if(ret != 0){
		ERRORLOG("Get available asset fail! ret : {}",ret);
		ackT->code = -4;	
		ackT->message = "Get available asset fail!";
		return ;
	}

	
	uint64_t curTime = MagicSingleton<TimeUtil>::GetInstance()->GetUTCTimestamp();
	uint64_t period = MagicSingleton<TimeUtil>::GetInstance()->GetPeriod(curTime);
	TxHelper::ProposalInfo null;
	assetMap.insert({global::ca::ASSET_TYPE_VOTE,null});

	std::map<std::string,uint64_t> assetTypeAmount;
	for(const auto& _assetType : assetMap){
		uint64_t gasAmount = 0;
		auto result = dbReader.getGasAmountByPeriod(period -1,_assetType.first,gasAmount);
		if(result != DBStatus::DB_SUCCESS && result != DBStatus::DB_NOT_FOUND){
			ERRORLOG("GetGasamountbyTimeType error!");
			ackT->code = -5;	
			ackT->message = "GetGasamountbyTimeType error!";
			return ;
		}
		if(gasAmount != 0){
			assetTypeAmount.insert({_assetType.first,gasAmount});
		}
	}
	
	if(assetTypeAmount.empty()){
		ERRORLOG("There was no fund reward the day before!");
		ackT->code = -999;	
		ackT->message = "There was no fund reward the day before!";
		return ;
	}

	bool isLocked = false;
	bool isPackage = false;

	uint64_t retValue = 0;
	
	ret = CheckFundQualificationAndGetRewardAmount(addr, curTime,retValue);
	uint64_t totalLockedAmount = 0;
	if(ret != 0)
	{
		if(ret == -6){
			ERRORLOG( "You have already claimed the fund reward.",ret );
			ackT->code = -998;	
			ackT->message = "You have already claimed the fund reward.";
			return ;
		}
	}else{
		if(dbReader.getTotalLockedAmonut(totalLockedAmount) == DBStatus::DB_SUCCESS && totalLockedAmount != 0){
		}else{
			ERRORLOG("Get total locked amount failed!");
		}	
		if(retValue != 0 && totalLockedAmount != 0){
			isLocked = true;
		}
	}


	uint64_t times , count = 0;
	ret = VerifyBonusAddr(addr);
	int64_t stakeTime = ca_algorithm::GetPledgeTimeByAddr(addr, global::ca::StakeType::STAKE_TYPE_NODE);
	if (stakeTime > 0 && ret == 0)
	{
		DEBUGLOG("Meet the pledge qualifications.");
		if(dbReader.getPackagerTimesByPeriod(period -1,addr,times) == DBStatus::DB_SUCCESS && 
			dbReader.getBlockNumberByPeriod(period -1,count) == DBStatus::DB_SUCCESS){
		}else{
			ERRORLOG("Get addr package times failed!");
		}
		if(times != 0 && count != 0){
			isPackage = true;
		}	
	}


	if(!isPackage && !isLocked){
		ERRORLOG("Nodes are not eligible to claim fund");
		ackT->code = -6;	
		ackT->message = "Nodes are not eligible to claim fund";
		return ;
	}

	std::map<std::string, uint64_t> lockedReward;
	std::map<std::string, uint64_t> packageReward;

	for(const auto& _assetType : assetTypeAmount){
		CTxUtxos* txUtxo = outTx.add_utxos();
		txUtxo->add_owner(addr);
		txUtxo->set_assettype(_assetType.first);

		if(isLocked){
			double rate = double(retValue) / double(totalLockedAmount);
			CTxOutput* vout =  txUtxo->add_vout();
			vout->set_addr(addr);
			uint64_t value = uint64_t(rate * _assetType.second * 0.5);
			vout->set_value(value);
			lockedReward.insert({_assetType.first,value});
		}

		//TODO:: if times != 0
		if(isPackage){
			double rate = double(times) / double(count);
			CTxOutput* vout =  txUtxo->add_vout();
			vout->set_addr(addr);
			uint64_t value = uint64_t(rate * _assetType.second * 0.5);
			vout->set_value(value);
			packageReward.insert({_assetType.first,value});
		}

	}

	nlohmann::json txInfo;
	txInfo["FundLockedAmount"] = lockedReward;
	txInfo["FundPackageAmount"] = packageReward;
	nlohmann::json data;
	data["TxInfo"] = txInfo;
	outTx.set_data(data.dump());
	outTx.set_info(encodedInfo);
	outTx.set_type(global::ca::kTxSign);

	TxHelper::getTransactionStartIdentity(height,curTime, type);
	outTx.set_time(curTime);
	outTx.set_version(global::ca::CURRENT_TRANSACTION_VERSION);
	outTx.set_consensus(global::ca::kConsensus);
	outTx.set_txtype((uint32_t)global::ca::TxType::kTXTypeFund);



	auto calculate_all_hash = [](const CTransaction& outTx) {
		std::string allHash;
		for (const auto& txUtxo : outTx.utxos()) {
			for (int i = 0; i < txUtxo.vin_size(); i++) {
				auto vin = txUtxo.vin(i);
				for (int j = 0; j < vin.prevout_size(); j++) {
					auto txPrevOutput = vin.prevout(j);
					allHash += txPrevOutput.hash();
				}
			}
		}
		allHash += std::to_string(outTx.time());
		return allHash;
	};

	DEBUGLOG("self hash:{}", calculate_all_hash(outTx));
	std::string identity;
	ret = TxHelper::fetchIdentityNodes(type, calculate_all_hash(outTx), curTime, identity);
	if(ret != 0)
	{
		ERRORLOG("fetchIdentityNodes fail!!! ret:{}", ret);
		ackT->message = "fetchIdentityNodes fail!!!";
		ackT->code = -15;
		return;
	}

	outTx.set_identity(identity);

		std::string txJsonString;
	std::string vrf_json_string;
	google::protobuf::util::Status status =google::protobuf::util::MessageToJsonString(outTx,&txJsonString);
	status=google::protobuf::util::MessageToJsonString(information,&vrf_json_string);

	ackT->txJson=txJsonString;
	ackT->vrfJson=vrf_json_string;
	ackT->code=0;
	ackT->message = "success";
	ackT->height=std::to_string(height-1);
	ackT->txType=std::to_string((int)type);

	return ;
}

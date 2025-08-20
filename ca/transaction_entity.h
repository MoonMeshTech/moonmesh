/**
 * *****************************************************************************
 * @file        transaction_entity.h
 * @brief       
 * @date        2023-09-27
 * @copyright   mm
 * *****************************************************************************
 */
#ifndef TRANSACTION_ENTITY_HEADER
#define TRANSACTION_ENTITY_HEADER

#include <iostream>

#include "proto/transaction.pb.h"
#include "proto/ca_protomsg.pb.h"

class TransactionEntity
{
private:
		TxMsgReq _msg;
        CTransaction _transaction;
		uint64_t txUtxoHeight_1;
        uint64_t _height;
        bool _executedBefore;

    public:
	    TransactionEntity(const TxMsgReq& msg, CTransaction& transaction, const uint64_t txUtxoHeight)
			 : _msg(msg), _transaction(transaction), txUtxoHeight_1(txUtxoHeight) {};
        TransactionEntity(CTransaction transaction, uint64_t height, bool executedBefore)
			 : _transaction(transaction), _height(height), _executedBefore(executedBefore){};
		TransactionEntity() = default;
        ~TransactionEntity() = default;

		/**
		 * @brief       Get the TxMsgReq object
		 * 
		 * @return      TxMsgReq 
		 */
        inline const TxMsgReq& GetTxMsg() const
		{
			return _msg;
		}

		/**
		 * @brief       Get the transaction object
		 * 
		 * @return      CTransaction 
		 */
        inline CTransaction GetTransaction() const
		{
			return _transaction;
		}

		/**
		 * @brief       Get the transaction object
		 * 
		 * @return      CTransaction 
		 */
		inline CTransaction& GetTx()
		{
			return _transaction;
		}

		/**
		 * @brief
		 * 
		 * @return      uint64_t 
		 */
		inline uint64_t get_tx_utxo_height() const
		{
			return txUtxoHeight_1;
		}

		/**
		 * @brief       Get the txmsg object
		 * 
		 * @return      TxMsgReq 
		 */
		inline uint64_t GetHeight() const
		{
			return _height;
		}

	/**
	 * @brief       Get the timestamp object
	 * 
	 * @return      time_t 
	 */
	inline bool is_executed_before() const
	{
		return _executedBefore;
	}

	/**
	 * @brief       Set the transaction object
	 * 
	 * @param       transaction: 
	 */
	inline void SetTransaction(const CTransaction& transaction)
	{
		_transaction = transaction;
	}

	/**
	 * @brief       Set the txmsg object
	 * 
	 * @param       msg: 
	 */
	inline void SetHeight(uint64_t height)
	{
		_height = height;
	}

	/**
	 * @brief       Set the timestamp object
	 * 
	 * @param       timestamp: 
	 */
	inline void has_executed_before(bool executedBefore)
	{
		_executedBefore = executedBefore;
	}
};

#endif

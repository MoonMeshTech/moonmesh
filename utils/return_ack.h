#ifndef _RETURN_ACK_CODE_H_
#define _RETURN_ACK_CODE_H_

#include "net/interface.h"
#include <map>
#include <string>
#include "ca/global.h"

template<typename Ack>
void returnAcknowledgmentCode(const MsgData& msgdata, std::map<int32_t, std::string> errInfo, Ack & ack, int32_t code, const std::string & extraInfo = "")
{
	ack.set_version(global::GetVersion());
	ack.set_code(code);
	if (extraInfo.size())
	{
		ack.set_message(extraInfo);
	}
	else
	{
		ack.set_message(errInfo[code]);
	}

	NetSendMessage<Ack>(msgdata, ack, net_com::Priority::PRIORITY_HIGH_LEVEL_1); //returnAcknowledgmentCode processes most of the transactions, and the default priority is high1
}

#endif // _RETURN_ACK_CODE_H_

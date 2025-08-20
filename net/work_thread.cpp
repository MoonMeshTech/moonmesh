#include "./work_thread.h"

#include "./socket_buf.h"
#include "./epoll_mode.h"
#include "./pack.h"
#include "./ip_port.h"
#include "./peer_node.h"
#include "./global.h"
#include "./net/dispatcher.h"

#include "../proto/net.pb.h"
#include "../net/interface.h"
#include "../utils/console.h"
#include "../include/logging.h"
#include "../common/bind_thread.h"
#include "key_exchange.h"

//cpu Bind the CPU
void BindCpu()
{
	if (global::cpu_count >= 4)
	{
		int index = GetCpuIndex();
		setThreadCpu(index);
	}
}

void WorkThreads::Start()
{
	int k = 0;

	int workNum = 128;
	INFOLOG("will create {} threads", workNum);
	if(0 == workNum)
	{
		workNum = global::cpu_count * 2;
	}
    if(0 == workNum)
    {
        workNum = 8;
    }

	for (auto i = 0; i < 8; i++)
	{
		this->threadsReadList.push_back(std::thread(WorkThreads::WorkRead, i));
		this->threadsReadList[i].detach();
	}

	for (auto i = 0; i < 8; i++)
	{
		this->threadsTransList.push_back(std::thread(WorkThreads::WorkWrite, i));
		this->threadsTransList[i].detach();
	}

	for (auto i = 0; i < workNum; i++)
	{
		this->threadsWorkList.push_back(std::thread(WorkThreads::Work, i));
		this->threadsWorkList[i].detach();
	}	
}

void WorkThreads::WorkWrite(int id)
{

	BindCpu();
	while (1)
	{
		MsgData data;
		if (false == global::queue_write_counter.tryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_WRITE: 
			WorkThreads::handle_net_write(data);
			break;
		default:
			INFOLOG(YELLOW "WorkWrite drop data: data.fd :{}" RESET, data.fd);
			break;
		}
	}
}
void WorkThreads::WorkRead(int id) //Read socket dedicated thread
{
	BindCpu();

	while (1)
	{
		MsgData data;
		if (false == global::queueReader.tryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_READ:
			WorkThreads::handle_net_read(data);
			break;
		default:
			INFOLOG(YELLOW "WorkRead drop data: data.fd {}" RESET, data.fd);
			break;
		}
	}
}

void WorkThreads::Work(int id)
{

	BindCpu();

	while (1)
	{
		MsgData data;
		if (false == global::queue_work.tryWaitTop(data))
			continue;

		switch (data.type)
		{
		case E_WORK:

			WorkThreads::networkReadHandler(data);
			break;
		default:
			INFOLOG("drop data: data.fd :{}", data.fd);
			break;
		}
	}
}

int WorkThreads::networkReadHandler(MsgData &data)
{
	MagicSingleton<ProtobufDispatcher>::GetInstance()->Handle(data);
	return 0;
}


int WorkThreads::handle_net_read(const MsgData &data)
{
	int nread = 0;
	char buf[MAXLINE] = {0};
	do
	{
		nread = 0;
		memset(buf, 0, sizeof(buf));
		nread = read(data.fd, buf, MAXLINE); 
		if (nread > 0)
		{
			MagicSingleton<bufferControl>::GetInstance()->addReadBufferToQueue(data.ip, data.port, buf, nread);
		}		
		if (nread == 0 && errno != EAGAIN)
		{
			DEBUGLOG("++++handle_net_read++++ ip:({}) port:({}) fd:({})",IpPort::IpSz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(data.fd);
			MagicSingleton<KeyExchangeManager>::GetInstance()->removeKey(data.fd);
			return -1;
		}
		if (nread < 0)
		{
			if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
			{
				MagicSingleton<EpollMode>::GetInstance()->EpollLoop(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
				return 0;
			}

			DEBUGLOG("++++handle_net_read++++ ip:({}) port:({}) fd:({})",IpPort::IpSz(data.ip),data.port,data.fd);
			MagicSingleton<PeerNode>::GetInstance()->delete_by_fd(data.fd);
			MagicSingleton<KeyExchangeManager>::GetInstance()->removeKey(data.fd);
			return -1;
		}
	} while (nread >= MAXLINE);

	MagicSingleton<EpollMode>::GetInstance()->EpollLoop(data.fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return 0;
}

bool WorkThreads::handle_net_write(const MsgData &data)
{
	auto port_and_ip = net_com::dataPackPortAndIp(data.port, data.ip);

	if (!MagicSingleton<bufferControl>::GetInstance()->IsExists(port_and_ip))
	{
		DEBUGLOG("!MagicSingleton<bufferControl>::GetInstance()->IsExists({})", port_and_ip);
		return false;
	}
	if(data.fd < 0)
	{
		ERRORLOG("handle_net_write fd < 0");
		return false;
	}
	std::mutex& buff_mutex = fdMutex(data.fd);
	std::lock_guard<std::mutex> lck(buff_mutex);
	std::string buff = MagicSingleton<bufferControl>::GetInstance()->writeBufferQueue(port_and_ip);
	
	if (0 == buff.size())
	{
		return true;
	}

	auto ret = net_tcp::Send(data.fd, buff.c_str(), buff.size(), 0);

	if (ret == -1)
	{
		ERRORLOG("net_tcp::Send error");
		return false;
	}
	if (ret > 0 && ret < (int)buff.size())
	{
		
		MagicSingleton<bufferControl>::GetInstance()->PopAndWriteBufferQueue(port_and_ip, ret);
		return true;
	}
	if (ret == (int)buff.size())
	{
		
		MagicSingleton<bufferControl>::GetInstance()->PopAndWriteBufferQueue(port_and_ip, ret);
		global::phoneListMutex.lock();
		for(auto it = global::Phone_List.begin(); it != global::Phone_List.end(); ++it)
		{
			if(data.fd == *it)
			{
				close(data.fd);
				if(!MagicSingleton<bufferControl>::GetInstance()->DeleteBuffer(data.ip, data.port))
				{
					ERRORLOG(RED "DeleteBuffer ERROR ip:({}), port:({})" RESET, IpPort::IpSz(data.ip), data.port);
				}
				MagicSingleton<EpollMode>::GetInstance()->DeleteEpollEvent(data.fd);
				global::Phone_List.erase(it);
				break;
			}
		}
		global::phoneListMutex.unlock();
		return true;
	}

	return false;
}


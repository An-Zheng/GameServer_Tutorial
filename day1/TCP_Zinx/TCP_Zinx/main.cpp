#include "zinx.h"
#include "ZinxTCP.h"
#include <iostream>
using namespace std;

//这个通道只是管输出,不管输入
class StdoutChannel : public Ichannel
{
public:
	/*通道初始化函数，一般地，开发者应该重写这个函数实现打开文件和一定的参数配置
	该函数会在通道对象添加到zinxkernel时被调用*/
	virtual bool Init()
    {
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        //初始化成功返回true，因为标准输入不需要做什么初始化
        return true;
    }

	/*读取数据， 开发者应该根据目标文件不同，重写这个函数，以实现将fd中的数据读取到参数_string中
	该函数会在数据源所对应的文件有数据到达时被调用*/
	virtual bool ReadFd(std::string &_input)
    {
        return false;
    }

	/*写出数据， 开发者应该将数据写出的操作通过重写该函数实现
	该函数会在调用zinxkernel::sendout函数后被调用（通常不是直接调用而是通过多路复用的反压机制调用）*/
	virtual bool WriteFd(std::string &_output)
    {
        //当epoll 有写事件触发的时候就调用这个函数
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
		cout << _output << endl;
        return true;   //写入成功要返回true告诉zinx内核该消息已经发送出去
    }

	/*通道去初始化，开发者应该在该函数回收相关资源
	该函数会在通道对象从zinxkernel中摘除时调用*/
	virtual void Fini()
    {
        //该对象被销毁前会调用这个函数，来进行销毁前的清理工作
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
    }

	/*获取文件描述符函数， 开发者应该在该函数返回当前关心的文件，
	一般地，开发者应该在派生类中定义属性用来记录数据来记录当前IO所用的文件，在此函数中只是返回该属性*/
	virtual int GetFd() 
    {
        //这个函数返回文件描述符告诉zinx框架要监听哪个文件描述符
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return STDOUT_FILENO; ///告诉zinx要监听stdout的文件描述符
    }
    /*获取通道信息函数，开发者可以在该函数中返回跟通道相关的一些特征字符串，方便后续查找和过滤*/
	virtual std::string GetChannelInfo()
    {
        //给通道起个名字，方便查找
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return "test_stdout_channel";
    }
protected:
	/*获取下一个处理环节，开发者应该重写该函数，指定下一个处理环节
	一般地，开发者应该在该函数返回一个协议对象，用来处理读取到的字节流*/
	virtual AZinxHandler *GetInputNextStage(BytesMsg &_oInput)
    {
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return nullptr;
    }
}*poOut = new StdoutChannel;
class Echo : public AZinxHandler
{
public:
	/*信息处理函数，开发者重写该函数实现信息的处理，当有需要一个环节继续处理时，应该创建新的信息对象（堆对象）并返回指针*/
	virtual IZinxMsg *InternalHandle(IZinxMsg &_oInput)
	{

		auto msg = dynamic_cast<BytesMsg&>(_oInput);
		//如果数据是从标准输入进来的,就从标注输出出去
		if (msg.szInfo == "test_stdin_channel")
		{
			//使用框架的反压机制,使用epoll来关注某个文件描述符是否写,可写的时候再去写数据
			//将消息丢到stdoutchannel对象去处理
			ZinxKernel::Zinx_SendOut(msg.szData, *poOut);
		}
		else
		{
			//从其他通道进来,按照原来的通道出去
			auto ch  = ZinxKernel::Zinx_GetChannel_ByInfo(msg.szInfo); //通过通道的名字获取通道的指针
			ZinxKernel::Zinx_SendOut(msg.szData, *ch);
		}

		
		return nullptr;
	}

	/*获取下一个处理环节函数，开发者重写该函数，可以指定当前处理环节结束后下一个环节是什么，通过参数信息对象，可以针对不同情况进行分别处理*/
	virtual AZinxHandler *GetNextHandler(IZinxMsg &_oNextMsg)
	{
		return nullptr;
	}

}*poEcho = new Echo;

class ExitFramework : public AZinxHandler
{
public:
	/*信息处理函数，开发者重写该函数实现信息的处理，当有需要一个环节继续处理时，应该创建新的信息对象（堆对象）并返回指针*/
	virtual IZinxMsg *InternalHandle(IZinxMsg &_oInput)
	{
		//将数据输出到标准输出上
		auto msg = dynamic_cast<BytesMsg&>(_oInput);
		if (msg.szData == "exit")
		{
			//如果客户输入的是exit,就直接退出整个框架生命循环
			ZinxKernel::Zinx_Exit();
			return nullptr;
		}
		//zinx框架中使用的是BytesMsg对象来封装数据传递给下一个处理节点
		return new BytesMsg(msg);
	}

	/*获取下一个处理环节函数，开发者重写该函数，可以指定当前处理环节结束后下一个环节是什么，通过参数信息对象，可以针对不同情况进行分别处理*/
	virtual AZinxHandler *GetNextHandler(IZinxMsg &_oNextMsg)
	{
		return poEcho;
	}

}*poExit = new ExitFramework;

class CMDHandler : public AZinxHandler
{
public:
	/*信息处理函数，开发者重写该函数实现信息的处理，当有需要一个环节继续处理时，应该创建新的信息对象（堆对象）并返回指针*/
	virtual IZinxMsg *InternalHandle(IZinxMsg &_oInput)
	{
		//将数据输出到标准输出上
		auto msg = dynamic_cast<BytesMsg&>(_oInput);
		if (msg.szData == "open")
		{
			ZinxKernel::Zinx_Add_Channel(*poOut);
			return nullptr;
		}
		else if(msg.szData == "close")
		{
			//将标准输出通道从zinx框架中移除
			ZinxKernel::Zinx_Del_Channel(*poOut);
			return nullptr;
		}
		//zinx框架中使用的是BytesMsg对象来封装数据传递给下一个处理节点
		return new BytesMsg(msg);
	}

	/*获取下一个处理环节函数，开发者重写该函数，可以指定当前处理环节结束后下一个环节是什么，通过参数信息对象，可以针对不同情况进行分别处理*/
	virtual AZinxHandler *GetNextHandler(IZinxMsg &_oNextMsg)
	{
		return poExit;
	}
}*poCMDHandler = new CMDHandler;

class StdinChannel : public Ichannel
{
public:
	/*通道初始化函数，一般地，开发者应该重写这个函数实现打开文件和一定的参数配置
	该函数会在通道对象添加到zinxkernel时被调用*/
	virtual bool Init()
    {
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        //初始化成功返回true，因为标准输入不需要做什么初始化
        return true;
    }

	/*读取数据， 开发者应该根据目标文件不同，重写这个函数，以实现将fd中的数据读取到参数_string中
	该函数会在数据源所对应的文件有数据到达时被调用*/
	virtual bool ReadFd(std::string &_input)
    {
        //读数据，如果epoll触发读事件就会调用这个函数进行读取
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
		cin >> _input;
		//丢给下一个处理这去处理,不要在这里输出到到标准输出

        //如果数据读取成功也要告诉zinx框架读取成功，返回true
        return true;
    }

	/*写出数据， 开发者应该将数据写出的操作通过重写该函数实现
	该函数会在调用zinxkernel::sendout函数后被调用（通常不是直接调用而是通过多路复用的反压机制调用）*/
	virtual bool WriteFd(std::string &_output)
    {
        //当epoll 有写事件触发的时候就调用这个函数
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return false;
    }

	/*通道去初始化，开发者应该在该函数回收相关资源
	该函数会在通道对象从zinxkernel中摘除时调用*/
	virtual void Fini()
    {
        //该对象被销毁前会调用这个函数，来进行销毁前的清理工作
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
    }

	/*获取文件描述符函数， 开发者应该在该函数返回当前关心的文件，
	一般地，开发者应该在派生类中定义属性用来记录数据来记录当前IO所用的文件，在此函数中只是返回该属性*/
	virtual int GetFd() 
    {
        //这个函数返回文件描述符告诉zinx框架要监听哪个文件描述符
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return STDIN_FILENO;
    }
    /*获取通道信息函数，开发者可以在该函数中返回跟通道相关的一些特征字符串，方便后续查找和过滤*/
	virtual std::string GetChannelInfo()
    {
        //给通道起个名字，方便查找
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return "test_stdin_channel";
    }
protected:
	/*获取下一个处理环节，开发者应该重写该函数，指定下一个处理环节
	一般地，开发者应该在该函数返回一个协议对象，用来处理读取到的字节流*/
	virtual AZinxHandler *GetInputNextStage(BytesMsg &_oInput)
    {
        //cout<<__FUNCTION__<<":"<< __LINE__<<endl;
        return poCMDHandler;
    }
}*poIn = new StdinChannel;


class TestTCPData : public ZinxTcpData
{
public:
	//构造函数传入的参数就是新的TCP连接的文件描述符
	//该参数直接扔给父类,父类会记住该参数,下次读取数据会自动从这个文件描述符中读取
	TestTCPData(int _fd) :ZinxTcpData(_fd) {};
	//TCPData只需要实现一个虚函数指定下一个处理节点
	virtual AZinxHandler * GetInputNextStage(BytesMsg & _oInput)
	{
		//每次从tcp连接输入的数据都会输出到标准输出上
		return poEcho;
	}
};

//编写工厂类,用来产生TestTCPData通道
class TestTcpConnFact: public IZinxTcpConnFact {
public:
	virtual ZinxTcpData *CreateTcpDataChannel(int _fd)
	{
		return new TestTCPData(_fd);
	}
};

int main()
{
    //1 初始化 epoll_create
    ZinxKernel::ZinxKernelInit();
	

	//创建tcp的listen通道对象
	ZinxTCPListen *listen = new ZinxTCPListen(10000, new TestTcpConnFact);

    //2 添加channel到zinx框架中进行监听  epoll_ctl 添加文件描述符
    ZinxKernel::Zinx_Add_Channel(*poIn);
    ZinxKernel::Zinx_Add_Channel(*poOut);
    ZinxKernel::Zinx_Add_Channel(*listen);

    //3 触发epoll_wait ，
    ZinxKernel::Zinx_Run();
	//
    //垃圾回收
    ZinxKernel::ZinxKernelFini();
    return 0;
}

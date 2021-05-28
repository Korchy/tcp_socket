//---------------------------------------------------------------------------
#pragma hdrstop

#include "tcpsocket.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
TCPSocket::TCPSocket(void)
{
        // ����������� ������ ��� ����������
	// ������������� ����������
ListenId = -1;
NewSocketId = -1;
PortNumber = 0;
MaxClients = 0;
SocketStruc = NULL;	// ��������� � ����������� ������
Listening = false;
DataType = T_DATA;	// �� ��������� - ����� ����� ����� ���� �������� ������
BlockSizeIn = 0;
BlockSizeOut = 0;
ListenProcess = 0;
ReadChannel = -1;
WriteChannel = -1;
}
//---------------------------------------------------------------------------
TCPSocket::~TCPSocket()
{
 	// ���������� ������
if(SocketStruc!=NULL) {
	delete SocketStruc;
	SocketStruc = NULL;
	}
	// ������� ��� ���������� ������
close(ListenId);	//shutdown(ListenId,0);
ListenId = -1;
	// ���� ��� ������� ������� ������������� ������ - ������� ���
if(ListenProcess!=0) {
	kill(ListenProcess,SIGTERM);
	}
	// ���� ��� ������ ����� ��� ������ - ������� ���
if(ReadChannel!=-1) close(ReadChannel);
}
//---------------------------------------------------------------------------
//			�������, ����� ��� ���� ��������
//---------------------------------------------------------------------------
bool TCPSocket::SetPort(unsigned short PortNum)
{
        // ���������� ����� �����, ����� ������� ����� �������� �����
if(PortNum>5000&&PortNum<49152) {
	PortNumber = PortNum;
	return true;
	}
else return false;
}
//---------------------------------------------------------------------------
void TCPSocket::SetMaxClients(unsigned int MaxClt)
{
        // ���������� ������������ ���-�� ��������, ������� ����� ����� ��������� � ������� �� �������������
MaxClients = MaxClt;
}
//---------------------------------------------------------------------------
void TCPSocket::SetDataType(int DType)
{
        // ���������� ��� ������ � �������� ����� �������� ����� (��������/�����)
if(DType==0) DataType = B_DATA;
else DataType = T_DATA;
}
//---------------------------------------------------------------------------
bool TCPSocket::Start()
{
        // ������ ������ � ������
	// ������� �������� ��������� ������ � ��������� �������� ���������
SocketStruc = new sockaddr_in();
zeromemory(SocketStruc,sizeof(*SocketStruc));
if(PortNumber==0||MaxClients==0) return false;
SocketStruc->sin_family = AF_INET;			// ��������-�����
SocketStruc->sin_addr.s_addr = htonl(INADDR_ANY);	// �������� � ����� �����������
SocketStruc->sin_port = htons(PortNumber);		// � ����� ����� ������� ����� ����� �����/��������
	// �������� ID ��������������� ������
ListenId = socket(PF_INET,SOCK_STREAM,getprotobyname("tcp")->p_proto);
if(ListenId<0) {
	Message("[ER] �� ������� ListenId");
	return false;
	}
	// ����� ��� ����������� ��������� ������������ ������ ����� bind �������� �� ����� ListenId (� �� ���� ���� ��� �� ������ �.�. ��������������
	// accept'� ���������� � �������� �������) ����� ���������� �������� SO_REUSEADDR
const int on = 1;
int Rez = setsockopt(ListenId,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
if(Rez<0) Message("[ER] �� ���������� setsockopt");
	// ������� ID ������ � ����������
	// ��������� bind
Rez = bind(ListenId,(sockaddr*)SocketStruc,sizeof(*SocketStruc));
if(Rez<0) {
	Message("[ER] �� ��������� bind");
	return false;
	}
	// ������� �������������� �����
Rez = listen(ListenId,MaxClients);
if(Rez<0)  {
	Message("[ER] �� �������� listen");
	return false;
	}
	// ������� ��������� ����������� ������� � ��������� � ��� ����� �� �������� ����������
int NewProcess = fork();
switch(NewProcess) {
	case 0:{		// �������-�������
		setsid();	// ������� ����� ������� ������� � ������
		signal(SIGHUP,SIG_IGN);	// ��������� (����������) ����� ������� SIGHUP
			// ������� 2 ������ ����������� �������� ������� � ��������� ������������� ������ (�������-�������� � ���������-��������)
		int ChannelOut[2];	// ��� ��������� ������ � ������
		pipe(ChannelOut);
		int ChannelIn[2];	// ��� �������� ������ ������
		pipe(ChannelIn);
		NewProcess = fork();	// ��� ��� ������� ����� �������-������� (����� ��������-����� ��������� �� ������� ���������)
		switch(NewProcess) {
			case 0:{		// �������-�������
				chdir("/");	// �������� ������� - ��������, ����� �������� ������� � �������������
				umask(0);	// ����� ������ �������� ������ ������ � 0
				close(0);	// ��������� ���������� �����������
				close(1);
				close(2);
					// �� ���� ��������������� ������� �������� �� ������ �����������������
				close(ChannelOut[0]);	// ����� �������� ������ ����������� ��� ������ ChannelOut[1]
				close(ChannelIn[1]);	// ����� �������� ������ ����������� ��� ������ ChannelIn[0]
					// ������ ����� � ����� �������� �� �������� ����������
				Listening = true;
				while(Listening==true) {	// ��������� ���� �������� ���������� �� ��������
					sockaddr_in CltSocketStruc;	// �������� ��������� ������ ��������������� �������
					socklen_t CltSocketStrucSize = sizeof(CltSocketStruc);
					// ��������� accept � ���� ����������� �� ��������
					int ConnectionId = accept(ListenId,(sockaddr*)&CltSocketStruc,&CltSocketStrucSize);
					if(ConnectionId==-1) {	// ��������� ������������� (������ ��� �����)
						Message("[ER] ������ ���������� accept");
						Listening = false;
						break;
						}
//					Message("[OK] ����������� ������ �� "+inet_ntoa(CltSocketStruc.sin_addr));
					// ��������� ����� ������� ��� �������������� � �������������� ��������
					NewProcess = fork();
					switch(NewProcess) {
						case 0: {	// �������-�������
							close(ListenId);	// ��������� ListenId - ����� ������������� ������ �� �����
							ListenId = -1;
							// ����� ������� � ��������
							if(DataType==B_DATA) {
								// �������� � ��������� �������
								Rez = WorkWithBinaryData(ConnectionId,ChannelOut[1],ChannelIn[0]);
								}
							else {
								// �������� � ���������� �������
								Rez = WorkWithTextData(ConnectionId,ChannelOut[1],ChannelIn[0]);
								}
							Listening = false;	// ����� ��������� ��������� ������� - ������� �� ����� �������������
							break;
							}
						case -1: {	// ������ ���������� ������ ��������
							Message("[ER] ������ ���������� ��������� ��������");
							// ������ �� ������ - ������������ �� ������������� ���������� ����������� �� �������
							break;
							}
						default: {	// ������������ �������
							// ������ �� ������ - ������������ �� ������������� ���������� ����������� �� �������
							break;
							}
						}
					close(ConnectionId);
					}
				// ��� ������ �� ����� ������������� - ��������� �������
				delete SocketStruc;	// ������� SocketStruc
				SocketStruc = NULL;
				close(ChannelOut[1]);	// ������� �������� ������
				close(ChannelIn[0]);
				// �������� �������
				exit(EXIT_SUCCESS);	// raise(SIGTERM);
				break;
				}
			case -1:{	// ������ ���������� ������ ��������
				Message("[ER] ������ ���������� ��������� ��������");
				delete SocketStruc;	// ������� SocketStruc
				SocketStruc = NULL;
				close(ListenId);	// ��������� ListenId
				ListenId = -1;
				close(ChannelOut[0]);	// ������� ����� �� ������
				close(ChannelOut[1]);
				close(ChannelIn[0]);	// ������� ����� �� ������
				close(ChannelIn[1]);
				// ������� � ���������� ���������
				break;
				}
			default:{	// �������� ������� (�������-��������) - ���� ������� �������� � ���������� ���������
				// ���������� ID ������������ ��������, ����� ����� ���� �� ���������
				ListenProcess = NewProcess;
				delete SocketStruc;	// ������� SocketStruc
				SocketStruc = NULL;
				close(ListenId);	// ��������� ListenId
				ListenId = -1;
					// �� ���� ChannelOut ��������� ����� �� ������ - ����� ���� ����� �������� ������ �� ������
				close(ChannelOut[1]);
				ReadChannel = ChannelOut[0];
					// �� ���� ChannelIn ��������� ����� �� ������ - ����� ���� ����� ���������� ������ � �����
				close(ChannelIn[0]);
				WriteChannel = ChannelIn[1];
				// ������� � ���������� ���������
				break;
				}
			}
		break;
		}
	case -1:{	// ������ ���������� ������ ��������
		Message("[ER] ������ ���������� ��������� ��������");
		delete SocketStruc;	// ������� SocketStruc
		SocketStruc = NULL;
		close(ListenId);	// ��������� ListenId
		ListenId = -1;
		break;
		}
	default:{	// �������� ������� (�������-��������)
		// ���� ������� ��������� ������, ����� � ���������� ��������� �������� ������ ���� ������� �.�. ������ ������ ������� ������������� ������ fork()
		delete SocketStruc;	// ������� SocketStruc
		SocketStruc = NULL;
		close(ListenId);	// ��������� ListenId
		ListenId = -1;
		// �������� �������
		exit(EXIT_SUCCESS);	// raise(SIGTERM);
		break;
		}
	}
return true;
}
//---------------------------------------------------------------------------
bool TCPSocket::WorkWithBinaryData(int ConnectionId, int OutputChannel, int InputChannel)
{
        // �������� �������� ������ �� �������
if(BlockSizeIn==0) return false;
	// ������� ������ ��� ������ ������
char* TakenData;
TakenData = new char[BlockSizeIn];
	// �������� ������ �� ������
int BytesToReceive = BlockSizeIn;
char* PtrToReceive = TakenData;
bool Received = false;
while(Received==false) {
	// ��������� ������ ������� �� BytesToReceive ����
	int BytesReceived = read(ConnectionId,PtrToReceive,BytesToReceive);
	switch(BytesReceived) {
		case 0: {
			// ����� ���������� ������
			Received = true;
			break;
			}
		case -1: {
			// ������ ��������� ������
			delete TakenData;
			TakenData = NULL;
			Received = true;
			break;
			}
		default: {
			// ��������� �����-�� ����� ����� ������
			// ���������� ��������� ������
			BytesToReceive -= BytesReceived;	// ������� ��� �������� �������
			PtrToReceive += BytesReceived;		// �������� ���������, ����� ����� �������� ������ ������������ ������
			break;
			}
		}
	}
	// ���� ������ ��������
if(TakenData!=NULL) {
	// �������� ������ � �����
	write(OutputChannel,TakenData,BlockSizeIn);
	// ������� ������
	delete TakenData;
	return true;
	}
else return false;
}
//---------------------------------------------------------------------------
bool TCPSocket::WorkWithTextData(int ConnectionId, int OutputChannel, int InputChannel)
{
        // ������ �� �������� ������
if(BlockSizeIn==0) return false;
std::string* TakenString;
TakenString = new std::string();	// ����������� ������
std::string* SendingString;
SendingString = new std::string();	// ������������ ������
char DataBlock;	// ���� ������
	// �������������� select �����
int Max_fd;
fd_set Rset;
FD_ZERO(&Rset);
bool Working = true;
while(Working==true) {
	// ������������������ select ��� ������ ����������� - �������� ��� ���������� ������
	FD_SET(ConnectionId,&Rset);	// ����� ������ ��������� �� ����� (�������� ��)
	FD_SET(InputChannel,&Rset);	// ����� ������ �� ������ (��������� ��)
	if(ConnectionId>InputChannel) Max_fd = ConnectionId+1;
	else Max_fd = InputChannel+1;
	select(Max_fd,&Rset,NULL,NULL,NULL);	// ��������� �������� ������������. ���� �� ����� �� ��� �������� ������ - ���������� ��
	if(FD_ISSET(ConnectionId,&Rset)) {
		// �������� ������ �� �������
		// ��������� ������ ������� �� 1 ����
		int BytesReceived = read(ConnectionId,&DataBlock,1);
		switch(BytesReceived) {
			case 0: {
				// ����� �������� ������ (������ �������� ����������)
				if((*TakenString).find("\n",0)==-1) *TakenString += "\n";	// ���� �� �����-���� ������� �� ������� ����� ������ - ��������
				Working = false;
				break;
				}
			case -1: {
				// ������ ��������� ������
				// �������� ������ � ���������� ������ � ��������
				*TakenString = "";
				break;
				}
			default: {
				// ���������� ��������� ����� ������
				// ���������� ����� � ������, ������� ������� (\r) �� ���������
		    		if(DataBlock!='\r') *TakenString += DataBlock;
				// ���� ����������� ������� ������ (\n), ����� ������ �����������
				if(DataBlock=='\n') {
					// ������� ������ ������ ������
//					Message(TakenString);
					// �������� ������ ������ � �����
					write(OutputChannel,(*TakenString).c_str(),(*TakenString).length()-1);
					// �������� ������
					*TakenString = "";
					}
				break;
				}
			}
		}
	if(FD_ISSET(InputChannel,&Rset)) {
		// ���������� ������ �������
		// ��������� ������ �� ������ ������� �� 1 ����
		int BytesReceived = read(InputChannel,&DataBlock,1);
		switch(BytesReceived) {
			case 0: {
				// ��������� �������� ������ ����� ����� (�������� ������� ������� ������)
				*SendingString = "";  // ������� �������
				Working = false;      // �������� ������ � ��������
				break;
				}
			case -1: {
				// ������ ������ ������ �� ������
				// �������� ������ � ���������� �������� � ��������
				*SendingString = "";
				break;
				}
			default: {
				// ���������� ������ ����� ������ �� ������
				// ���������� ����� � ������, ������� ������� (\r) �� ���������
		    		if(DataBlock!='\r') *SendingString += DataBlock;
				// ���� ����������� ������� ������ (\n), �������, ��� ������ �� ������ ��� ������ ��� �������� �������
				if(DataBlock=='\n') {
					// ��������� ������ ������ �� ������ - ��������� �� �������
					write(ConnectionId,(*SendingString).c_str(),(*SendingString).length());
					// �������� ������
					*SendingString = "";
					}
				break;
				}
			}
		}
	}
	// ���� ������� ������ � �������� ��������
	// ������� ������� ��� ������/�������� ������
delete TakenString;
TakenString = NULL;
delete SendingString;
SendingString = NULL;
return true;
}
//---------------------------------------------------------------------------
//				��������� �������
//---------------------------------------------------------------------------
void TCPSocket::zeromemory(void* ptr, int BytesCount)
{
        // ���������� ������� ������ 0
memset(ptr,0,BytesCount);
}
//---------------------------------------------------------------------------
void TCPSocket::Message(char Msg[])
{
        // ����� ��������� � ���
syslog(LOG_INFO||LOG_USER,Msg);
}
//---------------------------------------------------------------------------
void TCPSocket::Message(std::string* Msg)
{
        // ����� ��������� � ���
syslog(LOG_INFO||LOG_USER,(*Msg+"\n").c_str());
}
//---------------------------------------------------------------------------

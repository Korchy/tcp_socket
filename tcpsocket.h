//---------------------------------------------------------------------------
#ifndef TCPSocket_H
#define TCPSocket_H
//---------------------------------------------------------------------------
#include <iostream>		// ��� ������������� ��������� ������ ������ �� �����
#include <netinet/in.h>		// �������� �������� ��������� ������
#include <sys/socket.h>		// ����������� �-��� bind()
#include <unistd.h>		// ����������� �-��� close(),fork()
#include <string>		// ��� ������������� ���� string
#include <signal.h>		// ��� ��������� ��������
#include <sys/stat.h>		// ��� ������������� �-��� umask()
#include <syslog.h>		// ��� ������������� ������� ����� (/var/log/messages)
#include <arpa/inet.h>		// ��� ������������� �-��� inet_ntoa
#include <netdb.h>		// ��� ������������� getprotobyname
//---------------------------------------------------------------------------
// ����� TCPSocket - ������������ ��������� � �������� ������ ����� TCP-�����
//---------------------------------------------------------------------------
class TCPSocket
{
protected:
        // �������� ������ ��������
        // ����������
	
	// �������
	void Message(char Msg[]);	// ����� ��������� � ���
	void Message(std::string* Msg);	// ����� ��������� � ���
private:
        // �������� ������ ����� ������
        // ����������
	int ListenId;			// ID ��������������� ������
	int NewSocketId;		// ID ������ ������������ ������
	
	unsigned short PortNumber;	// � ����� ����� ������� ����� ������� ����� ������� � ���������
	unsigned int MaxClients;	// ������������ ���-�� ��������, ������� ����� ��������� �� ������������� �������
	
	sockaddr_in* SocketStruc;	// �������� ��������� ������ �������
	
	bool Listening;			// true - ����� ��������� � ������ �������� ���������� �� ��������, false - ���
	enum {B_DATA,T_DATA} DataType;	// ��� ������ � �������� �������� ����� (��������/�����)
	
	unsigned int ListenProcess;	// ����� �������� �� ������� �������� ������������� ������ �� ����������� ��������
	
	// �������
	void zeromemory(void* ptr, int BytesCount);	// ��������� ������� ������� ������
	
public:
        // �������� ����
        TCPSocket(void);		// ���������� ������ ��� ����������
        virtual ~TCPSocket();		// ���������� ������
	
        // ����������
	unsigned int BlockSizeIn;	// ������ ����� ����������� ������ (��� ������ - ���-�� ��������, ��� �������� ������ - ���-�� ����)
	unsigned int BlockSizeOut;	// ������ ����� ������������ ������ (��� ������ - ���-�� ��������, ��� �������� ������ - ���-�� ����)
	
	int ReadChannel;		// ���������� ������ ����� ������� ����� ������ ����������� �� ����� ������
	int WriteChannel;		// ���������� ������ ����� ������� ����� ������ ��� �������� �� ����� ������
	
        // �������
	bool Start();		// ������ ������ � ������
	bool SetPort(unsigned short PortNum);	// ���������� ����� �����, ����� ������� ����� �������� �����
	void SetMaxClients(unsigned int MaxClt);	// ���������� ������������ ���-�� ��������, ��������������� �� �������������
	void SetDataType(int DType);		// ���������� ��� ������ � �������� ����� �������� ����� (��������/�����)
		
	bool WorkWithBinaryData(int ConnectionId, int OutputChannel, int InputChannel);	// �������� �������� ������ �� �������
	bool WorkWithTextData(int ConnectionId, int OutputChannel, int InputChannel);	// �������� ������ ������ �� �������
	
};
//---------------------------------------------------------------------------
#endif
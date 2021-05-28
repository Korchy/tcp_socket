//---------------------------------------------------------------------------
#pragma hdrstop

#include "tcpsocket.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
TCPSocket::TCPSocket(void)
{
        // Конструктор класса без параметров
	// Инициализация переменных
ListenId = -1;
NewSocketId = -1;
PortNumber = 0;
MaxClients = 0;
SocketStruc = NULL;	// Структура с параметрами сокета
Listening = false;
DataType = T_DATA;	// По умолчанию - обмен через сокет идет строками текста
BlockSizeIn = 0;
BlockSizeOut = 0;
ListenProcess = 0;
ReadChannel = -1;
WriteChannel = -1;
}
//---------------------------------------------------------------------------
TCPSocket::~TCPSocket()
{
 	// Деструктор класса
if(SocketStruc!=NULL) {
	delete SocketStruc;
	SocketStruc = NULL;
	}
	// Закрыть все соединения сокета
close(ListenId);	//shutdown(ListenId,0);
ListenId = -1;
	// Если был запущен процесс прослушивания сокета - удалить его
if(ListenProcess!=0) {
	kill(ListenProcess,SIGTERM);
	}
	// Если был открыт канал для чтения - закрыть его
if(ReadChannel!=-1) close(ReadChannel);
}
//---------------------------------------------------------------------------
//			ФУНКЦИИ, ОБЩИЕ ДЛЯ ВСЕХ ОБЪЕКТОВ
//---------------------------------------------------------------------------
bool TCPSocket::SetPort(unsigned short PortNum)
{
        // Установить номер порта, через который будет работать сокет
if(PortNum>5000&&PortNum<49152) {
	PortNumber = PortNum;
	return true;
	}
else return false;
}
//---------------------------------------------------------------------------
void TCPSocket::SetMaxClients(unsigned int MaxClt)
{
        // Установить максимальное кол-во клиентов, которых можно будет поставить в очередь на прослушивание
MaxClients = MaxClt;
}
//---------------------------------------------------------------------------
void TCPSocket::SetDataType(int DType)
{
        // Установить тип данных с которыми будет работать сокет (двоичные/текст)
if(DType==0) DataType = B_DATA;
else DataType = T_DATA;
}
//---------------------------------------------------------------------------
bool TCPSocket::Start()
{
        // Запуск сокета в работу
	// Создать основную структуру сокета и заполнить основные параметры
SocketStruc = new sockaddr_in();
zeromemory(SocketStruc,sizeof(*SocketStruc));
if(PortNumber==0||MaxClients==0) return false;
SocketStruc->sin_family = AF_INET;			// Интернет-сокет
SocketStruc->sin_addr.s_addr = htonl(INADDR_ANY);	// Работать с любым интерфейсом
SocketStruc->sin_port = htons(PortNumber);		// № порта через который будем вести прием/передачу
	// Получить ID прослушиваемого сокета
ListenId = socket(PF_INET,SOCK_STREAM,getprotobyname("tcp")->p_proto);
if(ListenId<0) {
	Message("[ER] Не получен ListenId");
	return false;
	}
	// Чтобы при перезапуске программы использующей данный сокет bind цеплялся на тотже ListenId (а он можт быть еще не закрыт т.к. обрабатываются
	// accept'ы оставшиеся с прошлого запуска) нежно установить параметр SO_REUSEADDR
const int on = 1;
int Rez = setsockopt(ListenId,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
if(Rez<0) Message("[ER] Не установлен setsockopt");
	// Связать ID сокета с протоколом
	// Выполнить bind
Rez = bind(ListenId,(sockaddr*)SocketStruc,sizeof(*SocketStruc));
if(Rez<0) {
	Message("[ER] Не выполненн bind");
	return false;
	}
	// Создать прослушиваемый сокет
Rez = listen(ListenId,MaxClients);
if(Rez<0)  {
	Message("[ER] Не выполнен listen");
	return false;
	}
	// Создать отдельный независимый процесс и запустить в нем сокет на ожидание соединений
int NewProcess = fork();
switch(NewProcess) {
	case 0:{		// Процесс-потомок
		setsid();	// Сделать новый процесс главным в группе
		signal(SIGHUP,SIG_IGN);	// Отключаем (игнорируем) прием сигнала SIGHUP
			// Создать 2 канала связывающие основной процесс с процессом прослушивания сокета (процесс-родитель с процессом-потомком)
		int ChannelOut[2];	// Для получения данных с сокета
		pipe(ChannelOut);
		int ChannelIn[2];	// Для отправки данных сокету
		pipe(ChannelIn);
		NewProcess = fork();	// Еще раз создаем новый процесс-потомок (чтобы процессы-зомби удалялись из таблицы процессов)
		switch(NewProcess) {
			case 0:{		// Процесс-потомок
				chdir("/");	// Корневой каталог - основной, чтобы избежать проблем с монтированием
				umask(0);	// Маску режима создания файлов ставим в 0
				close(0);	// Закрываем извеестные дескрипторы
				close(1);
				close(2);
					// Из двух двунаправленных каналов оставить по одному однонаправленному
				close(ChannelOut[0]);	// Чтобы осталось только направление для записи ChannelOut[1]
				close(ChannelIn[1]);	// Чтобы осталось только направление для чтения ChannelIn[0]
					// Ставим сокет в новом процессе на ожидание соединений
				Listening = true;
				while(Listening==true) {	// Запускаем цикл ожидания соединений от клиентов
					sockaddr_in CltSocketStruc;	// Основная структура сокета подключающегося клиента
					socklen_t CltSocketStrucSize = sizeof(CltSocketStruc);
					// Выполняем accept и ждем подключений от клиентов
					int ConnectionId = accept(ListenId,(sockaddr*)&CltSocketStruc,&CltSocketStrucSize);
					if(ConnectionId==-1) {	// Отключить прослушивание (ошибка или выход)
						Message("[ER] Ошибка выполнения accept");
						Listening = false;
						break;
						}
//					Message("[OK] Подключился клиент от "+inet_ntoa(CltSocketStruc.sin_addr));
					// Порождаем новый процесс для взаимодействия с подключившимся клиентом
					NewProcess = fork();
					switch(NewProcess) {
						case 0: {	// Процесс-потомок
							close(ListenId);	// Закрываем ListenId - здесь прослушивание больше не нужно
							ListenId = -1;
							// Обмен данными с клиентом
							if(DataType==B_DATA) {
								// Работаем с двоичными данными
								Rez = WorkWithBinaryData(ConnectionId,ChannelOut[1],ChannelIn[0]);
								}
							else {
								// Работаем с текстовыми данными
								Rez = WorkWithTextData(ConnectionId,ChannelOut[1],ChannelIn[0]);
								}
							Listening = false;	// Чтобы корректно завершить процесс - выходим из цикла прослушивания
							break;
							}
						case -1: {	// Ошибка порождения нового процесса
							Message("[ER] Ошибка порождения дочернего процесса");
							// Ничего не делаем - возвращаемся на прослушивание следующего подключения от клиента
							break;
							}
						default: {	// Родительский процесс
							// Ничего не делаем - возвращаемся на прослушивание следующего подключения от клиента
							break;
							}
						}
					close(ConnectionId);
					}
				// При выходе из цикла прослушивания - завершить процесс
				delete SocketStruc;	// Удаляем SocketStruc
				SocketStruc = NULL;
				close(ChannelOut[1]);	// Закрыть открытые каналы
				close(ChannelIn[0]);
				// Оборвать процесс
				exit(EXIT_SUCCESS);	// raise(SIGTERM);
				break;
				}
			case -1:{	// Ошибка порождения нового процесса
				Message("[ER] Ошибка порождения дочернего процесса");
				delete SocketStruc;	// Удаляем SocketStruc
				SocketStruc = NULL;
				close(ListenId);	// Закрываем ListenId
				ListenId = -1;
				close(ChannelOut[0]);	// Закрыть канал на чтение
				close(ChannelOut[1]);
				close(ChannelIn[0]);	// Закрыть канал на запись
				close(ChannelIn[1]);
				// Возврат в вызывающую процедуру
				break;
				}
			default:{	// Основной процесс (процесс-родитель) - этот процесс вернется в вызывающую процедуру
				// Запоминаем ID порожденного процесса, чтобы можно было им управлять
				ListenProcess = NewProcess;
				delete SocketStruc;	// Удаляем SocketStruc
				SocketStruc = NULL;
				close(ListenId);	// Закрываем ListenId
				ListenId = -1;
					// Из пары ChannelOut оставляем канал на чтение - через него будем получать данные из сокета
				close(ChannelOut[1]);
				ReadChannel = ChannelOut[0];
					// Из пары ChannelIn оставляем канал на запись - через него будем отправлять данные в сокет
				close(ChannelIn[0]);
				WriteChannel = ChannelIn[1];
				// Возврат в вызывающую процедуру
				break;
				}
			}
		break;
		}
	case -1:{	// Ошибка порождения нового процесса
		Message("[ER] Ошибка порождения дочернего процесса");
		delete SocketStruc;	// Удаляем SocketStruc
		SocketStruc = NULL;
		close(ListenId);	// Закрываем ListenId
		ListenId = -1;
		break;
		}
	default:{	// Основной процесс (процесс-родитель)
		// Этот процесс завершаем совсем, чтобы в вызывающую процедуру вернулся только один процесс т.к. дальше пойдет процесс разветвленный вторым fork()
		delete SocketStruc;	// Удаляем SocketStruc
		SocketStruc = NULL;
		close(ListenId);	// Закрываем ListenId
		ListenId = -1;
		// Оборвать процесс
		exit(EXIT_SUCCESS);	// raise(SIGTERM);
		break;
		}
	}
return true;
}
//---------------------------------------------------------------------------
bool TCPSocket::WorkWithBinaryData(int ConnectionId, int OutputChannel, int InputChannel)
{
        // Получить двоичные данные от клиента
if(BlockSizeIn==0) return false;
	// Создать массив для приема данных
char* TakenData;
TakenData = new char[BlockSizeIn];
	// Получить данные из сокета
int BytesToReceive = BlockSizeIn;
char* PtrToReceive = TakenData;
bool Received = false;
while(Received==false) {
	// Принимаем данные блоками по BytesToReceive байт
	int BytesReceived = read(ConnectionId,PtrToReceive,BytesToReceive);
	switch(BytesReceived) {
		case 0: {
			// Конец переданных данных
			Received = true;
			break;
			}
		case -1: {
			// Ошибка получения данных
			delete TakenData;
			TakenData = NULL;
			Received = true;
			break;
			}
		default: {
			// Получение какой-то части блока данных
			// Продолжаем принимать данные
			BytesToReceive -= BytesReceived;	// Сколько еще осталось принять
			PtrToReceive += BytesReceived;		// Сдвинуть указатель, чтобы новые принятые данные дописывались дальше
			break;
			}
		}
	}
	// Если данные получены
if(TakenData!=NULL) {
	// Записать данные в канал
	write(OutputChannel,TakenData,BlockSizeIn);
	// Удалить массив
	delete TakenData;
	return true;
	}
else return false;
}
//---------------------------------------------------------------------------
bool TCPSocket::WorkWithTextData(int ConnectionId, int OutputChannel, int InputChannel)
{
        // Работа со строками текста
if(BlockSizeIn==0) return false;
std::string* TakenString;
TakenString = new std::string();	// Принимаемая строка
std::string* SendingString;
SendingString = new std::string();	// Отправляемая строка
char DataBlock;	// Блок данных
	// Инициализируем select нулем
int Max_fd;
fd_set Rset;
FD_ZERO(&Rset);
bool Working = true;
while(Working==true) {
	// Переинициализируем select для выбора дескриптора - получать или отправлять данные
	FD_SET(ConnectionId,&Rset);	// Брать данные пришедшие на сокет (получить их)
	FD_SET(InputChannel,&Rset);	// Брать данные из канала (отправить их)
	if(ConnectionId>InputChannel) Max_fd = ConnectionId+1;
	else Max_fd = InputChannel+1;
	select(Max_fd,&Rset,NULL,NULL,NULL);	// Запустить просмотр дескрипторов. Если на одном из них появятся данные - обработать их
	if(FD_ISSET(ConnectionId,&Rset)) {
		// Получаем данные от клиента
		// Принимаем данные блоками по 1 байт
		int BytesReceived = read(ConnectionId,&DataBlock,1);
		switch(BytesReceived) {
			case 0: {
				// Конец передачи данных (клиент разорвал соединение)
				if((*TakenString).find("\n",0)==-1) *TakenString += "\n";	// Если по какой-либо причине не получен конец строки - добавить
				Working = false;
				break;
				}
			case -1: {
				// Ошибка получения данных
				// Очистить строку и продолжать работу с клиентом
				*TakenString = "";
				break;
				}
			default: {
				// Нормальное получения блока данных
				// Складываем блоки в строку, перевод каретки (\r) не учитываем
		    		if(DataBlock!='\r') *TakenString += DataBlock;
				// Если встречается перевод строки (\n), прием строки заканчиваем
				if(DataBlock=='\n') {
					// Принята полная строка данных
//					Message(TakenString);
					// Записать строку текста в канал
					write(OutputChannel,(*TakenString).c_str(),(*TakenString).length()-1);
					// Очистить строку
					*TakenString = "";
					}
				break;
				}
			}
		}
	if(FD_ISSET(InputChannel,&Rset)) {
		// Отправляем данные клиенту
		// Прочитать данные из канала блоками по 1 байт
		int BytesReceived = read(InputChannel,&DataBlock,1);
		switch(BytesReceived) {
			case 0: {
				// Закончена передача данных через канал (основной процесс оборвал работу)
				*SendingString = "";  // Очистим строчку
				Working = false;      // Оборвать работу с клиентом
				break;
				}
			case -1: {
				// Ошибка чтения данных из канала
				// Очистить строку и продолжать работать с клиентом
				*SendingString = "";
				break;
				}
			default: {
				// Нормальное чтение блока данных из канала
				// Складываем блоки в строку, перевод каретки (\r) не учитываем
		    		if(DataBlock!='\r') *SendingString += DataBlock;
				// Если встречается перевод строки (\n), считаем, что прочли из канала всю строку для отправки клиенту
				if(DataBlock=='\n') {
					// Прочитана полная строка из канала - отправить ее клиенту
					write(ConnectionId,(*SendingString).c_str(),(*SendingString).length());
					// Очистить строку
					*SendingString = "";
					}
				break;
				}
			}
		}
	}
	// Если процесс работы с клиентом завершен
	// Удалить строчки для приема/отправки данных
delete TakenString;
TakenString = NULL;
delete SendingString;
SendingString = NULL;
return true;
}
//---------------------------------------------------------------------------
//				СЛУЖЕБНЫЕ ФУНКЦИИ
//---------------------------------------------------------------------------
void TCPSocket::zeromemory(void* ptr, int BytesCount)
{
        // Заполнение участка памяти 0
memset(ptr,0,BytesCount);
}
//---------------------------------------------------------------------------
void TCPSocket::Message(char Msg[])
{
        // Вывод сообщения в лог
syslog(LOG_INFO||LOG_USER,Msg);
}
//---------------------------------------------------------------------------
void TCPSocket::Message(std::string* Msg)
{
        // Вывод сообщения в лог
syslog(LOG_INFO||LOG_USER,(*Msg+"\n").c_str());
}
//---------------------------------------------------------------------------

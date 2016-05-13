#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

#include <lwip/sockets.h>
#include <lwip/raw.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <platform/platform_stdlib.h>

#define TCP_PACKET_COUNT        10000
#define BSD_STACK_SIZE		256

#define HOST_IP "192.168.1.101"
#define REMOTE_IP	((u32_t)0xc0a80165UL)	/*192.168.1.101*/
#define LOCAL_IP	((u32_t)0xc0a80164UL)	/*192.168.1.100*/

unsigned int g_srv_buf_size = 1500;
unsigned int g_cli_buf_size = 1500;
xTaskHandle g_server_task = NULL; 
xTaskHandle g_client_task = NULL;

xTaskHandle udpcllient_task = NULL;
xTaskHandle udpserver_task = NULL;

unsigned char g_start_server = 0;
unsigned char g_start_client = 0;
unsigned char g_terminate = 0;

unsigned char udp_start_server = 0;
unsigned char udp_start_client= 0;
char g_server_ip[16];
unsigned long  g_ulPacketCount = TCP_PACKET_COUNT;

int BsdTcpClient(const char *host_ip, unsigned short usPort)
{
    int                 iCounter;
    short               sTestBufLen;
    struct sockaddr_in  sAddr;
    int                 iAddrSize;
    int                 iSockFD;
    int                 iStatus;
    long                lLoopCount = 0;
	char			*cBsdBuf = NULL;

	if(g_cli_buf_size > 4300)
		g_cli_buf_size = 4300;
	else if (g_cli_buf_size == 0)
		g_cli_buf_size = 1500;
	
	cBsdBuf = pvPortMalloc(g_cli_buf_size);
	if(NULL == cBsdBuf){
		printf("\n\rTCP: Allocate client buffer failed.\n");
		return -1;
	}

	// filling the buffer
	for (iCounter = 0; iCounter < g_cli_buf_size; iCounter++) {
		cBsdBuf[iCounter] = (char)(iCounter % 10);
	}
	sTestBufLen  = g_cli_buf_size;

	//filling the TCP server socket address
	FD_ZERO(&sAddr);
	sAddr.sin_family = AF_INET;
	sAddr.sin_port = htons(usPort);
	sAddr.sin_addr.s_addr = inet_addr(host_ip);

	iAddrSize = sizeof(struct sockaddr_in);

	// creating a TCP socket
	iSockFD = socket(AF_INET, SOCK_STREAM, 0);
	if( iSockFD < 0 ) {
		printf("\n\rTCP ERROR: create tcp client socket fd error!");
		goto Exit1;
	}

	printf("\n\rTCP: ServerIP=%s port=%d.", host_ip, usPort);
	printf("\n\rTCP: Create socket %d.", iSockFD);
	// connecting to TCP server
	iStatus = connect(iSockFD, (struct sockaddr *)&sAddr, iAddrSize);
	if (iStatus < 0) {
		printf("\n\rTCP ERROR: tcp client connect server error! ");
		goto Exit;
	}

	printf("\n\rTCP: Connect server successfully.");
	// sending multiple packets to the TCP server
	while (lLoopCount < g_ulPacketCount && !g_terminate) {
		// sending packet
		iStatus = send(iSockFD, cBsdBuf, sTestBufLen, 0 );
		if( iStatus <= 0 ) {
			printf("\r\nTCP ERROR: tcp client send data error!  iStatus:%d", iStatus);
			goto Exit;
		} 
		lLoopCount++;
		//printf("BsdTcpClient:: send data count:%ld iStatus:%d \n\r", lLoopCount, iStatus);
	}

	printf("\n\rTCP: Sent %u packets successfully.",g_ulPacketCount);

Exit:
	//closing the socket after sending 1000 packets
	close(iSockFD);

Exit1:
	//free buffer
	vPortFree(cBsdBuf);

	return 0;
}

int BsdTcpServer(unsigned short usPort)
{
	struct sockaddr_in  sAddr;
	struct sockaddr_in  sLocalAddr;
	int                 iCounter;
	int                 iAddrSize;
	int                 iSockFD;
	int                 iStatus;
	int                 iNewSockFD;
	long                lLoopCount = 0;
	//long                lNonBlocking = 1;
	int                 iTestBufLen;
	int                 n;
	char			*cBsdBuf = NULL;

	if(g_srv_buf_size > 5000)
		g_srv_buf_size = 5000;
	else if (g_srv_buf_size == 0)
		g_srv_buf_size = 1500;

	cBsdBuf = pvPortMalloc(g_srv_buf_size);
	if(NULL == cBsdBuf){
		printf("\n\rTCP: Allocate server buffer failed.\n");
		return -1;
	}

	// filling the buffer
	for (iCounter = 0; iCounter < g_srv_buf_size; iCounter++) {
		cBsdBuf[iCounter] = (char)(iCounter % 10);
	}
	iTestBufLen  = g_srv_buf_size;

	// creating a TCP socket
	iSockFD = socket(AF_INET, SOCK_STREAM, 0);
	if( iSockFD < 0 ) {
		goto Exit2;
	}

	printf("\n\rTCP: Create server socket %d\n\r", iSockFD);
	n = 1;
	setsockopt( iSockFD, SOL_SOCKET, SO_REUSEADDR,
			(const char *) &n, sizeof( n ) );

	//filling the TCP server socket address
	memset((char *)&sLocalAddr, 0, sizeof(sLocalAddr));
	sLocalAddr.sin_family      = AF_INET;
	sLocalAddr.sin_len         = sizeof(sLocalAddr);
	sLocalAddr.sin_port        = htons(usPort);
	sLocalAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	iAddrSize = sizeof(sLocalAddr);

	// binding the TCP socket to the TCP server address
	iStatus = bind(iSockFD, (struct sockaddr *)&sLocalAddr, iAddrSize);
	if( iStatus < 0 ) {
		printf("\n\rTCP ERROR: bind tcp server socket fd error! ");
		goto Exit1;
	}
	printf("\n\rTCP: Bind successfully.");

	// putting the socket for listening to the incoming TCP connection
	iStatus = listen(iSockFD, 20);
	if( iStatus != 0 ) {
		printf("\n\rTCP ERROR: listen tcp server socket fd error! ");
		goto Exit1;
	}
	printf("\n\rTCP: Listen port %d", usPort);

	// setting socket option to make the socket as non blocking
	//iStatus = setsockopt(iSockFD, SOL_SOCKET, SO_NONBLOCKING, 
	//                        &lNonBlocking, sizeof(lNonBlocking));
	//if( iStatus < 0 ) {
	//    return -1;
	//}
Restart:    
	iNewSockFD = -1;
	lLoopCount = 0;

	// waiting for an incoming TCP connection
	while( iNewSockFD < 0 ) {
		// accepts a connection form a TCP client, if there is any
		// otherwise returns SL_EAGAIN
		int addrlen=sizeof(sAddr);
		iNewSockFD = accept(iSockFD, ( struct sockaddr *)&sAddr, 
		            (socklen_t*)&addrlen);
		if( iNewSockFD < 0 ) {
			printf("\n\rTCP ERROR: Accept tcp client socket fd error! ");
			goto Exit1;
		} 
		printf("\n\rTCP: Accept socket %d successfully.", iNewSockFD);
	}

	// waits packets from the connected TCP client
	while (!g_terminate) {
		iStatus = recv(iNewSockFD, cBsdBuf, iTestBufLen, 0);  //MSG_DONTWAIT   MSG_WAITALL
		if( iStatus < 0 ) {
			printf("\n\rTCP ERROR: server recv data error iStatus:%d ", iStatus);
			goto Exit;
		} else if (iStatus == 0) {
			printf("\n\rTCP: Recieved %u packets successfully.", lLoopCount);
			close(iNewSockFD);
			goto Restart;	
		}
                if(iStatus > 0)
                {
                  printf("receive data");
                }
		lLoopCount++;
	}

Exit:
	// close the connected socket after receiving from connected TCP client
	close(iNewSockFD);

Exit1:
	// close the listening socket
	close(iSockFD);

Exit2:
	//free buffer
	vPortFree(cBsdBuf);

	return 0;
}

static void TcpServerHandler(void *param)
{
	unsigned short port = 5001;
	vTaskDelay(1000);
	printf("\n\rTCP: Start tcp Server!");
        
	if(g_start_server)
		BsdTcpServer(port);
	
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rTCP: Tcp server stopped!");
	g_server_task = NULL;
	vTaskDelete(NULL);
}

static void TcpClientHandler(void *param)
{
	unsigned short port = 5001;
	vTaskDelay(1000);
	printf("\n\rTCP: Start tcp client!");
	if(g_start_client)
		BsdTcpClient(g_server_ip, port);
		
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rTCP: Tcp client stopped!");
	g_client_task = NULL;
	vTaskDelete(NULL);
}



#if 1 //xjx-test
void TcpClientHandler_test(void *param)
{
	unsigned short port = 6666;
	vTaskDelay(10000);
	printf("\n\rTCP: Start tcp client!");
        
        g_start_client = 1; //xjx-test
        
	if(g_start_client)
		BsdTcpClient("192.168.1.176", port);
		
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rTCP: Tcp client stopped!");
	g_client_task = NULL;
	vTaskDelete(NULL);
}


void TcpServerHandler_test(void *param)
{
	unsigned short port = 5555;
	vTaskDelay(10000);
	printf("\n\rTCP: Start tcp Server!");
        g_start_server = 1;
	if(g_start_server)
		BsdTcpServer(port);
	
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes\n\r", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rTCP: Tcp server stopped!");
	g_server_task = NULL;
	vTaskDelete(NULL);
}
#endif


/***************************udp related*********************************/
int udpclient()
{
	int cli_sockfd;
	socklen_t addrlen;
	struct sockaddr_in cli_addr;
	int loop= 0;
	char *buffer ;
//	int delay = 2;


	if(!g_ulPacketCount)
		g_ulPacketCount = 100;

	if(!g_cli_buf_size)
		g_cli_buf_size = 1500;

	buffer = (char*)pvPortMalloc(g_cli_buf_size);
	
	if(NULL == buffer){
		printf("\n\rudpclient: Allocate buffer failed.\n");
		return -1;
	}
	
	/*create socket*/
	memset(buffer, 0, g_cli_buf_size);
	cli_sockfd=socket(AF_INET,SOCK_DGRAM,0);
	if (cli_sockfd<0) {
		printf("create socket failed\r\n\n");
		return 1;
	}
	
	/* fill sockaddr_in*/	
	addrlen=sizeof(struct sockaddr_in);
	memset(&cli_addr, 0, addrlen);
		
	cli_addr.sin_family=AF_INET;
	cli_addr.sin_addr.s_addr=inet_addr(g_server_ip);
	cli_addr.sin_port=htons(5001);

	/* send data to server*/
	while(loop < g_ulPacketCount && !g_terminate) {
		if(sendto(cli_sockfd, buffer, g_cli_buf_size, 0,(struct sockaddr*)&cli_addr, addrlen) < 0) {
// Dynamic delay to prevent send fail due to limited skb, this will degrade throughtput
//			if(delay < 100)
//				delay += 2;
		}

//		vTaskDelay(delay);
		loop++;
	}
	close(cli_sockfd);
	//free buffer
	vPortFree(buffer);
	return 0;
}

int udpserver()
{
	int ser_sockfd;
	socklen_t addrlen;
	struct sockaddr_in ser_addr, peer_addr;
	uint32_t start_time, end_time;
	unsigned char *buffer;
	int total_size = 0, report_interval = 1;

	if (g_srv_buf_size == 0)
		g_srv_buf_size = 1500;

	buffer = pvPortMalloc(g_srv_buf_size);

	if(NULL == buffer){
		printf("\n\rudpclient: Allocate buffer failed.\n");
		return -1;
	}

	/*create socket*/
	ser_sockfd=socket(AF_INET,SOCK_DGRAM,0);
	if (ser_sockfd<0) {
		printf("\n\rudp server success");
		return 1;
	}

	/*fill the socket in*/
	addrlen=sizeof(ser_addr);
	memset(&ser_addr, 0,addrlen);
	ser_addr.sin_family=AF_INET;
	ser_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	ser_addr.sin_port=htons(5001);
	
	/*bind*/
	if (bind(ser_sockfd,(struct sockaddr *)&ser_addr,addrlen)<0) {
		printf("bind failed\r\n");
		return 1;
	}

	start_time = xTaskGetTickCount();
	total_size = 0;

	while(1) {
		int read_size = 0;
		addrlen = sizeof(peer_addr);
		read_size=recvfrom(ser_sockfd,buffer,g_srv_buf_size,0,(struct sockaddr *) &peer_addr,&addrlen);
		if(read_size < 0){
			printf("%s recv error\r\n", __FUNCTION__);
			goto Exit;
		}

		end_time = xTaskGetTickCount();
		total_size += read_size;
		if((end_time - start_time) >= (configTICK_RATE_HZ * report_interval)) {
			printf("\nUDP recv %d bytes in %d ticks, %d Kbits/sec\n", 
				total_size, end_time - start_time, total_size * 8 / 1024 / ((end_time - start_time) / configTICK_RATE_HZ));
			start_time = end_time;
			total_size = 0;
		}

		/*ack data to client*/
// Not send ack to prevent send fail due to limited skb, but it will have warning at iperf client
//		sendto(ser_sockfd,buffer,read_size,0,(struct sockaddr*)&peer_addr,sizeof(peer_addr));
	}

Exit:	
	close(ser_sockfd);
	//free buffer
	vPortFree(buffer);
	return 0;
}

void Udpclienthandler(void *param)
{
	/*here gives the udp demo code*/
	vTaskDelay(1000);
	printf("\n\rUdp client test");
	
	udpclient();	
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rUDP: udp client stopped!");
	udpcllient_task = NULL;
	vTaskDelete(NULL);	
}

void Udpserverhandler(void *param)
{
	/*here gives the udp demo code*/
	vTaskDelay(1000);
	printf("\n\rUdp server test");
	
	udpserver();	
#if defined(INCLUDE_uxTaskGetStackHighWaterMark) && (INCLUDE_uxTaskGetStackHighWaterMark == 1)
	printf("\n\rMin available stack size of %s = %d * %d bytes", __FUNCTION__, uxTaskGetStackHighWaterMark(NULL), sizeof(portBASE_TYPE));
#endif
	printf("\n\rUDP: udp client stopped!");
	udpserver_task = NULL;
	vTaskDelete(NULL);	
}

/***************************end of udp*********************************/
void cmd_tcp(int argc, char **argv)
{
	g_terminate = g_start_server = g_start_client = 0;
	g_ulPacketCount = 10000;
	memset(g_server_ip, 0, 16);

	if(argc < 2)
		goto Exit;
	
	if(strcmp(argv[1], "-s") == 0 ||strcmp(argv[1], "s") == 0)	{
		if(g_server_task){
			printf("\n\rTCP: Tcp Server is already running.");
			return;
		}else{
			g_start_server = 1;
			if(argc == 3)
				g_srv_buf_size = atoi(argv[2]);
		}
	}else if(strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "c") == 0)	{
		if(g_client_task){
			printf("\n\rTCP: Tcp client is already running. Please enter \"tcp stop\" to stop it.");
			return;
		}else{
			if(argc < 4)
				goto Exit;
			g_start_client = 1;
			strncpy(g_server_ip, argv[2], (strlen(argv[2])>16)?16:strlen(argv[2]));
			g_cli_buf_size = atoi(argv[3]);
			if(argc == 5)
				g_ulPacketCount = atoi(argv[4]);
		}

	}else if(strcmp(argv[1], "stop") == 0){
		g_terminate = 1;
	}else
		goto Exit;

	if(g_start_server && (NULL == g_server_task)){
		if(xTaskCreate(TcpServerHandler, "tcp_server", BSD_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &g_server_task) != pdPASS)
			printf("\n\rTCP ERROR: Create tcp server task failed.");
	}
	if(g_start_client && (NULL == g_client_task)){
		if(xTaskCreate(TcpClientHandler, "tcp_client", BSD_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &g_client_task) != pdPASS)
			printf("\n\rTCP ERROR: Create tcp client task failed.");
	}

	return;
Exit:
	printf("\n\rTCP: Tcp test command format error!");
	printf("\n\rPlease Enter: \"tcp -s\" to start tcp server or \"tcp <-c *.*.*.*> <buf len> [count]]\" to start tcp client\n\r");
	return;
}

void cmd_udp(int argc, char **argv)
{
	g_terminate = udp_start_server = udp_start_client = 0;
	g_ulPacketCount = 10000;
	if(argc == 2){
		if(strcmp(argv[1], "-s") == 0 ||strcmp(argv[1], "s") == 0){
			if(udpserver_task){
				printf("\r\nUDP: UDP Server is already running.");
				return;
			}else{
				udp_start_server = 1;
			}
		}else if(strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "c") == 0){
			if(udpcllient_task){
				printf("\r\nUDP: UDP Server is already running.");
				return;
			}else{
				udp_start_client= 1;
			}
		}else if(strcmp(argv[1], "stop") == 0){
			g_terminate = 1;
		}else
			goto Exit;
	}else if(strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "c") == 0)	{
		if(udpcllient_task){
			printf("\n\nUDP: UDP client is already running. Please enter \"udp stop\" to stop it.");
			return;
		}else{
			if(argc < 4)
				goto Exit;
			udp_start_client = 1;
			strncpy(g_server_ip, argv[2], (strlen(argv[2])>16)?16:strlen(argv[2]));
			g_cli_buf_size = atoi(argv[3]);
			if(argc == 5)
				g_ulPacketCount = atoi(argv[4]);
		}

	}else
	    goto Exit;
		
	if(udp_start_server && (NULL == udpserver_task)){
		if(xTaskCreate(Udpserverhandler, "udp_server", BSD_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &udpserver_task) != pdPASS)
			printf("\r\nUDP ERROR: Create udp server task failed.");
	}
	
	if(udp_start_client && (NULL == udpcllient_task)){
		if(xTaskCreate(Udpclienthandler, "udp_client", BSD_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1 + PRIORITIE_OFFSET, &udpcllient_task) != pdPASS)
			printf("\r\nUDP ERROR: Create udp client task failed.");
	}

	return;
Exit:
	printf("\r\nUDP: udp test command format error!");
	printf("\r\nPlease Enter: \"udp -s\" to start udp server or \"udp -c to start udp client\r\n");
	return;
}


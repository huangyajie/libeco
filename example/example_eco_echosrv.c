#include "eco_socket.h"
#include "eloop.h"
#include <arpa/inet.h>



static int tcp_server_init(int port)
{
    int sock;

    struct sockaddr_in servaddr;

    sock = eco_socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    
    const int one = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);

    if(eco_bind(sock, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
    {
        fprintf(stderr,"绑定端口失败\n");
        eco_close(sock);
        return -3;
    }

    if(eco_listen(sock, SOMAXCONN) != 0)
    {
        fprintf(stderr,"listen() 失败\n");
        eco_close(sock);
        return -4;
    }
	
    return sock;
}


static void sock_readwrite_func(struct schedule * sch, void *ud) 
{
	int ret = 0;
    int fd = *((int*)ud);
    char rbuf[256] = {0};
    
    for(;;)
    {
        memset(rbuf,0,sizeof(rbuf));
        ret = eco_read(fd,rbuf,sizeof(rbuf));

        if(ret <= 0)
        {
            //errno == EAGAIN EINTR
            continue;
        }

        fprintf(stderr,"co_id = %d ----- rbuf = %s\n",eco_running_id(sch),rbuf);

        eco_write(fd,rbuf,strlen(rbuf));
    }
    
}


static void sock_accept_func(struct schedule * sch, void *ud) 
{
	int listen_fd = -1;
    int cfd = -1;
    int co_rw = -1;

    struct sockaddr_in sin;
    unsigned int sl = sizeof(struct sockaddr_in);
    
    listen_fd = tcp_server_init(5678);
    if(listen_fd < 0)
    {
        return;
    }

	for(;;)
	{   
        cfd = eco_accept(listen_fd, (struct sockaddr *) &sin, &sl);
        if (cfd < 0) 
        {
            continue;
        }

        fprintf(stderr,"new connection cfd = %d \n",cfd);

        co_rw = eco_create(eco_get_cur_schedule(),sock_readwrite_func,&cfd);
        eco_resume_later(eco_get_cur_schedule(),co_rw);
		
	}
}


int main()
{

    eco_loop_init();

    int co_ac = eco_create(eco_get_cur_schedule(),sock_accept_func,NULL);
    eco_resume(eco_get_cur_schedule(),co_ac);
    
    eco_loop_run();

    eco_loop_exit();
     
    
    return 0;
}
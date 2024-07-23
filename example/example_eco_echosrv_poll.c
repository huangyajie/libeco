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

static void sock_func(struct schedule * sch, void *ud) 
{
	int listen_fd = -1;
	int ret = 0;
    int i = 0;
    int j = 0;
    int nfds = -1;
    int cfd = -1;
    char rbuf[256] = {0};
    struct poll_fd fd_all[1024];
    struct poll_fd fd_in[1024];
    struct poll_fd fd_out[1024];

    memset(fd_in,0,sizeof(fd_all));

    struct sockaddr_in sin;
    memset(&sin,0,sizeof(sin));
    unsigned int sl = sizeof(struct sockaddr_in);
    
    listen_fd = tcp_server_init(5678);
    if(listen_fd < 0)
    {
        return;
    }
    fd_all[listen_fd].fd = listen_fd;
    fd_all[listen_fd].events = ELOOP_READ;

	for(;;)
	{   
        j = 0;
        memset(fd_in,0,sizeof(fd_in));
        memset(fd_out,0,sizeof(fd_out));
        for(i = 0;i < sizeof(fd_all)/sizeof(fd_all[0]);i++)
        {
            if(fd_all[i].fd > 0)
            {
                fd_in[j].fd = fd_all[i].fd;
                fd_in[j].events = fd_all[i].events;
                j++;
            }
        }

        
        nfds = eco_poll(fd_in,j,fd_out,sizeof(fd_out)/sizeof(fd_out[0]),1000);
        if(nfds <= 0)
        {
            continue;
        }


        for(i = 0;i < nfds;i++)
        {
            if(fd_out[i].fd == listen_fd)  //新连接到来
            {
                
                cfd = eco_accept(fd_out[i].fd, (struct sockaddr *) &sin, &sl);
                if (cfd < 0) 
                {
                    fprintf(stderr, "Accept failed\n");
                    continue;
                }
                fprintf(stderr,"new connection cfd = %d \n",cfd);
                if(cfd > sizeof(fd_out)/sizeof(fd_out[0]) - 1)
                {
                    eco_close(cfd);
                    continue;
                }
                fd_all[cfd].fd = cfd;
                fd_all[cfd].events = ELOOP_READ;
            }
            else  //读写事件 
            {
                memset(rbuf,0,sizeof(rbuf));
                ret = eco_read(fd_out[i].fd,rbuf,sizeof(rbuf));
 
                if(ret <= 0)
                {
                    //errno == EAGAIN EINTR
                    fd_all[fd_out[i].fd].fd = -1;
                    fd_all[fd_out[i].fd].events = 0 ;
                    eco_close(fd_out[i].fd);
                    continue;
                }

                fprintf(stderr,"rbuf = %s --- %d\n",rbuf,i);

                eco_write(fd_out[i].fd,rbuf,strlen(rbuf));
            }
        }
		
	}
}


int main()
{

    eco_loop_init();

    int co = eco_create(eco_get_cur_schedule(),sock_func,NULL);
    eco_resume(eco_get_cur_schedule(),co);
    
    eco_loop_run();

    eco_loop_exit();
     
    
    return 0;
}
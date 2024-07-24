#include "eco_socket.h"
#include "eloop.h"
#include <arpa/inet.h>
#include <pthread.h>


static void sock_func(struct schedule * sch, void *ud) 
{
    fprintf(stderr,"sock_func >>>>>>>> \n");
    int fd = -1;
    int ret = 0;
    int i = 0;
    char buf[256] = {0};
    char rbuf[256] = {0};
    for(;;)
    {
        if ( fd < 0 )
        {
            fd = eco_socket(PF_INET, SOCK_STREAM, 0);
            struct sockaddr_in addr;
            bzero(&addr,sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(5678);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            ret = eco_connect(fd,(struct sockaddr*)&addr,sizeof(addr));

            if(ret > 0)
            {
                fprintf(stderr,"connect success fd = %d \n",ret);
            }
            else
            {
                eco_close(fd);
                fd = -1;
                eco_sleep(3);
                fprintf(stderr,"connect failed,reconnecting ...\n");
                continue;
            }
                        
        }

        snprintf(buf,sizeof(buf),"hello:%d",i++);
        eco_write(fd,buf,strlen(buf));
        eco_read(fd,rbuf,sizeof(rbuf));
        fprintf(stderr,"rbuf = %s\n",rbuf);

        eco_msleep(1000);           

    }

    eco_close(fd);
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
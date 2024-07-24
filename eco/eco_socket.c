#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "eloop.h"
#include "eco.h"
#include "eco_socket.h"

#define ECO_SOCKET_CONNECT_TIMEOUT 5000 //ms
#define ECO_SOCKET_READ_TIMEOUT  1000
#define ECO_SOCKET_WRITE_TIMEOUT  1000


struct eco_base
{
    struct eloop_base* ebase;
    struct schedule* sch;    
};

struct poll_ctx
{
    struct eco_base* cur_eco_base;
    struct eloop_fd efd;  //事件管理
    unsigned int cur_event;  //当前监听的事件
    struct eloop_timeout *timer;  //超时检测定时器
    int pending_id;  //挂起协程id
    int resume_id;   //需要唤醒协程id
    int res;  //用于存放协程返回时结果 
};




static __thread struct eco_base* g_cur_eco_base = NULL;  //每个线程独立


static int _eco_poll(int fd,unsigned int events,int timeout);

static struct eco_base* _get_cur_eco_base()
{
    return g_cur_eco_base;
}

static  int _set_cur_eco_base(struct eco_base* eco_base)
{
    if(eco_base == NULL)
    {
        return -1;
    }
    g_cur_eco_base = eco_base;
    return 0;
}

struct schedule* eco_get_cur_schedule()
{
    return g_cur_eco_base->sch;
}

//创建socket
int eco_socket(int domain, int type, int protocol)
{
    int fd = socket(domain,type,protocol);
    if(fd < 0)
    {
        return -1;
    }

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0) | O_NONBLOCK);

    return fd;
}


//绑定
// int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int eco_bind(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(fd,addr,addrlen);
}


//监听
//int listen(int fd,int backlog);
int eco_listen(int fd,int backlog)
{
    return listen(fd,backlog);
}

//接受连接
// int accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int eco_accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{

    int ret = _eco_poll(fd,ELOOP_READ,ECO_SOCKET_READ_TIMEOUT);
    if(ret <= 0)
    {
        return -1;
    }

    ret = accept(fd,addr,addrlen);
    if(ret < 0)
    {
        //errno EAGAIN EMFILE  
        return -1;
    }

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0) | O_NONBLOCK);
    
    return ret;

}



//建立连接
// int connect(int fd, const struct sockaddr *address, socklen_t address_len)
int eco_connect(int fd, const struct sockaddr *address, socklen_t address_len)
{
    int ret = -1;
    ret = connect(fd,address,address_len);
    if(ret < 0)
    {
        if(errno == EINPROGRESS)
        {
            ret = _eco_poll(fd,ELOOP_WRITE,ECO_SOCKET_CONNECT_TIMEOUT);
            if(ret <= 0)
            {
                return -1;
            }

            int err = 0;
            socklen_t errlen = sizeof(err);
            ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
            if (ret < 0) 
            {
                return -1;
            } 
            
            if (err != 0) 
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }

    }

    fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0) | O_NONBLOCK);
    return fd;

}


// ssize_t read( int fd, void *buf, size_t nbyte )
ssize_t eco_read(int fd, void *buf, size_t nbyte)
{
    int ret = -1;
    
    #if 0   //nbyte可能会大于实际要读的数据，循环读存在一直等待的问题，写不存在此问题，可以循环写
    int index = 0;
    while (nbyte - index > 0)
    {
        ret = _eco_poll(fd,ELOOP_READ,ECO_SOCKET_READ_TIMEOUT);
        if(ret <= 0)
        {
            continue;
        }
        ret = read(fd,buf+index,nbyte-index);
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            else if(errno == EAGAIN)
            {
                break;
            }

            return -1;  //链接异常断开
        }
        else if(ret == 0)
        {
            return 0;  //对方主动断开链接
        }
        else
        {
            index += ret;
        }
    }
  
    return index;
    #endif

    ret = _eco_poll(fd,ELOOP_READ,ECO_SOCKET_READ_TIMEOUT);
    if(ret <= 0)
    {
        return -1;
    }

    return read(fd,buf,nbyte);
}


// ssize_t write( int fd, const void *buf, size_t nbyte )
ssize_t eco_write(int fd, const void *buf, size_t nbyte)
{
    int ret = -1;
    int index = 0;
    while (nbyte - index > 0)
    {
        ret = write(fd,buf+index,nbyte-index);
        if(ret < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            else if(errno == EAGAIN)
            {
                _eco_poll(fd,ELOOP_WRITE,ECO_SOCKET_WRITE_TIMEOUT);
                continue;
            }

            return -1;  //链接异常断开
        }
        else if(ret == 0)
        {
            return 0;  //对方主动断开链接
        }
        else
        {
            index += ret;
        }
    }
    
    
    return index;
}

//关闭socket
int eco_close(int fd)
{
    return close(fd);
}




// unsigned int sleep(unsigned int seconds);
unsigned int eco_sleep(unsigned int seconds)   //单位ms
{
    return eco_msleep(seconds*1000);
}

static int _eco_poll_timeout(int timeout,void* ud);

unsigned int eco_msleep(unsigned int  msecs)   //单位ms
{
    int ret = _eco_poll_timeout(msecs,NULL);
    if(ret < 0)
    {
        return -1;
    }

    return 0;
}

static void _timer_cb (struct eloop_base* base,struct eloop_timeout *t)
{
    struct poll_ctx* pctx = (struct poll_ctx*)t->priv;
    pctx->res = 0;
    eco_resume(pctx->cur_eco_base->sch,pctx->pending_id);
}

static void _ev_cb (struct eloop_base* base,struct eloop_fd *efd, unsigned int events)
{
    struct poll_ctx* pctx = (struct poll_ctx*)efd->priv;
    if(!(pctx->cur_event & events))  //不是监听的事件，不唤醒协程
    {
        return;
    }

    pctx->res = 1;
    eloop_timeout_cancel(base,pctx->timer);
    eco_resume(pctx->cur_eco_base->sch,pctx->pending_id);
}

//-1:失败 0：超时  1：事件被触发
static int _eco_poll(int fd,unsigned int events,int timeout)
{
    struct  poll_fd pfd_in[1];
    struct  poll_fd pfd_out[1];
    pfd_in[0].fd = fd;
    pfd_in[0].events = events;

    return eco_poll(pfd_in,1,pfd_out,1,timeout);
}

//-1:失败 0：超时  > 0：事件被触发 (1个或多个)
int eco_poll(struct poll_fd* in,unsigned int nfds,struct poll_fd* out,unsigned int out_sz,int timeout)
{
    if((in == NULL) || (out == NULL))
    {
        return -1;
    }

    int i = 0;
    int ret = 0;
    struct eco_base* cur_eco_base = _get_cur_eco_base();
    struct poll_ctx* pctx = (struct poll_ctx*)calloc(nfds,sizeof(struct poll_ctx));
    if(pctx == NULL)
    {
        return -1;
    }

    struct eloop_timeout *timer = (struct eloop_timeout *)calloc(1,sizeof(struct eloop_timeout));
    if(timer == NULL)
    {
        free(pctx);
        pctx = NULL;
        return -1;
    }

    for(i = 0;i < nfds;i++)
    {
        pctx[i].cur_eco_base = cur_eco_base;
        pctx[i].pending_id = eco_running_id(cur_eco_base->sch);
        pctx[i].cur_event = in[i].events;
        pctx[i].efd.fd = in[i].fd;
        pctx[i].efd.cb = _ev_cb;
        pctx[i].efd.priv = &pctx[i];
        pctx[i].timer = timer;
        
        eloop_fd_add(cur_eco_base->ebase,&pctx[i].efd,pctx[i].cur_event);
    }

    timer->cb = _timer_cb;
    timer->priv = &pctx[0] ;
    eloop_timeout_set(cur_eco_base->ebase,timer,timeout);

    
    eco_yield(cur_eco_base->sch);

    //资源清理、返回
    for(i = 0;i < nfds;i++)
    {
        if(pctx[i].res == 1)
        {
            ret = eloop_get_trigger_events(cur_eco_base->ebase,out,out_sz);
            break;
        }
    }

    for(i = 0;i < nfds;i++)
    {
        eloop_fd_delete(cur_eco_base->ebase,&pctx[i].efd); 
    }

    free(timer);
    timer = NULL;

    free(pctx);
    pctx = NULL;

    return ret;
}



static void _eco_rusume_timer_cb (struct eloop_base* base,struct eloop_timeout *t)
{
    struct poll_ctx* pctx = (struct poll_ctx*)t->priv;
    if(pctx->resume_id >= 0)
    {
        eco_resume(pctx->cur_eco_base->sch,pctx->resume_id);  //启动新协程
    }
    
    eco_resume(pctx->cur_eco_base->sch,pctx->pending_id); //返回挂起协程
}

//-1:失败 0：成功 
static int _eco_poll_timeout(int timeout,void* ud)
{
    int ret = 0;
    struct eco_base* cur_eco_base = _get_cur_eco_base();
    
    struct poll_ctx* pctx = (struct poll_ctx*)calloc(1,sizeof(struct poll_ctx));
    if(pctx == NULL)
    {
        return -1;
    }
    
    pctx->resume_id = -1;

    struct eloop_timeout *timer = (struct eloop_timeout *)calloc(1,sizeof(struct eloop_timeout));
    if(timer == NULL)
    {
        free(pctx);
        pctx = NULL;
        return -1;
    }

    pctx->cur_eco_base = cur_eco_base;
    pctx->pending_id = eco_running_id(cur_eco_base->sch);
    if(ud != NULL)
    {
        pctx->resume_id = *((int*)ud);
    }
    pctx->timer = timer;
    timer->cb = _eco_rusume_timer_cb;
    timer->priv = pctx;
    eloop_timeout_set(cur_eco_base->ebase,timer,timeout);

    eco_yield(cur_eco_base->sch);
 
    free(timer);
    timer = NULL;

    free(pctx);
    pctx = NULL;

    return ret;
}


int eco_resume_later(struct schedule* sch,int id)
{
    //sch 暂时没有使用,与eco_resume接口保持统一
    return _eco_poll_timeout(1,&id);
}

int eco_loop_init()
{
    struct eco_base* base = (struct eco_base*)calloc(1,sizeof(struct eco_base));
    if(base == NULL)
    {
        return -1;
    }

    base->ebase = eloop_init();
    if(base->ebase == NULL)
    {
        free(base);
        return -1;
    }

    base->sch = eco_init();
    if(base->sch == NULL)
    {
        free(base->ebase);
        free(base);
        return -1;
    }

    _set_cur_eco_base(base);

    return 0;
}

int eco_loop_run()
{
    eloop_run(_get_cur_eco_base()->ebase);
    return 0;
}

int eco_loop_exit()
{
    
    eloop_done(_get_cur_eco_base()->ebase);
    eco_exit(_get_cur_eco_base()->sch);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ucontext.h>
#include "eco.h"


#define STACK_SIZE 1024*1024
#define DEFAULT_COROUTINE_NUM 32



struct coroutine
{
    char stack[STACK_SIZE];
    int status;
    coroutine_func func;
    void* ud;
    ucontext_t ctx;
    struct schedule* sch;
};

struct schedule
{
    ucontext_t main;
    int nco;
    int cap;
    int runnig_id;
    struct coroutine** co;
};

//初始化调度器
struct schedule* eco_init()
{
    struct schedule* sch = (struct schedule*)calloc(1,sizeof(struct schedule));
    if(sch == NULL)
    {
        return NULL;
    }

    sch->nco = 0;
    sch->cap = DEFAULT_COROUTINE_NUM;
    sch->runnig_id = -1;
    sch->co = (struct coroutine**) calloc(sch->cap,sizeof(struct coroutine*));
    if(sch->co == NULL)
    {
        return NULL;
    }

    return sch;
}

//创建协程
int eco_create(struct schedule* sch,coroutine_func func,void* ud)
{
    if(sch == NULL)
    {
        return -1;
    }

    if(sch->nco >= sch->cap) 
    {
        return -1;
    }

    int i = 0;
    struct coroutine* co = (struct coroutine*)calloc(1,sizeof(struct coroutine));
    if(co == NULL)
    {
        return -1;
    }

    co->status = COROUTINE_READY;
    co->func = func;
    co->ud = ud;
    co->sch = sch;


    for (i = 0; i < sch->cap; i++)
    {
        if(sch->co[i] != NULL)
        {
            continue;
        }
        break;
    }

    sch->co[i] = co;
    sch->nco++;
    
    return i;
}

static void co_delete(struct schedule* sch,int id)
{
    free(sch->co[id]);
    sch->co[id] = NULL;
    sch->runnig_id = -1;
    sch->nco--;
}

static void mainfunc(uint32_t low32,uint32_t hi32)
{
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    struct schedule* sch = (struct schedule*) ptr;

    int id = sch->runnig_id;

    struct coroutine* co = sch->co[id];
    co->func(sch,co->ud);
    co->status = COROUTINE_DEAD;
    //co_delete(sch,id); //对于独立栈不能在这里free,此时栈还在占用,放到resume返回清理
}

//协程执行
int eco_resume(struct schedule* sch,int id)
{
    if(sch == NULL)
    {
        return -1;
    }

    if((id < 0) || (id > sch->cap-1) || (sch->runnig_id > 0))
    {
        return -1;
    } 

    int ret = 0;
    struct coroutine* co = sch->co[id]; 
    if(co == NULL)
    {
        return -1;
    }


    if(co->status == COROUTINE_READY)
    {
        getcontext(&co->ctx);
        co->ctx.uc_stack.ss_sp = co->stack;
        co->ctx.uc_stack.ss_size = sizeof(co->stack);
        co->ctx.uc_link = &sch->main;
        co->status = COROUTINE_RUNNING;
        sch->runnig_id = id;
        uintptr_t ptr = (uintptr_t)sch;
		makecontext(&co->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
		swapcontext(&sch->main, &co->ctx);
        if(co->status == COROUTINE_DEAD)
        {
            co_delete(sch,id);
        }
        
    }
    else if (co->status == COROUTINE_SUSPEND)
    {
        sch->runnig_id = id;
        co->status = COROUTINE_RUNNING;
		swapcontext(&sch->main, &co->ctx);
        if(co->status == COROUTINE_DEAD)
        {
            co_delete(sch,id);
        }
    }
    else
    {
        ret = -1;
    }
    
    return ret;
}

//协程挂起
int eco_yield(struct schedule* sch)
{
    if(sch == NULL)
    {
        return -1;
    }

    if(sch->runnig_id < 0)
    {
        return -1;
    }

    struct coroutine* co = sch->co[sch->runnig_id]; 
    co->status = COROUTINE_SUSPEND;
    sch->runnig_id = -1;
    swapcontext(&co->ctx,&sch->main);

    return 0;
}

//协程当前状态
int eco_status(struct schedule* sch,int id)
{
    if(sch == NULL)
    {
        return COROUTINE_DEAD;
    }

    if((id < 0) || (id > sch->cap-1) || (sch->co[id] == NULL))
    {
        return COROUTINE_DEAD;
    } 
    
    return sch->co[id]->status;
}

//当前运行协程ID
int eco_running_id(struct schedule* sch)
{
    if(sch == NULL)
    {
        return -1;
    }

    return sch->runnig_id;
}

//调度器退出
int eco_exit(struct schedule* sch)
{
    if(sch == NULL)
    {
        return -1;
    }

    int i = 0;

    for(i = 0; i < sch->cap; i++)
    {
        if(sch->co[i] != NULL)
        {
            free(sch->co[i]);
            sch->co[i] = NULL;
        }
    }

    free(sch->co);
    sch->co = NULL;

    free(sch);
    sch = NULL;

    return 0;
}


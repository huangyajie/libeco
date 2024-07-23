#include "eco.h"

#include <stdio.h>

struct args 
{
	int n;
};

static void foo(struct schedule * S, void *ud) 
{
	struct args * arg = ud;
	int start = arg->n;
	int i;
	for (i=0;i<5;i++) {
		printf("coroutine %d : %d\n",eco_running_id(S) , start + i);
		eco_yield(S);
	}
}

static void test(struct schedule *S) 
{
	struct args arg1 = { 0 };
	struct args arg2 = { 100 };

	int co1 = eco_create(S, foo, &arg1);
	int co2 = eco_create(S, foo, &arg2);
	printf("main start\n");
	while (eco_status(S,co1) && eco_status(S,co2)) 
	{
		eco_resume(S,co1);
		eco_resume(S,co2);
	} 
	printf("main end\n");
}



int main() 
{
	struct schedule * S = eco_init();
	test(S);
	eco_exit(S);
	
	return 0;
}


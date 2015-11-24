#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#define max_threads 20
#define stackSize 1<<16
#define maxs 25

struct mythread{
	ucontext_t * context;
	int id;
	int prior;
};

struct sigaction act;
struct mythread * currentThread,*Main;
struct itimerval timer,it;
int flag=0;

void block(){
	sigfillset(&act.sa_mask);
	sigprocmask(SIG_BLOCK,&act.sa_mask,NULL);
}

void unblock(){
	sigprocmask(SIG_UNBLOCK,&act.sa_mask,NULL);
	sigemptyset(&act.sa_mask);
}

struct heap{
private:
	int curr=1;
	struct mythread * arr[maxs];
public:
	void insert(struct mythread * thread){
		block();
		if(curr==max_threads){
			unblock();
			return -1;
		}
		initTimer(timer);
		arr[curr++]=thread;
		heapify(curr);
		unblock();
		return 1;
	}
	void heapify(int val){
		if(val==1){
			return ;
		}
		struct mythread *parent=arr[curr/2],*child=arr[curr];
		if(child->prior>parent->prior){
			swap(arr[curr],arr[curr/2]);
		}
		heapify(curr/2);
	}
	void revHeapify(int val){
		if((val*2)>=curr){
			return ;
		}
		int l=val*2,r=val*2+1;
		if(l<curr && r<curr){
			if(arr[l]->prior>=arr[r]->prior){
				if(arr[l]->prior>arr[val]->prior){
					swap(arr[l],arr[val]);
					revHeapify(l);
				}
				else{
					return;
				}
			}
			else{
				if(arr[r]->prior>arr[val]->prior){
					swap(arr[r],arr[val]);
					revHeapify(r);
				}
				else{
					return;
				}
			}
		}
		else{
			if(arr[l]->prior>arr[val]->prior){
				swap(arr[l],arr[val]);
				revHeapify(l);
			}
			else{
				return;
			}
		}
	}
	struct mythread* getNext(){
		block();
		if(curr==1){
			unblock();
			return NULL;
		}
		struct mythread * ret=arr[1];
		curr--;
		arr[1]=arr[curr];
		revHeapify(1);
		unblock();
		return ret;
	}
}tree;

void set_stack_values(ucontext_t * context){
	context->uc_stack.ss_sp=malloc(stackSize);
	context->uc_stack.ss_size=stackSize;
	context->uc_stack.ss_flags=0;
}

void initialize(struct mythread * thread){
	thread->context=(ucontext_t *)malloc(sizeof(ucontext_t));
	getcontext(thread->context);
	set_stack_values(thread->context);
	ucontext_t * temp;
	getcontext(temp);
	temp->uc_link=0;
	set_stack_values(temp);
	makecontext(temp,kill_thread,0);
	thread->context.uc_link=temp;
}

void initTimer(struct itimerval it){
	struct sigaction act, oact;
	act.sa_handler=(void*)&schedule;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPROF, &act, &oact);
	it.it_interval.tv_sec=0;
	it.it_interval.tv_usec=0;
	it.it_value.tv_sec=0;
	it.it_value.tv_usec =100000;
	setitimer(ITIMER_PROF, &it, NULL);
}


int create_thread(struct mythread * thread, void (*fun)(),void* arg){
	if(flag==0){
		flag=1;
		getcontext(Main);
		set_stack_values(Main);
	}
	initialize(thread);
	thread->prior=abs(rand()*1007)%20+1;
	makecontext(thread->context,fun,1,arg);
	return tree.insert(thread);
}

void schedule(void ){
	block();
	struct mythread * next=tree.getNext();
	if(next==NULL){
		it.it_interval.tv_sec=0;
	 	it.it_interval.tv_usec=0;
	 	it.it_value.tv_sec=0;
	 	it.it_value.tv_usec =0;
	 	unblock();
	 	setitimer(ITIMER_PROF, &it, NULL);
		setcontext(Main->context);
	}
}


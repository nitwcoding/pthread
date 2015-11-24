#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>

#define max_threads 10
#define ssize 60000

struct mythread{
	int s_id;
	ucontext_t *context;
};

struct queue{
	struct queue* next;
	struct mythread *thread;
}*head,*tail;

struct thread_status{
	int s_id;
	struct thread_status * next;
};

struct sigaction act;
ucontext_t Main;
struct itimerval it;
struct itimerval timer;
struct mythread * current_thread;
struct thread_status * running,*waiting;
int id,count;

void killThread();
void delete_running(int t_id,struct thread_status * head);
void initTimer(struct itimerval it);
void schedule(void);

int mythread_self(){
	return current_thread->s_id;
}

void block(){
	sigfillset(&act.sa_mask);
	sigprocmask(SIG_BLOCK,&act.sa_mask,NULL);
}

void unblock(){
	sigprocmask(SIG_UNBLOCK,&act.sa_mask,NULL);
	sigemptyset(&act.sa_mask);
}


void init(struct mythread * newThread){
	newThread->context=(ucontext_t *)malloc(sizeof(ucontext_t));
	getcontext(newThread->context);
	ucontext_t *temp;
	getcontext(temp);
	temp->uc_stack.ss_sp=malloc(ssize);
	temp->uc_stack.ss_size=ssize;
	temp->uc_stack.ss_flags=0;
	makecontext(temp,killThread,0);
	newThread->context->uc_link=temp;
	newThread->context->uc_stack.ss_sp=malloc(ssize);
	newThread->context->uc_stack.ss_size=ssize;
	newThread->context->uc_stack.ss_flags=0;
}


void addRunning(int newId){
	struct thread_status * temp=(struct thread_status*)malloc(sizeof(struct thread_status));
	temp->next=running;
	temp->s_id=newId;
	running=temp;
	return ;
}

int enqueue(struct mythread *newThread,int flag){
	block();
	if(head==NULL){
		initTimer(timer);
		struct mythread *temp=(struct mythread*)malloc(sizeof(struct mythread));
		init(temp);
		temp->s_id=-1;
		head=(struct queue*)malloc(sizeof(struct queue));
		tail=(struct queue*)malloc(sizeof(struct queue));
		head->thread=temp;
		tail->next=NULL;
		head->next=tail;
		tail->thread=newThread;
		newThread->s_id=id;
		id++;
		count++;
		addRunning(newThread->s_id);
		current_thread=head->thread;
		unblock();
		return 1;
	}
	else{
		if(count==max_threads){
			unblock();
			return -1;
		}
		else{
			struct queue*temp=(struct queue*)malloc(sizeof(struct queue));
			temp->next=NULL;
			temp->thread=newThread;
			if(flag){
				newThread->s_id=id;
				id++;
				count++;
				addRunning(newThread->s_id);
			}
			tail->next=temp;
			tail=tail->next;
			if(flag){
				unblock();
				return 0;
			}
		}
	}
}

int thread_create(struct mythread * newThread, void (*fun)(), void *arg){
	init(newThread);
	makecontext(newThread->context,fun,1,arg);
	int ret=enqueue(newThread,1);
	return ret;
}

void initTimer(struct itimerval it){
	struct sigaction act, oact;
	act.sa_handler=(void*)&schedule;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGPROF, &act, &oact);
	it.it_interval.tv_sec=0;
	it.it_interval.tv_usec=1000;
	it.it_value.tv_sec=0;
	it.it_value.tv_usec =5000;
	setitimer(ITIMER_PROF, &it, NULL);
}

void killThread(){
	block();
	ucontext_t temp;
	if(head->next->next==NULL){
		current_thread=NULL;
		delete_running(head->thread->s_id,running);
		it.it_interval.tv_sec=0;
		it.it_interval.tv_usec=0;
		it.it_value.tv_sec=0;
		it.it_value.tv_usec=0;
		fflush(stdout);
		count--;
		ucontext_t * main1;
		main1=head->next->thread->context;
		free(head->next);
		free(head);
		head=NULL;
		setitimer(ITIMER_PROF,&it,NULL);
		unblock();
		swapcontext(&temp,main1);
		return ;
	}
	else
	{
		current_thread=head->next->thread;
		delete_running(head->thread->s_id,running);
		struct queue * temp1=head;
		head=head->next;
		free(temp1);
		count--;
		unblock();
		fflush(stdout);    
		swapcontext(&temp,current_thread->context);
	}
}

void delete_running(int t_id,struct thread_status * head){
	struct thread_status * temp=head;
	if(temp->s_id==t_id){
		running=temp->next;
		free(temp);
	}
	else{
		while(temp->next!=NULL){
			if(temp->next->s_id==t_id){
				struct thread_status * temp2=temp->next;
				temp->next=temp->next->next;
				free(temp2);
				return ;
			}
			temp=temp->next;
		}
	}
}

void schedule(void){
	block();
	if(current_thread==NULL){
		current_thread=head->thread;
		unblock();
		swapcontext(&Main,current_thread->context);
	}
	else{
		if(head->next==NULL){
			it.it_interval.tv_sec=0;
	 		it.it_interval.tv_usec=0;
	 		it.it_value.tv_sec=0;
	 		it.it_value.tv_usec =0;
	 		struct queue * temp=head;
	 		head=head->next;
	 		unblock();
	 		setitimer(ITIMER_PROF, &it, NULL);
			setcontext(temp->thread->context);
		}
		else{
			current_thread=head->next->thread;
			printf("%d scheduling thread ",current_thread->s_id);
			struct mythread * t=head->thread;
			struct queue * temp=head;
			enqueue(t,0);
			head=head->next;
			free(temp);
			unblock();
			swapcontext(t->context,current_thread->context);
		}
	}
}

void thread_wait(struct mythread * newThread){
	while(if_running(newThread->s_id,running)){
		schedule();
	}
}

int if_running(int t_id,struct thread_status * newThread){
	struct thread_status * temp=newThread;
	while(temp!=NULL){
		if(temp->s_id==t_id){
			return 1;
		}
		temp=temp->next;
	}
	return 0;
}

struct mythread_mutex{
	int lock;
	int owner;
	struct thread_status * head,*tail;
};

void mythread_mutex_init(struct mythread_mutex * mutex){
	mutex->lock=0;
	mutex->owner=-1;
}

int add_to_waiting(struct mythread_mutex * mutex,int s_id){
	if(mutex->head==NULL){
		mutex->head=(struct thread_status *)malloc(sizeof(struct thread_status));
		mutex->tail=mutex->head;
		mutex->head->s_id=s_id;
		mutex->head->next=NULL;
		mutex->tail->next=NULL;
	}
	else{
		struct thread_status * temp=(struct thread_status *)malloc(sizeof(struct thread_status));
		temp->next=NULL;
		temp->s_id=s_id;
		mutex->tail->next=temp;
		mutex->tail=temp;
	}
}


int mythread_mutex_lock(struct mythread_mutex * mutex){
	block();
	if(mutex->lock==0){
		mutex->lock=1;
		mutex->owner=mythread_self();
		mutex->head=(struct thread_status *)malloc(sizeof(struct thread_status));
		mutex->tail=mutex->head;
		mutex->head->s_id=mythread_self();
		mutex->head->next=NULL;
		mutex->tail->next=NULL;
		unblock();
		return 0;
	}
	else if(mutex->owner==mythread_self()){
		unblock();
		return -1;
	}
	else{
		add_to_waiting(mutex,mythread_self());
		while(1){
			if(mutex->owner!=mythread_self()){
				schedule();
				block();
			}
			else{
				break;
			}
		}
		unblock();
	}
}


int mythread_mutex_unlock(struct mythread_mutex * mutex){
	block();
	if(mutex->owner==mythread_self()){
		mutex->lock=0;
		if(mutex->head==NULL){
			unblock();
			return 0;
		}
		else{
			struct thread_status * temp=mutex->head;
			mutex->head=mutex->head->next;
			free(temp);
			if(mutex->head==NULL){
				unblock();
				return 0;
			}
			else{
				mutex->lock=1;
				mutex->owner=mutex->head->s_id;
				unblock();
				return 0;
			}
		}
	}
	else{
		unblock();
		return -1;
	}
}

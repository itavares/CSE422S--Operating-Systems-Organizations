#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/kthread.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include<linux/slab.h> 
#include <linux/delay.h>
#include <linux/time.h>

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Lab 02");
MODULE_DESCRIPTION ("Find Primes");

static ulong num_threads = 1;
static ulong upper_bound = 10;

module_param(num_threads, ulong, 0);
MODULE_PARM_DESC(num_threads, "number of threads");

module_param(upper_bound, ulong, 0);
MODULE_PARM_DESC(upper_bound, "upper bound");

DEFINE_MUTEX(curr_lock);
DEFINE_SPINLOCK(barrier1_lock);
DEFINE_SPINLOCK(barrier2_lock);
//unsigned long flags1;
//unsigned long flags2;

static int *counters;
atomic_t *numbers;
static int curr_pos;
atomic_t a_complete;

atomic_t barrier1_state;
atomic_t barrier2_state;
int num_thread_reached1;
int num_thread_reached2;

struct timeval time1, time2, time3;

static void first_barrier(void){
	//spin_lock_irqsave(&barrier1_lock, flags1);
	spin_lock(&barrier1_lock);
	num_thread_reached1++;
	if(num_thread_reached1 == num_threads){
		printk("First barrier reached\n");
		do_gettimeofday(&time2);
		atomic_set(&barrier1_state,1);
	}
	//spin_unlock_irqrestore(&barrier1_lock, flags1);
	spin_unlock(&barrier1_lock);
}

static void second_barrier(void){
       	//spin_lock_irqsave(&barrier2_lock, flags2);
       	spin_lock(&barrier2_lock);
	num_thread_reached2++;
        if(num_thread_reached2 == num_threads){
                printk("Second barrier reached\n");
                do_gettimeofday(&time3);
                atomic_set(&barrier2_state,1);
        }
	spin_unlock(&barrier2_lock);
}

static void mark_non_primes(int *counters){
	int i;
	int my_curr_pos, curr_num;
	bool finish=false;

	while(!finish){
		//store and update current
		mutex_lock(&curr_lock);
		my_curr_pos = curr_pos;
		curr_pos++;
		//keep looking for next number which is not crossed out by any thread
		while(curr_pos< upper_bound-1){
			if(atomic_read(&numbers[curr_pos]) != 0){
				break;
			}
			curr_pos++;
		}
		mutex_unlock(&curr_lock);
		//end of array is reached
		if(my_curr_pos >= upper_bound-1){
			finish = true;
		}else{
		
			//crossed out non-prime number
			curr_num = atomic_read(&numbers[my_curr_pos]);
			for( i=my_curr_pos+curr_num; i < upper_bound-1; i=i+curr_num){
				atomic_set(&numbers[i],0);
				counters[0]++;//update thread's counter
			}
		}
	}
}

static int do_work(void *counter)
{
	first_barrier();
	while(atomic_read(&barrier1_state)==0);

	mark_non_primes(counter);

	second_barrier();
        while(atomic_read(&barrier2_state) == 0);

	atomic_add(1,&a_complete);
	
	return 0;
}

static int simple_init(void)
{
	int i;
	bool needExit = true;
	atomic_set(&a_complete,0);//not finished yet
	do_gettimeofday(&time1);
    	//printk(KERN_INFO "Monitor module loaded\n");
	if(num_threads<1 && upper_bound<2){
		printk(KERN_ERR "Num threads %lu should be 1 or greater\n", num_threads);
		printk(KERN_ERR "Upper bound %lu should be 2 or greater\n", upper_bound);
	}else if(num_threads<1){
		printk(KERN_ERR "Num threads %lu should be 1 or greater\n", num_threads);
	}else if(upper_bound<2){
		printk(KERN_ERR "Upper bound %lu should be 2 or greater\n", upper_bound);
	}else{
		needExit = false;
	}
	if(needExit){
        	numbers = 0;
        	counters = 0;
        	upper_bound = 0;
        	atomic_set(&a_complete,num_threads);
        	return -1;
	}
    		
	numbers = (atomic_t*)kmalloc((upper_bound-1)*sizeof(atomic_t), GFP_KERNEL);
	if(!numbers){
		numbers =0;
		counters=0;
		printk(KERN_ERR "Error creating number array\n");
		return -1;
	}	

	counters = kmalloc((num_threads)*sizeof(int), GFP_KERNEL);
	if(!counters){
		printk(KERN_ERR "Error creating counter array\n");
		kfree(numbers);
		numbers=0;
		counters=0;
		return -1;
	}

	for(i = 0; i < num_threads; i++ ){
		counters[i] = 0;
	}

	for(i = 0; i < upper_bound-1; i++ ){
		atomic_set(&numbers[i],i+2);
	}

	curr_pos = 0;
	atomic_set(&barrier1_state,0);
	atomic_set(&barrier2_state,0);
	struct task_struct *tasks[num_threads];
	for(i = 0; i < num_threads; i++ ){
		tasks[i] = kthread_create(do_work, (void *)&counters[i],"thread"+i);
	}

	for(i = 0; i < num_threads; i++ ){
		wake_up_process(tasks[i]);
	}
		
	return 0;
}

static void simple_exit(void)
{
	/*int i;
	int ret;
	for(i = 0; i < num_threads; i++ ){
		ret = kthread_stop(tasks[i]);
		if(ret != -EINTR){
			printk(KERN_INFO "Thread % has stopped\n", i);	
		}
	}*/

	if(atomic_read(&a_complete) < num_threads){
		printk(KERN_INFO "Processing has not completed %d\n", atomic_read(&a_complete));
		msleep(100);
		return;
	}

	int i, prime_count = 0, line_limit = 8;
	for(i = 0; i < upper_bound -1 ;i++){
		if(atomic_read(&numbers[i]) != 0){
			printk(KERN_CONT "%d ", atomic_read(&numbers[i]));
			prime_count++;
			if(prime_count%line_limit==0){
				printk(KERN_CONT "\n");
			}
		}
	}
	printk(KERN_CONT "\n");
        int total_crossed_out = 0;
        for(i = 0; i < num_threads; i++ ){
                printk(KERN_CONT "Counters %d [%d]\n", i, counters[i]);
                total_crossed_out += counters[i];
        }

	printk(KERN_INFO "Num of primes:  %d,  Num of Non-Primes: %lu\n", prime_count , (upper_bound-1-prime_count));
	printk(KERN_INFO "Num of crossed out: %lu \n", (total_crossed_out-(upper_bound-1-prime_count)));

	printk(KERN_INFO "Num threads %lu, Upper bound %lu \n", num_threads, upper_bound);
	printk(KERN_INFO "Init time: %lu seconds, %lu u_seconds\n", time2.tv_sec - time1.tv_sec, time2.tv_usec - time1.tv_usec);
	printk(KERN_INFO "Process time: %lu seconds, %lu u_seconds\n", time3.tv_sec - time2.tv_sec, time3.tv_usec - time2.tv_usec);

	kfree(numbers);
	kfree(counters);
	printk(KERN_INFO "Monitor module unloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);

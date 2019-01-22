#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/jiffies.h> 
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/sched.h>

MODULE_LICENSE ("GPL");
MODULE_AUTHOR ("Lab 01");
MODULE_DESCRIPTION ("Single Thread Monitor Module");

static ulong log_sec = 1;
static ulong log_nsec = 0;

module_param(log_sec, ulong, 0);
MODULE_PARM_DESC(log_sec, "delay in seconds");

module_param(log_nsec, ulong, 0);
MODULE_PARM_DESC(log_nsec, "delay in nanoseconds");

static ktime_t ktime;
static struct hrtimer timer;
static struct task_struct *ts0;
static struct task_struct *ts1;
static struct task_struct *ts2;
static struct task_struct *ts3;

static enum hrtimer_restart repeat_timer( struct hrtimer *timer)
{
    	printk(KERN_INFO "Timer is restarted\n");
		wake_up_process(ts0);	
		wake_up_process(ts1);	
		wake_up_process(ts2);	
		wake_up_process(ts3);	
		hrtimer_forward_now(timer, ktime);
        return HRTIMER_RESTART;
}

static int do_work(void *unused)
{
	int iters=1;
    	printk(KERN_INFO "do_work runs on cpu %d\n", smp_processor_id());
	while(1){
		printk(KERN_INFO "Iters %d pid %d on CPU%d nvcsw: %lu nivcsw: %lu\n", 
				iters, current->pid, smp_processor_id(), 
				current->nvcsw, current->nivcsw);
		iters++;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if(kthread_should_stop()) {
		    	printk(KERN_INFO "Thread will stop\n");				
			do_exit(0);
		}
	}
	return 0;
}

static int simple_init(void)
{
    printk(KERN_INFO "Monitor module loaded (%d)\n", current->pid);
	
	printk(KERN_INFO "Delay in seconds %lu\n", log_sec);
	printk(KERN_INFO "Delay in nanoseconds %lu\n", log_nsec);
    	
	ktime = ktime_set(log_sec, log_nsec);
	hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	timer.function = &repeat_timer;

	hrtimer_start(&timer, ktime, HRTIMER_MODE_REL);
	
	ts0 = kthread_create(do_work, NULL, "thread0");
	ts1 = kthread_create(do_work, NULL, "thread1");
	ts2 = kthread_create(do_work, NULL, "thread2");
	ts3 = kthread_create(do_work, NULL, "thread3");
	kthread_bind(ts0,0);
	kthread_bind(ts1,1);
	kthread_bind(ts2,2);
	kthread_bind(ts3,3);
		
	return 0;
}

static void simple_exit(void)
{

	int ret = hrtimer_cancel(&timer);
	if(ret){
		printk(KERN_ERR "Timer still running\n");
	}

	ret = kthread_stop(ts0);
	if(ret != -EINTR){
		printk(KERN_INFO "Thread 0 has stopped\n");	
	}
 	
	ret = kthread_stop(ts1);
        if(ret != -EINTR){
                printk(KERN_INFO "Thread 1 has stopped\n");
        }

 	ret = kthread_stop(ts2);
        if(ret != -EINTR){
                printk(KERN_INFO "Thread 2 has stopped\n");
        }

 	ret = kthread_stop(ts3);
        if(ret != -EINTR){
                printk(KERN_INFO "Thread 3 has stopped\n");
        }

	printk(KERN_INFO "Monitor module unloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);

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
static struct task_struct *ts;

static enum hrtimer_restart repeat_timer( struct hrtimer *timer)
{
    	printk(KERN_INFO "Timer is restarted\n");
		wake_up_process(ts);	
		hrtimer_forward_now(timer, ktime);
        return HRTIMER_RESTART;
}

static int do_work(void *unused)
{
	int iters=1;
    	printk(KERN_INFO "do_work\n");
	while(1){
		printk(KERN_INFO "Iters %d pid %d ppid %d nvcsw: %lu nivcsw: %lu\n", 
				iters, current->pid, current->real_parent->pid, 
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
	
	ts = kthread_run(do_work, NULL, "do_work");
	if (IS_ERR(ts)) {
		printk(KERN_ERR "Error! kthread_run\n");
		return PTR_ERR(ts);
	}

	return 0;
}

static void simple_exit(void)
{

	int ret = hrtimer_cancel(&timer);
	if(ret){
		printk(KERN_ERR "Timer still running\n");
	}

	ret = kthread_stop(ts);
	if(ret != -EINTR){
		printk(KERN_INFO "Thread has stopped\n");	
	}

	printk(KERN_INFO "Monitor module unloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);

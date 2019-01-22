CSE422 Lab01
1.
    Authors:
            Ighor Tavares
            Funda Atik

2. Module Design
    For each parameter for the module we had to first define them as static ulong variables. 
    Once declared, we implemented them to be parameters using the module_param for each of them. 
    A description for each parameter was also added with the macro MODULE_PARAM_DESC.

3. Timer Design and Evaluation
    A static variable and a static enum were declared for ktime_t and hrtimer. 
    These were used in the enumeration created to reschedule the timer’s next expiration one timer 
    interval forward into the future.This function takes the declared static struct hrtimer as its 
    only parameters and returns HRTIMER_RESTART that let the kernel know the timer is being restarted.  
    In the init() function,  the unsigned long variables are passed into ktime_set() which the result 
    is stored in the static variable type ktime initially declared.  Then, we initialize the hrtimer, 
    with its init() function using CLOCK_MONOTONIC. In the exit function of our module, we define 
    an int variable to hold the return value of hrtimer_cancel() function and check if the time still 
    running before terminating. 

    $sudo insmod monitor_mod.ko log_sec=0 log_nsec=100000000 (0.1s)
        Sep 25 18:27:51 itavaresPi kernel: [ 4253.296790] Timer is restarted.
        Sep 25 18:27:51 itavaresPi kernel: [ 4253.396792] Timer is restarted.
        Sep 25 18:27:51 itavaresPi kernel: [ 4253.496790] Timer is restarted.
        Sep 25 18:27:51 itavaresPi kernel: [ 4253.596792] Timer is restarted.
        Sep 25 18:27:51 itavaresPi kernel: [ 4253.696791] Timer is restarted.

    $sudo insmod monitor_mod.ko (1s)
        Sep 25 18:24:14 itavaresPi kernel: [ 4036.334153] Timer is restarted.
        Sep 25 18:24:15 itavaresPi kernel: [ 4037.334167] Timer is restarted.
        Sep 25 18:24:16 itavaresPi kernel: [ 4038.334164] Timer is restarted.
        Sep 25 18:24:17 itavaresPi kernel: [ 4039.334173] Timer is restarted.
        Sep 25 18:24:18 itavaresPi kernel: [ 4040.334174] Timer is restarted.
        Sep 25 18:24:19 itavaresPi kernel: [ 4041.334175] Timer is restarted.

    There is a slight variation of  1µs every time the timer is rescheduled for 0.1s, and this variation 
    is larger (max:9µs, min: 1ms, avg 3.5µ) for a timer which is rescheduled for 1s. 

4. Thread Design and Evaluation
    For this portion of the project, we declared a static struct task_struct pointer that holds a pointer 
    to the current task_struct of the module’s thread. Another static function (do_work)  is declared within 
    the module that returns an integer and it takes a single void * parameter ( which we named unused). 
    The objective of this function is to run in the kernel threads once they are awakened by the module. 
    Inside of this new function, we only print the name of the function to show that it runs and it returns 0 
    to indicate success. The init and exit functions for the module are modified where kthread_run is added 
    to init, so we can spawn the kernel thread we just defined and used kthread_stop in the exit to stop 
    the execution of the thread.

    In the static thread function, we added a loop that prints system log messages indication another 
    iteration of the loop has started and in which interaction it is (we declared an integer to keep 
    track of the iterations). Within the loop, we set the current state of the task into INTERRUPTIBLE, 
    and right after, call schedule function to suspend its execution, waiting for the next piece of code 
    to wake up. We also added kthread_should_stop() to test whether or not it should terminate the thread. 
    At this point, we changed the timer expiration function to wake up a thread every time its called by 
    the module with wake_up_process() being called inside of the function. 

    $sudo insmod monitor_mod.ko log_sec=0 log_nsec=100000000 (0.1s)
        fatik kernel: [ 3134.850981] Iters 142 pid 1984 ppid 2 nvcsw: 142 nivcsw: 0
        fatik kernel: [ 3134.950936] Timer is restarted
        fatik kernel: [ 3134.950988] Iters 143 pid 1984 ppid 2 nvcsw: 143 nivcsw: 0
        fatik kernel: [ 3135.050935] Timer is restarted
        fatik kernel: [ 3135.050976] Iters 144 pid 1984 ppid 2 nvcsw: 144 nivcsw: 0
        fatik kernel: [ 3135.150936] Timer is restarted
        fatik kernel: [ 3135.150997] Iters 145 pid 1984 ppid 2 nvcsw: 145 nivcsw: 0
        fatik kernel: [ 3135.250937] Timer is restarted

    $sudo insmod monitor_mod.ko (1s)
        fatik kernel: [15824.419034] Iters 3 pid 6068 ppid 2 nvcsw: 2 nivcsw: 1
        fatik kernel: [15825.418588] Timer is restarted
        fatik kernel: [15825.418653] Iters 4 pid 6068 ppid 2 nvcsw: 3 nivcsw: 1
        fatik kernel: [15826.418588] Timer is restarted
        fatik kernel: [15826.418629] Iters 5 pid 6068 ppid 2 nvcsw: 4 nivcsw: 1
        fatik kernel: [15827.418596] Timer is restarted
        fatik kernel: [15827.418660] Iters 6 pid 6068 ppid 2 nvcsw: 5 nivcsw: 1
        fatik kernel: [15828.418603] Timer is restarted
        fatik kernel: [15828.418672] Iters 7 pid 6068 ppid 2 nvcsw: 6 nivcsw: 1
        fatik kernel: [15829.418603] Timer is restarted
        fatik kernel: [15829.418669] Iters 8 pid 6068 ppid 2 nvcsw: 7 nivcsw: 1

    For a timer, which is scheduled for 0.1s (first example), the variations in the interval between 
    rescheduled timers are about 2µs, and the variations between the timing of each loop’s iteration 
    are are between from 0µs to 44µs (avg. 7µs) for 100 iterations. In the second example, when the 
    timer is restarted at each 1s period, the variations in the interval between timers become around 
    (~3.5µs), and the timing of loop’s iteration varies about between from 0µs to 55µs (avg. 15µs) 
    for 100 iterations. As can be seen the results, the kernel thread of our module is preempted 
    voluntarily. At each iteration, the number of voluntary context switches (nvcsw) is increased although 
    the number of involuntary context switches (nivcsw) does not change.

5. Multi-threading Design and Evaluation
    For the multithreading module, 4 task_struct pointers were declared for four different kernel threads. 
    The implementation of creating a thread in the init() function was changed, and instead of calling 
    kthread_run, kthread_create() function was used to create a kernel thread and call the do_work() 
    static function to indicate the thread was initialized. In addition to calling kthread_create(), 
    it was required to call kthread_bind() function to bind the thread to a specific CPU. This process of 
    creating and binding a thread was repeated four times (for each thread).  The exit() function was also 
    altered, so when called, it removes each thread previously created by calling kthread_stop() for each 
    thread checking if they were actually stopped.  Finally, in the enum repeat_timer() function, each thread 
    was awakened with wake_up_process every time the timer is restarted. 

    $sudo insmod mt_monitor_mod.ko log_sec=0 log_nsec=10000000 (0.1s)
        atik kernel: [ 5963.334607] Iters 712 pid 3066 on CPU1 nvcsw: 712 nivcsw: 0
        atik kernel: [ 5963.334614] Iters 712 pid 3065 on CPU0 nvcsw: 712 nivcsw: 0
        fatik kernel: [ 5963.334621] Iters 712 pid 3067 on CPU2 nvcsw: 712 nivcsw: 1
        fatik kernel: [ 5963.334629] Iters 712 pid 3068 on CPU3 nvcsw: 712 nivcsw: 0
        fatik kernel: [ 5963.344570] Timer is restarted
        fatik kernel: [ 5963.344615] Iters 713 pid 3066 on CPU1 nvcsw: 713 nivcsw: 0
        fatik kernel: [ 5963.344622] Iters 713 pid 3067 on CPU2 nvcsw: 713 nivcsw: 1
        fatik kernel: [ 5963.344629] Iters 713 pid 3065 on CPU0 nvcsw: 713 nivcsw: 0
        atik kernel: [ 5963.344636] Iters 713 pid 3068 on CPU3 nvcsw: 713 nivcsw: 0
        atik kernel: [ 5963.354568] Timer is restarted
        fatik kernel: [ 5963.354606] Iters 714 pid 3066 on CPU1 nvcsw: 714 nivcsw: 0
        fatik kernel: [ 5963.354613] Iters 714 pid 3067 on CPU2 nvcsw: 714 nivcsw: 1
        fatik kernel: [ 5963.354620] Iters 714 pid 3065 on CPU0 nvcsw: 714 nivcsw: 0
        fatik kernel: [ 5963.354628] Iters 714 pid 3068 on CPU3 nvcsw: 714 nivcsw: 0
    
    $sudo insmod mt_monitor_mod.ko (1s)
        [17925.736311] Timer is restarted
        [17925.736357] Iters 299 pid 6821 on CPU0 nvcsw: 299 nivcsw: 0
        [17925.736364] Iters 299 pid 6822 on CPU1 nvcsw: 299 nivcsw: 0
        [17925.736370] Iters 299 pid 6824 on CPU3 nvcsw: 299 nivcsw: 1
        [17925.736377] Iters 299 pid 6823 on CPU2 nvcsw: 299 nivcsw: 4
        [17925.836310] Timer is restarted
        [17925.836355] Iters 300 pid 6824 on CPU3 nvcsw: 300 nivcsw: 1
        [17925.836362] Iters 300 pid 6823 on CPU2 nvcsw: 300 nivcsw: 4
        [17925.836369] Iters 300 pid 6821 on CPU0 nvcsw: 300 nivcsw: 0
        [17925.836377] Iters 300 pid 6822 on CPU1 nvcsw: 300 nivcsw: 0
        [17925.936317] Timer is restarted
        [17925.936380] Iters 301 pid 6821 on CPU0 nvcsw: 301 nivcsw: 0
        [17925.936388] Iters 301 pid 6823 on CPU2 nvcsw: 301 nivcsw: 4
        [17925.936395] Iters 301 pid 6824 on CPU3 nvcsw: 301 nivcsw: 1
        [17925.936415] Iters 301 pid 6822 on CPU1 nvcsw: 301 nivcsw: 0
                    
    When the specified interval is 0.1s, the timing of loop’s iteration varies like that:
        - For thread/0, average variation is 10µs and max variation 60µs.
        - For thread/1, average variation is 38µs and max variation 1345µs.
        - For thread/2, average variation is 9µs and max variation 34µs.
        - For thread/3, average variation is 13µs and max variation 125µs.

    The average variation of four threads is about 18µs, whereas the average variation for 
    a single-threaded case is about 7µs (for 100 iterations and 0.1s timer interval). We expect 
    this result since multi-threaded programs have extra overheads for managing threads. Similar 
    to the single-threaded case, all threads increment their voluntary context switches count at 
    each iteration. Moreover, only 2 threads (thread/2 and thread/3) experience involuntary preemption 
    in multi-threaded case. However, we can say that involuntary preemption happens more frequently in 
    multi-threaded programs than single-threaded one.

    When the specified interval is 1s, the timing of loop’s iteration varies like that:
        - For thread/0, average variation is 44µs and max variation 1433µs.
        - For thread/1, average variation is 14µs and max variation 51µs.
        - For thread/2, average variation is 13µs and max variation 47µs.
        - For thread/3, average variation is 14µs and max variation 11µs.

    The average variation of four threads is about 21µs, whereas the average variation for a single-threaded 
    case is about 15µs (for 100 iterations and 1s timer interval). When a timer scheduled with a long interval, 
    similar to the single-threaded case, the overall average variation increases. 

6. 
    * Look "trace.png" image.
    
7. System Performance
    After zooming in on several the threads wakeups, we observe that all threads run to completion every time. 
    Kernel preemption has been introduced in 2.6 kernels so high priority threads like logger or interrupts might 
    preempt the current task that is running on the kernel.  

    * Look "total_exec.png" image.

    - The execution time for the first wakeup is 57 µs.
    - The execution time for the second wakeup is 29 µs.
    - The execution time for the third wakeup is 48 µs.

    * jitter.png

    For one thread group, 
        - Min jitter is 14µs
        - Max jitter is 30µs
        - Average jitter is 21.7µs
    
    The actual running time over five wakeups of thread/0 is like that:
        - Mean: 24µs
        - Max: 56µs
        - Min: 1µs

8. Development Effort
    About 16 person hours.

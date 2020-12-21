            +--------------------+
            |        CS 140      |
            | PROJECT 1: THREADS |
            |   DESIGN DOCUMENT  |
            +--------------------+

---- GROUP ----


>> Fill in the names and email addresses of your group members.

Yining She <sheyn@shanghaitech.edu.cn>
Shaoting Peng <pengsht@shanghaitech.edu.cn>

---- PRELIMINARIES ----

>> If you have any preliminary comments on your submission, notes for the
>> TAs, or extra credit, please give them here.

>> Please cite any offline or online sources you consulted while
>> preparing your submission, other than the Pintos documentation, course
>> text, lecture notes, and course staff.

Online source: https://www.cnblogs.com/laiy/p/pintos_project1_thread.html

                 ALARM CLOCK
                 ===========

---- DATA STRUCTURES ----

>> A1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

In `thread.h’, struct `thread’:

struct thread {
	// ....

	int64_t sleep_ticks;
	// record one thread’s total ticks of sleep
}

---- ALGORITHMS ----

>> A2: Briefly describe what happens in a call to timer_sleep(),
>> including the effects of the timer interrupt handler.

First, use `start’ to record the number of timer ticks since then OS boosted, and the `old_level’ sentences are used to ensure the current code between can’t be interrupted.

Originally, the `while’ sentence keeps yielding the current thread to realize sleeping, changing the thread between the `ready_list’ and `running_list’, which wastes lots of CPU resources.

To avoid busy waiting, we record the total time the current thread is going to sleep and call `thread_block()’, which sets the current thread to `blocked’ and re-schedules the next thread to run.

In `timer_interrupt()’, we check for each thread’s sleeping times, and unblock the thread if it has reached its `sleep_ticks’ (i.e. `wake it up!’).


>> A3: What steps are taken to minimize the amount of time spent in
>> the timer interrupt handler?



---- SYNCHRONIZATION ----

>> A4: How are race conditions avoided when multiple threads call
>> timer_sleep() simultaneously?

When timer_sleep() disabled interrupt, other threads which are calling this function will be stored in a list.

>> A5: How are race conditions avoided when a timer interrupt occurs
>> during a call to timer_sleep()?

The following lines of code in `timer_sleep()’ show an atomic operation which can’t be interrupted:

	enum intr_level old_level;
	old_level = intr_disable ();   
	......
	intr_set_level (old_level);

As explained above.

---- RATIONALE ----

>> A6: Why did you choose this design?  In what ways is it superior to
>> another design you considered?



             PRIORITY SCHEDULING
             ===================

---- DATA STRUCTURES ----

>> B1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

In `thread.h’, struct `thread’:

struct thread {
	// ....

	struct list locks_have;
	// To record a list of locks the thread owns

   	struct lock *lock_waiting;
	// To record the lock that the thread is waiting for (if any)

   	int origin_priority;
	// The priority before donation
}

In `synch.h’, struct `lock’:

struct lock {
	// ....

	int priority;
	// the max priority among the current holder 
	// and the threads acquiring this lock

	struct list_elem elem;
	// used to maintain a priority queue
}

In `synch.h’, struct `semaphore’:

struct semaphore {
	// ....
	
	struct lock *owner;
	// record who owns the semaphore
}

>> B2: Explain the data structure used to track priority donation.
>> Use ASCII art to diagram a nested donation.  (Alternately, submit a
>> .png file.)

In the following explanation, we have: 
- Thread t1 with low priority, owns lock l1;
- Thread t2 with med priority, owns lock l2 and acquire l1;
- Thread t3 with high priority, acquire l2;

1. t2 try to acquire l1:

   Donation: t1->priority = t2->priority = med;
				___________         ___________     ________________
		    	| t2 (med) | donate | t1 (low) | => | t1 (low->med) |
   			    |__________|        |__________|    |_______________|
2. t3 tries to acquire l2:

   Donation1: t2->priority = t3->priority = high;
			    _____________        ___________     _________________
			    | t3 (high) | donate | t2 (med) | => | t2 (med->high) |
			    |___________|        |__________|    |________________|
   Donation2: t1->priority = t2->priority = high;
				_________________         ________________     ______________________
			    | t2 (med->high) | donate | t1 (low->med) | => | t1 (low->med->high) |
			    |________________|        |_______________|    |_____________________|

Explanation: Because t3 hold l2, and l2->holder is acquiring l1 (i.e. `holder->lock_waiting != NULL’, in function `donate_priority()’, thread.c, line 680), recursively do donation from the `current holder’ to `new holder’. That’s how a nested donation works.

---- ALGORITHMS ----

>> B3: How do you ensure that the highest priority thread waiting for
>> a lock, semaphore, or condition variable wakes up first?

In `thread.c’ and `thread.h’, we reimplement the `list_push_back()’ into `list_inserted_ordered()’ using ` comparator_thread_priority()’ to maintain a priority queue, thus we can get the thread owning the highest priority from `list_begin()’.

>> B4: Describe the sequence of events when a call to lock_acquire()
>> causes a priority donation.  How is nested donation handled?

Sequence of events:

0. Check for `thread_mlfqs’ first! If it’s true, we can’t do priority donation! (It troubled us for a long time in mission3 to find a problem in mission2)

1. If a priority donation is caused, it means the current thread has a higher priority than the lock holder. We write a recursive function `donate_priority()’ to handler the problem. 

2. As for the current lock, we update the priorities of threads which are trying to acquire this lock (i.e. in the test `priority-donate-one’), as well as other locks acquired by those threads (if any) (i.e. in the test `priority-donate-multiple’). And then update the priority related to those locks by calling this function again until there’s no new holders any more.

3. After changing the priority, life has to go on. Semaphore is used to realize a lock, so we get into `sema_down()’.

4. In `sema_down()’, we first disable the interrupt, then check for the sema’s value.

5. Because the lock is currently being held by some other thread, the value should be zero and then the current thread is inserted into `waiters’ according to its priority, then set the status to `BLOCKED’ and waiting for the lock to be released.

Nested donation is handled in step2 by the recursive function `donate_priority()’.

>> B5: Describe the sequence of events when lock_release() is called
>> on a lock that a higher-priority thread is waiting for.

Sequence of events:

0. Check for `thread_mlfqs’ first as above...

1. Update the lock holder to NULL, and call `restore_priority()’ to get back to the priority before donation.

2. Because in `locks_have’, we store the locks according to their priorities DESC, the first element in the list should be the max priority, then update the thread’s priority to it.

3. `sema_down()’ is actually how a lock is released:, unblock the first thread (with the highest priority) in the `waiters’, and increase the sema’s value by one.

4. The current thread should yield the CPU!

---- SYNCHRONIZATION ----

>> B6: Describe a potential race in thread_set_priority() and explain
>> how your implementation avoids it.  Can you use a lock to avoid
>> this race?

---- RATIONALE ----

>> B7: Why did you choose this design?  In what ways is it superior to
>> another design you considered?

We decompose the whole process into three parts: “before `sema_down()’”, “after `sema_down()’”, and “lock_release()”; and we have three variables need to be considered: “current thread”, “lock holder” and “lock”, each with their own struct members. And we draw a graph:


              ADVANCED SCHEDULER
              ==================

---- DATA STRUCTURES ----

>> C1: Copy here the declaration of each new or changed `struct' or
>> `struct' member, global or static variable, `typedef', or
>> enumeration.  Identify the purpose of each in 25 words or less.

In `thread.h’, struct `thread’:

struct thread {
	// ....

	int nice;
	// record nice value

	int64_t recent_cpu;
	// record recent cpu (signed long)
}


---- ALGORITHMS ----

>> C2: Suppose threads A, B, and C have nice values 0, 1, and 2.  Each
>> has a recent_cpu value of 0.  Fill in the table below showing the
>> scheduling decision and the priority and recent_cpu values for each
>> thread after each given number of timer ticks:

timer  recent_cpu    priority   thread
ticks   A   B   C   A   B   C   to run
-----  --  --  --  --  --  --   ------
 0		0	0	0  63  61  59		A
 4		4	0	0  62  61  59		A
 8		8	0	0  61  61  59		B
12		8	4	0  61  60  59		A
16	   12	4	0  60  60  59		B
20	   12	8	0  60  59  59		A
24	   16	8	0  59  59  59		C
28	   16	8	4  59  59  58		B
32	   16  16	4  59  58  58		A
36	   20  16	4  58  58  58		C


>> C3: Did any ambiguities in the scheduler specification make values
>> in the table uncertain?  If so, what rule did you use to resolve
>> them?  Does this match the behavior of your scheduler?

Yes, it did. Within the implementation, there would be ambiguities due to the order of threads with same priority, \
whether switch thread when current thread's priority is equal to the max priority in the ready_list, and so on.

We choose to insert the current thread to ready list and pop out a new one after updating priority at any cases.
And when inserting a thread, it should be inserted after all the threads with same priority if there are.

In our program, we would call thread_yield() no matter whether current thread's priority is lower than the max priority in the ready_list.\
Moreover, the comparator function we used in list_insert_ordered() doesn't return true when equal, which leads to that the newly inserted element\
will always be inserted after the elements with same priority while before all elements with lower priority.
So the rule we choose match the behavior of your scheduler.


>> C4: How is the way you divided the cost of scheduling between code
>> inside and outside interrupt context likely to affect performance?

We only do necessary operations inside the interrupt context, like updating load_avg, recent_cpu and threads' priorities. Any other operations are done outside interrupt context.
In this way, we can keep the time of executing interrupt handler as short as possible to improve average performance.

---- RATIONALE ----

>> C5: Briefly critique your design, pointing out advantages and
>> disadvantages in your design choices.  If you were to have extra
>> time to work on this part of the project, how might you choose to
>> refine or improve your design?

Our design basicly meets the project requirement and passes all testcases.
However, for an operating system, just passing the test cases provided is not enough actually. We think our design is likely to crash if used in daily life.

When coding, we try our best to change the struct type given as little as possible to keep the system stable. Besides, we write many support funtions to make our design modularized while keeping the code readable.

Given extra time, we would like to design more extreme testcases to test the correctness of our design and fix bugs. Correctness is given the highest priority. If we can, we would alse improve the structure of our code to make it more clear and efficient.

>> C6: The assignment explains arithmetic for fixed-point math in
>> detail, but it leaves it open to you to implement it.  Why did you
>> decide to implement it the way you did?  If you created an
>> abstraction layer for fixed-point math, that is, an abstract data
>> type and/or a set of functions or macros to manipulate fixed-point
>> numbers, why did you do so?  If not, why not?

We use a set of macros to implement arithmetic for fixed-point math. Since each arithmetic operation is simple and short, using macros to define them in several lines would keep code clean and easy to read, and also takes less time.

               SURVEY QUESTIONS
               ================

Answering these questions is optional, but it will help us improve the
course in future quarters.  Feel free to tell us anything you
want--these questions are just to spur your thoughts.  You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

>> In your opinion, was this assignment, or any one of the three problems
>> in it, too easy or too hard?  Did it take too long or too little time?

>> Did you find that working on a particular part of the assignment gave
>> you greater insight into some aspect of OS design?

>> Is there some particular fact or hint we should give students in
>> future quarters to help them solve the problems?  Conversely, did you
>> find any of our guidance to be misleading?

>> Do you have any suggestions for the TAs to more effectively assist
>> students, either for future quarters or the remaining projects?

>> Any other comments?
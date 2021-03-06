# COMP SCI 537 Discussion Week 5

The plan for this week:
- [Recap](https://github.com/akshatgit/CS537-Discussion-sp22-2/tree/main/week-3#how-xv6-starts): How xv6 starts
- [Recap](https://github.com/akshatgit/CS537-Discussion-sp22-2/tree/main/week-3#scheduler-logic-in-xv6): Scheduler Logic in xv6, old discussion video by [Remzi](https://www.youtube.com/watch?v=eYfeOT1QYmg)
- How to approach P2-A?
- `sleep()`: is not really just sleeping 

## How to approach P2-A?
- Implement easy syscalls first: For e.g. Implement `settickets`, `srand` and `getpinfo` first. 
- Test each part independently. 
- - Create custom test user applications. For e.g. when you implement the first 3 syscalls, create a C file, spawn a child process with `fork`, set it's tickets(`settickets`) and fetch proc table details using `getpinfo`. 
- - Debug any issue with GDB (Can't stress this more!). 
- Start Early! (Biggest Hint!)

## `sleep()`: is not really just sleeping

There are typically three cases of *voluntary blocking* of a user process in xv6:

* It calls the `sleep(num_ticks)` syscall, which will be served by the in-kernel function `sys_sleep()`
* It calls the `wait()` syscall, trying to wait for a child process to finish
* It tries to do `read()` on a blocking pipe - a mechanism for doing inter-process communications

A slightly confusing naming here: all these three situations will eventually call an internal helper function, `proc.c: sleep(chan)`, that performs the blocking. Though named `sleep`, this internal helper function is used not only by `sys_sleep()`, but also by all other mechanisms of blocking. So `proc.c:sleep(chan)` function in xv6 is a terrible name... What it does is more like "waiting" or "blocking" instead of sleeping.

```C
// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}
```

What does this do? `sleep()` works just like `pthread_cond_wait`: it requires the function caller to first acquire a lock (first argument `lk`) that guards a shared data structure `chan` (the second argument); then `sleep()` puts the current process into `SLEEPING` state and *then* release the lock. 

What does this mean (what is the semantics of this function)? "Channel" here is a waiting mechanism which could be any address. When process A updates a data structure and expects that some other processes could be waiting on a change of this data structure, A can scan and check other SLEEPING processes' `chan; `if process B's `chan` holds the address of the data structure process A just updated, then A would wake B up.

It might be helpful to read `sleep()` with its inverse function `wakeup()`:

```C
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}
```

Now let's take a closer look at `sleep()`:

```C
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  // [skip some checking]...

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // [Mark as sleeping and then call sched()]
  // [At some point this process gets waken up and resume]

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}
```

You may notice it has a conditional checking to see if `lk` refers to `ptable.lock`. Why? Pause here for a second to think about it. Hint: we are going to call `sched()` in the middle.

[Spoiler Alert!]

The reason is, `sched()` requires `ptable.lock` gets held. So here we are actually dealing with two locks: `lk` held by caller and `ptable.lock` that `sleep()` should hold. In a boundary case, these two locks are the same, which acquire special handling and that is why see the condition checking above.

```C
// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
    // ...
}
```

Remember this condition checking... It will save you from a lot of kernel panic we promise...

## `sys_sleep()`: A Use Case of `sleep()`

After understanding `sleep()`, we are now able to understand how `sys_sleep()` works.

```C
// In sysproc.c
int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}
```

Here the global variable `ticks` is used as the wait channel (recall global variable `ticks` is defined in `trap.c`), and the corresponding guard lock is `ticklock`.

```C
// In trap.c
struct spinlock tickslock;
uint ticks;

void
trap(struct trapframe *tf)
{
  // ... 
  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  // ...
  }
  // ...
}
```

So what really happens is: a process that calls **`sleep()` syscall** will be wakened up every time that the global variable `ticks` changes. It then replies on the while loop in `sys_sleep()` to ensure it has slept long enough.

```C
  while(ticks - ticks0 < n){
    // ...
    sleep(&ticks, &tickslock);
  }
```

This is not really a smart implementation of `sleep` syscall, because this process will jump between `RUNNABLE`, `RUNNING`, and `SLEEPING` back and forth until it sleeps long enough, which is inefficient and will mess up our compensation mechanism.

To make it more efficient, you need to make `wakeup1()` smarter and treat a process that waits on `ticks` differently to avoid falsely wake up.

For example, one implementation could be:

```C
// In sysproc.c
int
sys_sleep(void)
{
  // ...
  acquire(&tickslock);
  ticks0 = ticks;

  // Before sleep, mark this process's some fields to denote when this process should wake up
  MARK_SOME_FIELDS_OF_PROC();
  sleep(&ticks, &tickslock);
 
  release(&tickslock);
}

// In proc.c
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {
      // Add more checking to see are we really going to wake this process up
      if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
    }
}
```

## One More (Important) Hint

Whenever you need to access the global variable `ticks`, think twice whether the `ticklock` is already being held or not. A process who tries to acquire the lock that it has already acquired will cause a kernel panic.

For example, suppose you want to access `ticks` in `sleep()`. Instead of

```C
sleep(void *chan, struct spinlock *lk)
{
  // ...
  uint now_ticks;
  acquire(&tickslock);
  now_ticks = ticks;
  release(&tickslock);
  // ...
}
```

Make sure you don't double-acquire `ticklock`:

```C
sleep(void *chan, struct spinlock *lk)
{
  // ...
  if (lk == &tickslock) { // already hold
    now_ticks = ticks;
  } else {
    acquire(&tickslock);
    now_ticks = ticks;
    release(&tickslock);
  }
  // ...
    
  // same for release...
}
```

Keep this in mind. It will save you from a huge amount of kernel panic...

# COMP SCI 537 Discussion Week 3

The plan for this week:
- Way to pass struct from user program to syscall - https://github.com/himanshusagar/xv6/commit/c38a44f061b0f8b721219355b8f5a3e2281ef380
- Recap Syscalls 
- Scheduler Logic in xv6
- Shell Overview

## Recap Sycalls 


## How xv6 starts

All the C program starts with `main`, including an operating system:

In `main.c`:

```C
// Bootstrap processor starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // phys page allocator
  kvmalloc();      // kernel page table
  mpinit();        // detect other processors
  lapicinit();     // interrupt controller
  seginit();       // segment descriptors
  picinit();       // disable pic
  ioapicinit();    // another interrupt controller
  consoleinit();   // console hardware
  uartinit();      // serial port
  pinit();         // process table
  tvinit();        // trap vectors
  binit();         // buffer cache
  fileinit();      // file table
  ideinit();       // disk 
  startothers();   // start other processors
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // must come after startothers()
  userinit();      // first user process
  mpmain();        // finish this processor's setup
}
```

It basically does some initialization work, including setting up data structures for page table, trap, and files, detecting other CPUs, creating the first process (init), etc. 

But what happens before `main()` gets called? Checkout `bootasm.S` and `bootmain.c` if interested.

Eventually, `main()` calls `mpmain()`, which then calls `scheduler()`:

```C
// Common CPU setup code.
static void
mpmain(void)
{
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // load idt register
  xchg(&(mycpu()->started), 1); // tell startothers() we're up
  scheduler();     // start running processes
}
```

The `scheduler()` is a forever-loop that keeps picking the next process to run (defined in `proc.c`):

```C
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}
```

<!-- `Note`: Your P3 will mainly revolve around scheduler code.  -->
## Scheduler Logic in xv6

The most interesting piece is what inside the while-loop:

```C
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
```

What it does is scanning through the ptable and find a `RUNNABLE` process `p`, then

1. set current CPU's running process to `p`: `c->proc = p`
2. switch userspace page table to `p`: `switchuvm(p)`
3. set `p` to `RUNNING`: `p->state = RUNNING`
4. `swtch(&(c->scheduler), p->context)`: This is the most tricky and magic piece. What it does is save the current registers into `c->scheduler`, and load the saved registers from `p->context`. After executing this line, the CPU is actually starting to execute `p`'s code (strictly speaking, it's in `p`'s kernel space, not jumping back to `p`'s userspace yet), and the next line `switchkvm()` will not be executed until later this process traps back to kernel again.

Both `c->scheduler` and `p->context` are of type `struct context`, while `c` is of type `struct cpu`.

What is the "context"?  
Let's consider a single CPU core. On that core, we have the notion of the current **execution context**, which basically means the current values of the following registers:

```C
// proc.h
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;     // backup of stack pointer
  uint eip;     // instruction pointer
};
```

Each CPU core has its current context. The context defines where in code this CPU core is currently running.

To jump between different processes, we "switch" the context - this is called a process **context switch**.
- We save the current running process P1's context somewhere - **To where?**  Into P1's PCB.
- We set the CPU context to be the context we previously saved for the to-be-scheduled process P2 - **From where?**  From P2's PCB.

The `cpu` struct details are below:

```C
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
};

// Per-process state
struct proc {
  // ...
  int pid;                     // Process ID
  //...
  struct context *context;     // swtch() here to run process
  // ...
};
```

**What is the first user process that gets scheduled on the CPU?**  The `init` process. The `init` then forks a child `sh`, which runs the xv6 shell program. The `init` then waits on `sh`, and the `sh` process will at some timepoint be scheduled -- this is when you see that active xv6 shell prompting you for some input!

[QUIZ] Before we finishing this section, consider this question: what is the current xv6's scheduling policy?

## `sched()`: From User Process to Scheduler

As you see in the scheduler, when a scheduler decision is made, `swtch(&(c->scheduler), p->context)` will switch to that user process.  
**But when will a user process swtch back to the scheduler?**  Basically, whenever the process calls `sched()`. 

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
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}
```

Skip some uninteresting sanity checking, the major work is really just one single line: `swtch(&p->context, mycpu()->scheduler)`.

There are three cases where a process could come into kernel mode and call `sched()`:

- The process *exits*
- The process goes to *block* voluntarily, examples:
  - It calls the sleep syscall
  - It calls the wait syscall
  - It tries to read on a pipe
- The process "*yield*"s - typically at a timer interrupt
  - At every ~10ms, the timer issues a hardware interrupt
  - This forces the CPU to trap into the kernel, see `trap()` in `trap.c`, the `IRQ_TIMER` case
  - xv6 then increments a global counter named `ticks`, then `yield()` the current running process

```C
// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  // ... (some cleanup)
  sched();
  panic("zombie exit");
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  //... (prepare to sleep)
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();
  //... (after wakeup)
}
```

It's unsurprising to see `sched()` gets called in these three cases... `exit()` is expected. `sleep()` here is a bad name... It actually means "blocked" or "wait". Any process that is waiting for some events (e.g. IO) will call this. When handling compensation ticks, we will play more with `sleep()` (in next week's discussion). `yield()` here is another bad name... xv6 doesn't have a syscall named `yield()`. You should search in the codebase again to see who calls `yield()`.

## Timer Interrupt

Scheduling will be less useful without considering timer interrupt. As you have seen in P1B, all the interrupts are handled in `trap.c: trap()`.

```C
struct spinlock tickslock;
uint ticks;

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
  // something you have already seen in P1B
  }

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
  // other cases...
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
```

There are two interesting things going on here:

1. If it is a timer interrupt `case T_IRQ0 + IRQ_TIMER`: the global variable `ticks` gets incremented
2. If it is a timer interrupt satisfying `myproc() && myproc()->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER`: call `yield()`, which then relinquish CPU and gets into scheduler


## Shell Overview:
Shell is a program that takes input as "commands" line by line, parses them, then
- For most of the commands, the shell will create a new process and load it with an executable file to run. E.g. `gcc`, `ls`
- For some commands, the shell will react to them directly without creating a new process. These are called "built-in" commands. E.g. `alias`

If you are curious about which catalog a command falls into, try `which cmd_name`:

```console
# The output of the following commands may vary depending on what shell you are running
# Below is from zsh. If you are using bash, it may look different
$ which gcc
/usr/bin/gcc
$ which alias
alias: shell built-in command
$ which which
which: shell built-in command
```
### `fork`: So Is the Process Created

The syscall `fork()` creates a copy of the current process. We call the original process "parent" and the newly created process "child". But how are we going to tell which is which if they are the same??? The child process will get the return value 0 from `fork` while the parent will get the child's pid as the return value.

```C
pid_t pid = fork();
if (pid == 0) { // the child process will execute this
    printf("I am child with pid %d. I got return value from fork: %d\n", getpid(), pid);
    exit(0); // we exit here so the child process will not keep running
} else { // the parent process will execute this
    printf("I am parent with pid %d. I got return value from fork: %d\n", getpid(), pid);
}
```

You could find the code above in the repo as `fork_example.c`. After executing it, you will see output like this:

```console
$ ./fork_example
I am parent with pid 46565. I got return value from fork: 46566
I am child with pid 46566. I got return value from fork: 0
```
### `exec`: 

`fork` itself is not sufficient to run an operating system. It can only create a copy of the previous program, but we don't want the exact same program all the time. That's when `exec` shines. `exec` is actually a family of functions, including `execve`, `execl`, `execle`, `execlp`, `execv`, `execvp`, `execvP`... The key one is `execve` (which is the actual syscall) and the rest of them are just some wrappers in the glibc that does some work and then calls `execve`. For this project, `execv` is probably what you need. It is slightly less powerful than `execve`, but is enough for this project.

What `exec` does is: it accepts a path/filename and finds this file. If it is an executable file, it then destroys the current address space (including code, stack, heap, etc), loads in the new code, and starts executing the new code.

```C
int execv(const char *path, char *const argv[])
```

Here is how `execv` works: it takes a first argument `path` to specify where the executable file locates, and then provides the command-line arguments `argv`, which will eventually become what the target program receives in their main function `int main(int argc, char* argv[])`.

```C
pid_t pid = fork();
if (pid == 0) {
    // the child process will execute this
    char *const argv[3] = {
        "/bin/ls", // string literial is of type "const char*"
        "-l",
        NULL // it must have a NULL in the end of argv
    };
    int ret = execv(argv[0], argv);
    // if succeeds, execve should never return
    // if it returns, it must fail (e.g. cannot find the executable file)
    printf("Fails to execute %s\n", argv[0]);
    exit(1); 
}
// do parent's work
```

You could find the code above in the repo as `exec_example.c`. After executing it, you will see output exactly like executing `ls -l`.

As the last word of this section, I strongly recommend you to read the [document](https://linux.die.net/man/3/execv) yourself to understand how it works.

### `waitpid`: Wait for child to finish

Now you know the shell is the parent of all the processes it creates. The next question is, when executing a command, the shell should suspend and not asking for new inputs until the child process finishes executing.

```console
$ sleep 10 # this will create a new process executing /usr/bin/sleep
```

The command above will create a child process that does nothing other than sleeping for 10 seconds. During this period, you may notice your shell is also not printing a new prompt. This is what the shell does: it waits until the child process to terminate (no matter voluntarily or not). If you use `ctrl-c` to terminate `sleep`, you should see the shell would print the prompt immediately.

This is also be done by a syscall `waitpid`:

```C
pid_t waitpid(pid_t pid, int *status, int options);
```

This syscall will suspend the parent process and resume again when the child is done. It takes three arguments: `pid`, a pointer to an int `status`, and some flags `options`. `pid` is the pid of the child that the parent wants to waiit. `status` is a pointer pointing a piece of memory that `waitpid` could write the status to. `options` allows you to configure when `waitpid` should return. By default (`options = 0`), it only returns when the child terminates. This should be sufficient for this project.

Side note: NEVER write code like this:

```C
// assume we know pid
int* status;
waitpid(pid, status, 0); // QUIZ: what is wrong?
```

`*status` will be filled in the status that `waitpid` returns. This allows you to have more info about what happens to the child. You could use some macro defined in the library to check it, e.g. `WIFEXITED(status)` is true if the child terminated normally (i.e. not killed by signals); `WEXITSTATUS(status)` could tell you what is the exit code from the child process (e.g. if the child `return 0` from the main function, `WEXITSTATUS(status)` will give you zero).

Again, you should read the [document](https://linux.die.net/man/2/waitpid) yourself carefully.

Putting what we have discussed together, you should now have some idea of the skeleton of the shell

```C
pid_t pid = fork(); // create a new child process
if (pid == 0) { // this is child
    // do some preparation
    execv(path, argv);
    exit(1); // this means execv() fails
}
// parent
int status;
waitpid(pid, &status, 0); // wait the child to finish its work before keep going
// continue to handle the next command
```

Resources:

- Xv6 [Book](https://pdos.csail.mit.edu/6.828/2014/xv6/book-rev8.pdf)
- Man Pages - `exec` & `fork`

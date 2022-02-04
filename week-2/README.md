# COMP SCI 537 Discussion Week 2

## Topics:
- xv6 and GDB
- Syscalls Continued..

## xv6 and GDB
To run xv6 with gdb: in one window

```bash
$ make qemu-nox-gdb
```

then in another window:

```bash
$ gdb kernel
```
You might get the error message blow:

```bash
$ gdb
GNU gdb (Ubuntu 9.2-0ubuntu1~20.04) 9.2
Copyright (C) 2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<http://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.

For help, type "help".
Type "apropos word" to search for commands related to "word".
warning: File "/dir/xv6/.gdbinit" auto-loading has been declined by your `auto-load safe-path' set to "$debugdir:$datadir/auto-load".
To enable execution of this file add
        add-auto-load-safe-path /dir/xv6/.gdbinit
line to your configuration file "/u/c/h/chenhaoy/.gdbinit".
To completely disable this security protection add
        set auto-load safe-path /
line to your configuration file "/u/c/h/chenhaoy/.gdbinit".
For more information about this security protection see the
--Type <RET> for more, q to quit, c to continue without paging--
```

Recent gdb versions will not automatically load `.gdbinit` for security purposes. You could either:

- `echo "add-auto-load-safe-path $(pwd)/.gdbinit" >> ~/.gdbinit`. This enables the autoloading of `.gdbinit` in the current working directory.
- `echo "set auto-load safe-path /" >> ~/.gdbinit`. This enables the autoloading of every `.gdbinit`

After either operation, you should be able to launch gdb. Specify you want to attach to the kernel

```bash
$ gdb kernel
GNU gdb (Ubuntu 9.2-0ubuntu1~20.04) 9.2
Copyright (C) 2020 Free Software Foundation, Inc.
...
Type "apropos word" to search for commands related to "word"...
Reading symbols from kernel...
+ target remote localhost:29475
The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:	ljmp   $0x3630,$0xf000e05b
0x0000fff0 in ?? ()
+ symbol-file kernel
```

 Once GDB has connected successfully to QEMU's remote debugging stub, it retrieves and displays information about where the remote program has stopped: 

 ```bash
 The target architecture is assumed to be i8086
[f000:fff0]    0xffff0:	ljmp   $0x3630,$0xf000e05b
0x0000fff0 in ?? ()
+ symbol-file kernel
 ```
 QEMU's remote debugging stub stops the virtual machine before it executes the first instruction: i.e., at the very first instruction a real x86 PC would start executing after a power on or reset, even before any BIOS code has started executing.

 Type the following in GDB's window:

 ```bash
 (gdb) b exec
Breakpoint 1 at 0x80100a80: file exec.c, line 12.
(gdb) c
Continuing.
 ```
These commands set a breakpoint at the entrypoint to the exec function in the xv6 kernel, and then continue the virtual machine's execution until it hits that breakpoint. You should now see QEMU's BIOS go through its startup process, after which GDB will stop again with output like this:

```bash
The target architecture is assumed to be i386
0x100800 :	push   %ebp

Breakpoint 1, exec (path=0x20b01c "/init", argv=0x20cf14) at exec.c:11
11	{
(gdb) 
```

At this point, the machine is running in 32-bit mode, the xv6 kernel has initialized itself, and it is just about to load and execute its first user-mode process, the /init program. You will learn more about exec and the init program later; for now, just continue execution: 

```bash
(gdb) c
Continuing.
0x100800 :	push   %ebp

Breakpoint 1, exec (path=0x2056c8 "sh", argv=0x207f14) at exec.c:11
11	{
(gdb) 
```
The second time the exec function gets called is when the /init program launches the first interactive shell, sh.

 Now if you continue again, you should see GDB appear to "hang": this is because xv6 is waiting for a command (you should see a '$' prompt in the virtual machine's display), and it won't hit the exec function again until you enter a command and the shell tries to run it. Do so by typing something like:

```bash
$ cat README
```
You should now see in the GDB window:

```bash
0x100800 :	push   %ebp

Breakpoint 1, exec (path=0x1f40e0 "cat", argv=0x201f14) at exec.c:11
11	{
(gdb) 
```

GDB has now trapped the exec system call the shell invoked to execute the requested command.

Now let's inspect the state of the kernel a bit at the point of this exec command. 

```bash
(gdb) p argv[0]
(gdb) p argv[1]
(gdb) p argv[2]
```

Let's continue to end the cat command.
```bash
(gdb) c
Continuing.
```


## Syscalls Recap 
We will explore main files required to do p1b. 

- `Makefile`:
    -  `CPUS`: # CPUS for QEMU to virtualize (make it 1)
    -  `UPROGS`: what user-space program to build with the kernel. Note that xv6 is a very simple kernel that doesn't have a compiler, so every executable binary replies on the host Linux machine to build it along with the kernel.
    - Exercise: add a new user-space application called `hello` which prints `"hello world\n"` to the stdout.

- `usys.S`: where syscalls interfaces are defined in the assembly. This is the entry point to syscalls. 
    - Code for `usys.S` 
        ```assembly
        #include "syscall.h"
        #include "traps.h"

        #define SYSCALL(name) \
        .globl name; \
        name: \
            movl $SYS_ ## name, %eax; \
            int $T_SYSCALL; \
            ret

        SYSCALL(fork)
        SYSCALL(exit)
        SYSCALL(wait)
        # ...
        ```
    - It first declares a macro `SYSCALL`, which takes an argument `name` and expands it.

        ```assembly
        #define SYSCALL(name) \
        .globl name; \
        name: \
            movl $SYS_ ## name, %eax; \
            int $T_SYSCALL; \
            ret
        ```    
    - For e.g. let `name` be `getpid`, the macro above will be expanded into

        ```assembly
        .globl getpid
        getpid:
            movl $SYS_getpid, %eax
            int $T_SYSCALL
            ret
        ```
    - Where is `SYS_getpid` and `T_SYSCALL` located?
    - so it will be further expanded into
        ```assembly
        .globl getpid
        getpid:
            movl $11, %eax
            int $64
            ret
        ```
    - Run `gcc -S usys.S` to see all syscalls. 
        ```console
        $ gcc -S usys.S
        // ...
        .globl dup; dup: movl $10, %eax; int $64; ret
        .globl getpid; getpid: movl $11, %eax; int $64; ret
        .globl sbrk; sbrk: movl $12, %eax; int $64; ret
        // ...
        ```
    - `.globl getpid` declares a label `getpid` for compiler/linker; when someone in the user-space executes `call getpid`, the linker knows which line it should jump to. It then moves 11 into register `%eax` and issues an interrupt with an operand 64.
- `trapasm.S`: The user-space just issues an interrupt with an operand `64`. The CPU then starts to run the kernel's interrupt handler.
    - It builds what we call a trap frame, which basically saves some registers into the stack and makes them into a data structure `trapframe`. 

    - `trapframe` is defined in `x86.h`:

    ```C
    struct trapframe {
    // registers as pushed by pusha
    uint edi;
    uint esi;
    uint ebp;
    uint oesp;      // useless & ignored
    uint ebx;
    uint edx;
    uint ecx;
    uint eax;

    // rest of trap frame
    ushort gs;
    ushort padding1;
    ushort fs;
    ushort padding2;
    ushort es;
    ushort padding3;
    ushort ds;
    ushort padding4;
    uint trapno;

    // below here defined by x86 hardware
    uint err;
    uint eip;
    ushort cs;
    ushort padding5;
    uint eflags;

    // below here only when crossing rings, such as from user to kernel
    uint esp;
    ushort ss;
    ushort padding6;
    };
    ```
    - `alltraps` eventually executes `call trap`. The function `trap()` is defined in `trap.c`:


- `trap.c`: recall a syscall is implemented as an interrupt (sometimes also called "trap")
    - Trap code:
        ```C
        void
        trap(struct trapframe *tf)
        {
        if(tf->trapno == T_SYSCALL){
            if(myproc()->killed)
            exit();
            myproc()->tf = tf;
            syscall();
            if(myproc()->killed)
            exit();
            return;
        }
        // ...
        }
        ```
    - The 64 we just saw is `tf->trapno` here. `trap()` checks whether `trapno` is `T_SYSCALL`, if yes, it calls `syscall()` to handle it.

- `syscall.c` and `syscall.h`:

    - `void syscall(void)`: 
        ```C
        void
        syscall(void)
        {
        int num;
        struct proc *curproc = myproc();

        num = curproc->tf->eax;
        if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
            curproc->tf->eax = syscalls[num]();
        } else {
            cprintf("%d %s: unknown sys call %d\n",
                    curproc->pid, curproc->name, num);
            curproc->tf->eax = -1;
        }
        }
        ```
    - `Note:` See how to identify which syscall to call and where is the return value for completed syscalls. This is the heart of P1-B.


- `sysproc.c`:

    -  Kill syscall: Kill takes `PID` of the process to kill as an arguement, but this function has no arguement. How does this happen? 

        ```C
        int
        sys_kill(void)
        {
        int pid;

        if(argint(0, &pid) < 0)
            return -1;
        return kill(pid);
        }
        ```
        `Hint`: See `argint` is defined in `syscall.c`

- `proc.c` and `proc.h`:

    - `struct proc`: per-process state
    - `struct context`: saved registers
    - `ptable.proc`: a pool of `struct proc`
    - `static struct proc* allocproc(void)`: where a `struct proc` gets allocated from `ptable.proc` and **initialized**

- `user.h` and `defs.h`: what the difference compared to `user.h`? 

    `Hint:` [link](https://www.cs.virginia.edu/~cr4bd/4414/S2020/xv6.html)


## Exercise

Create a hello-world syscall and call it from a new user application. 
Hint: Check `kill`

## Resources:

- Xv6 [Book](https://pdos.csail.mit.edu/6.828/2014/xv6/book-rev8.pdf)
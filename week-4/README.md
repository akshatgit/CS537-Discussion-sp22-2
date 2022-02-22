# COMP SCI 537 Discussion Week 4

## Topics:
- Lottery Scheduler
- Shell Continued 
    - File Descriptor for redirection
    - Linked list review
    - Important C libraries


## Lottery Scheduler
Processes are assigned a subset of the tickets. Each process is assigned a probability, and scheduler picks a random number to schedule a process from the pool of READY processes.

For e.g. We have 5 processes:
- Process A (p=0.1): 0-9 tickets
- Process B (p=0.2): 10-29 tickets
- Process C (p=0.4): 30-69 tickets
- Process D (p=0.1): 70-79 tickets
- Process E (p=0.2): 80-99 tickets

Total tickets: 100

Now consider the following scheduler scenario:


| Time      | Ticket Number | Process scheduled |
| ----------- | ----------- |-----|
| t = 1      | 11       | B|
| t = 2      | 32       | C|
| t = 3      | 45       | C|
| t = 4      | 52       | C|
| t = 5      | 83       | E|
| t = 6      | 78       | D|
| t = 7      | 88       | E|
| t = 8      | 66       | C|
| t = 9      | 5       | A|

All the processes are alloted time in proportion to the tickets they were alloted. This allotment may not be exactly proportional for all instances of lottery scheduler, but approximately it will show the proportion of processes' tickets alloted.

## Shell 
### File Descriptor for redirection

If you have ever printed out a file descriptor, you may notice it is just an integer

```C
int fd = open("filename", O_RDONLY);
printf("fd: %d\n", fd);
// you may get output "3\n"
```

<!-- However, the file-related operation is stateful: e.g. it must keep a record what is the current read/write offset, etc. How does an integer encoding this information?

It turns out there is a level of indirection here: the kernel maintains such states and the file descriptor it returns is essentially a "handler" for later index back to these states. The kernel provides the guarantee that "the user provides the file descriptor and the operations to perform, the kernel will look up the file states that corresponding to the file descriptor and operate on it". -->

It would be helpful to understand what happens with some actual code from a kernel. For simplicity, we use ths xv6 code below to illustrate how file descriptor works, but remember, your p2b is on Linux. Linux has a very similar implementation.

For every process state (`struct proc` in xv6), it has an array of `struct file` (see the field  `proc.ofile`) to keep a record of the files this process has opened.

```C
// In proc.h
struct proc {
  // ...
  int pid;
  // ...
  struct file *ofile[NOFILE];  // Open files
};

// In file.h
struct file {
  // ...
  char readable;    // these two variables are actually boolean, but C doesn't have `bool` type,
  char writable;    // so the authors use `char`
  // ...
  struct inode *ip; // this is a pointer to another data structure called `inode`
  uint off;         // offset
};
```

The file descriptor is essentially an index for `proc.ofile`. In the example above, when opening a file named "filename", the kernel allocates a `struct file` for all the related state of this file. Then it stores the address of this `struct file` into `proc.ofile[3]`. In the future, when providing the file descript `3`, the kernel could get `struct file` by using `3` to index `proc.ofile`. This also gives you a reason why you should `close()` a file after done with it: the kernel will not free `struct file` until you `close()` it; also `proc.ofile` is a fixed-size array, so it has limit on how many files a process can open at max (`NOFILE`).

In addition, file descriptors `0`, `1`, `2` are reserved for stdin, stdout, and stderr.

### File Descriptors after `fork()`

During `fork()`, the new child process will copy `proc.ofile` (i.e. copying the pointers to `struct file`), but not `struct file` themselves. In other words, after `fork()`, both parent and child will share `struct file`. If the parent changes the offset, the change will also be visible to the child.

```
struct proc: parent {
    +---+
    | 0 | ------------+---------> [struct file: stdin]
    +---+             |
    | 1 | --------------+-------> [struct file: stdout]
    +---+             | |
    | 2 | ----------------+-----> [struct file: stderr]
    +---+             | | |
    ...               | | |
}                     | | |
                      | | |
struct proc: child {  | | |
    +---+             | | |
    | 0 | ------------+ | |
    +---+               | |
    | 1 | --------------+ |
    +---+                 |
    | 2 | ----------------+
    +---+
    ...
}
```
`High-level Ideas of Redirection: What to Do`

When a process writes to stdout, what it actually does it is writing data to the file that is associated with the file descriptor `1`. So the trick of redirection is, we could replace the `struct file` pointer at `proc.ofile[1]` with another `struct file`.

For example, when handling the shell command `ls > log.txt`, what the file descriptors eventually should look like:

```
struct proc: parent {                                      <= `mysh` process
    +---+
    | 0 | ------------+---------> [struct file: stdin]
    +---+             |
    | 1 | ----------------------> [struct file: stdout]
    +---+             | 
    | 2 | ----------------+-----> [struct file: stderr]
    +---+             |   |
    ...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                                <= `ls` process
    +---+             |   |
    | 0 | ------------+   |
    +---+                 |
    | 1 | ----------------|-----> [struct file: log.txt]   <= this is a new stdout!
    +---+                 |
    | 2 | ----------------+
    +---+
    ...
}
```

### `dup2()`: How to Do

The trick to implement the redirection is the syscall `dup2`. This syscall performs the task of "duplicating a file descriptor".

```C
int dup2(int oldfd, int newfd);
```

`dup2` takes two file descriptors as the arguments. It performs these tasks (with some pseudo-code):

1. if the file descriptor `newfd` has some associated files, close it. (`close(newfd)`)
2. copy the file associated with `oldfd` to `newfd`. (`proc.ofile[newfd] = proc.ofile[oldfd]`)

Consider the provious example with `dup2`:

```C
int pid = fork();
if (pid == 0) { // child;
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); // [1]
    dup2(fd, fileno(stdout));                                     // [2]
    // execv(...)
}
```

Here `fileno(stdout)` will give the file descriptor associated with the current stdout. After executing `[1]`, you should have:

```
struct proc: parent {                                     <= `mysh` process
    +---+
    | 0 | ------------+---------> [struct file: stdin]
    +---+             |
    | 1 | --------------+-------> [struct file: stdout]
    +---+             | |
    | 2 | ----------------+-----> [struct file: stderr]
    +---+             | | |
    ...               | | |
}                     | | |
                      | | |
struct proc: child {  | | |                               <= child process (before execv "ls")
    +---+             | | |
    | 0 | ------------+ | |
    +---+               | |
    | 1 | --------------+ |
    +---+                 |
    | 2 | ----------------+
    +---+
    | 3 | ----------------------> [struct file: log.txt]  <= open a file "log.txt"
    +---+
    ...
}
```

After executing `[2]`, you should have

```
struct proc: parent {                                     <= `mysh` process
    +---+
    | 0 | ------------+---------> [struct file: stdin]
    +---+             |
    | 1 | ----------------------> [struct file: stdout]
    +---+             | 
    | 2 | ----------------+-----> [struct file: stderr]
    +---+             |   |
    ...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                               <= child process (before execv "ls")
    +---+             |   |
    | 0 | ------------+   |
    +---+                 |
    | 1 | --------------+ |
    +---+               | |
    | 2 | ----------------+
    +---+               |
    | 3 | --------------+-------> [struct file: log.txt]
    +---+
    ...
}
```

Also, compared to the figure of what we want, it has a redudent file descriptor `3`. We should close it. The modified code should look like this:

```C
int pid = fork();
if (pid == 0) { // child;
    int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); // [1]
    dup2(fd, fileno(stdout));                                     // [2]
    close(fd);                                                    // [3]
    // execv(...)
}
```

After we finishing `dup2`, the previous `fd` is no longer useful. We close it at `[3]`. Then the file descriptors should look like this:

```
struct proc: parent {                                     <= `mysh` process
    +---+
    | 0 | ------------+---------> [struct file: stdin]
    +---+             |
    | 1 | ----------------------> [struct file: stdout]
    +---+             | 
    | 2 | ----------------+-----> [struct file: stderr]
    +---+             |   |
    ...               |   |
}                     |   |
                      |   |
struct proc: child {  |   |                               <= child process (before execv "ls")
    +---+             |   |
    | 0 | ------------+   |
    +---+                 |
    | 1 | --------------+ |
    +---+               | |
    | 2 | ----------------+
    +---+               |
    | X |               +-------> [struct file: log.txt]
    +---+
    ...
}
```

Despite the terrible drawing, we now have what we want! You can imagine doing it in a similar way for stdin redirection.

Again, you should read the [document](https://man7.org/linux/man-pages/man2/dup2.2.html) yourself to understand how to use it.

`Note`, piping is actually also implemented in a similar way.

### Linked List Review for Alias

### Some Imp C Functions
- strtok
- strdup

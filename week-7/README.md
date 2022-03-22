# COMP SCI 537 Discussion Week 7

The plan for this week:
- Overview of project 3A and 3B
- How to start 

## Project 3A Overview - Map-reduce
- Refer to [last](https://medium.com/edureka/mapreduce-tutorial-3d9535ddbe7c) disucssion example on intro to MR


```c
#ifndef __mapreduce_h__
#define __mapreduce_h__

// Different function pointer types used by MR
typedef char *(*Getter)(char *key, int partition_number);
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, Getter get_func, int partition_number);
typedef unsigned long (*Partitioner)(char *key, int num_partitions);

// External functions: these are what you must define
void MR_Emit(char *key, char *value);

unsigned long MR_DefaultHashPartition(char *key, int num_partitions);

void MR_Run(int argc, char *argv[], 
	    Mapper map, int num_mappers, 
	    Reducer reduce, int num_reducers, 
	    Partitioner partition);

#endif // __mapreduce_h__
```

You will create `num_mappers` threads for map tasks, and `num_reducers` threads for reduce tasks, thereby increasing the efficiency of the sequential implementation provided with the project. 

## Project 3B Overview - xv6 kernel threads 
We will create 2 new syscalls: 
- `clone()`
- `join()`

Using the above two system calls, we will make a threading library, and also build a lock primitive (`ticket` lock). Finally build a user app to demo the system. 
### New system calls:
New System calls:
- `int clone(void(*fcn)(void *, void *), void *arg1, void *arg2, void *stack)`: The threading library(`thread_create`) will use `malloc` to allocate a new `stack` and then use this system call to create/start running the thread. 
- `int join(void **stack)`: This will be used by `thread_join`, where parent will wait for child thread to finish. 

## How to start
### Project 3A - Map-reduce

`wordcount` walkthrough example:
```c
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mapreduce.h"

void Map(char *file_name) {
    FILE *fp = fopen(file_name, "r");
    assert(fp != NULL);

    char *line = NULL;
    size_t size = 0;
    while (getline(&line, &size, fp) != -1) {
        char *token, *dummy = line;
        while ((token = strsep(&dummy, " \t\n\r")) != NULL) {
            MR_Emit(token, "1");
        }
    }
    free(line);
    fclose(fp);
}

void Reduce(char *key, Getter get_next, int partition_number) {
    int count = 0;
    char *value;
    while ((value = get_next(key, partition_number)) != NULL)
        count++;
    printf("%s %d\n", key, count);
}

int main(int argc, char *argv[]) {
    MR_Run(argc, argv, Map, 10, Reduce, 10, MR_DefaultHashPartition);
}
```
Couple of important points:
- We are running 10 mapper  and 10 reducer tasks. 
- Each mapper task is reading a seperate file, but emiting(`MR_Emit`) token to the same `hashmap`. Thus, designing data structre for this step is one of the main challenge in this project.
- Reducers will run only when all mapper tasks are finished. 

### Project 3B - Review of x86 architecture

Some registers used in x86 architecture: 
- EAX, EBX, ECX, EDX, ESI, and EDI are general purpose registers used to store variables
during computations.
- The general purpose registers EBP(base register) and ESP(stack pointer) store the base and top of the current stack
frame.
- The program counter (PC) is also referred to as EIP (instruction pointer).
- The segment registers CS, DS, ES, FS, GS, and SS store pointers to various segments
(e.g., code segment, data segment, stack segment) of the process memory.
- The control registers like CR0 hold control information. For example, the CR3 register
holds the address of the page table of the current running process, which is used to translate
virtual addresses to physical addresses.

Now, assume you have one function `foo()` that calls another function `bar()`:

```c
int bar(int arg1, int arg2, int arg3) {
  return arg1;
}int foo() {
  int local_var = bar(1, 2, 3);
  local_var += 5;
  return local_var;
}
```

- The first thing foo() does (and the first thing every function does) is push the base pointer onto the stack, then save the stack pointer into the base pointer. The base pointer points to the beginning of the stack frame. Just after it are the local variables of the current function. Just before is the return address. (Check `callee-save` and `caller-save` registers.)

- foo() grows the stack to make room for local variables. (How is this done??)

- foo() saves arguments to bar() in registers. (If the arguments were too big to fit into register, it would have pushed them onto the stack instead.)

- foo() calls bar(). This pushes the return address onto the stack and jumps to the first instruction of bar().

- Just like foo(), the first thing bar() does is push the base pointer onto the stack and save the stack pointer into the base pointer. (This chain of pointers is also how stack traces work).

- bar() moves its arguments from registers onto the stack.

- bar() saves the return value into the eax register.

- The leave instruction copies the base pointer into the stack pointer, then pops the saved base pointer back into ebp.

- The ret instruction pops the return address off the stack and jumps to it.

- Back in foo(), the return value moves from the eax register into the area on the stack reserved for local variables. foo() finishes out its last instructions, then follows the same procedures as bar() to return to its own caller.


# Refrence:
- xv6 registers [lecture](https://www.cse.iitb.ac.in/~mythili/teaching/cs347_autumn2016/notes/03-xv6-process.pdf) by IIT Bombay
- x86 function call [blog1](https://medium.com/@connorstack/a-guide-to-x86-calling-convention-824a3236ed65), [blog2](https://textbook.cs161.org/memory-safety/x86.html)
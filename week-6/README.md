# COMP SCI 537 Discussion Week 6

The plan for this week:
- [Recap](https://github.com/akshatgit/CS537-Discussion-sp22-2/tree/main/week-5): Scheduling.
- Backslash zero - `\0`.
- `libpthread`: thread library in C
- Simple Pthreads doing useful work


## Backslash zero - `\0`.

Backslash zero may seem innocents but since memory management is taken care of by programmar in C, things can get a bit hectic.

```C
// Demo of BackSlash 0 pitfalls.
int init_size = 10;
char *ptr = malloc( sizeof( char ) * init_size);
//Assume G is garbage because actual garbage in not printable on terminal.
memset(ptr , 'G' , init_size);
memset(ptr , 'A' , 7);
ptr[4] = '\0';
```
Now, because of this backslash 0 at index 4, all strings related functions will have trouble in operating on ptr because they expect `'\0'` to be properly inserted.
```C
printf("Length of string %s is %ld ..... but ptr[%d] = %c \n" , ptr ,  strlen(ptr) , 5 , ptr[5]);
printf("Similarly ptr[%d] = %c \n" , 6 , ptr[6]);
printf("But ptr[%d] = %c ? \n" , 7 , ptr[7]);
```

Although, pointer has access to memory beyond `\0` but anything after '\0' is ignored by string manipulation functions. For example as per man page of **strcat**,

_The strcat() function appends the src string to the dest string,
overwriting the terminating null byte (`\0`) at the end of dest,
and then adds a terminating null byte._

```C
char append[] = "BBBB";
strcat(ptr , append);
printf("After append string %s's length is %ld \n" , ptr ,  strlen(ptr) );
printf("So, ptr[%d] is still %c \n" , 9 ,  ptr[9]);
```

### Demo of backslash zero - `\0`.

Step 1 : Compile programm for backslash zero 

`make backslash`

Step 2 : Run it. Press enter key one by one to slowly browse though code while it spits out useful pointer related information.

`./backslash`

## `libpthread`: thread library in C`

POSIX.1 specifies a set of interfaces (functions, header files)
for threaded programming commonly known as POSIX threads, or
Pthreads.  A single process can contain multiple threads, all of
which are executing the same program.  These threads share the
same global memory (data and heap segments), but each thread has
its own stack.


### Create a new thread.

The pthread_create() function starts a new thread in the calling
process.  The new thread starts execution by invoking
start_routine(); arg is passed as the sole argument of
start_routine().

```C
int pthread_create(pthread_t *restrict thread,
    const pthread_attr_t *restrict attr,
    void *(*start_routine)(void *),
    void *restrict arg);
```

Example Code:
This is how one might call pthread_create function but it doesn't do much error handling. does it?
```C
pthread_create(&p1, NULL, NULL, "A")
```
Thus in our code, it will look like this so that macro can do error handling.
 ```C  
  Pthread_create(&p1, NULL, NULL, "A"); 
``` 

Error handling macro
```C 
#define Pthread_create(thread, attr, start_routine, arg) assert(pthread_create(thread, attr, start_routine, arg) == 0);
 ```

What about attributes which we have convinently set as NULL? Mostly, we set them as NULL and let libpthread decide appropriate values for us.
                                                             
Thread attributes:                                           

| Attribute | Example Value |
|-----------|---------------|              
| Detach state     | PTHREAD_CREATE_JOINABLE                 |
| Scope      | PTHREAD_SCOPE_SYSTEM            |
| Inherit scheduler     | PTHREAD_INHERIT_SCHED            |
| Scheduling policy     |SCHED_OTHER                 |
| Scheduling priority    |  0              |
| Guard size   | 4096 bytes                  |
| Stack address     | 88            |
| Stack size     | 0x201000 bytes             |

                 
### Time to try them out.


Step 1 : Compile programm for t0  

`make t0`

Step 2 : Run it.

`./t0`

### One possible output.  

```
main: begin
B
A
main: end
```   

### Another possible output.  
                       
```                     
main: begin             
A                       
B                       
main: end               
```        

Why do these different variations exist? Think about their parallel nature.


## Simple Pthreads doing useful work

Time to count our work using counters....

```C

void *mythread(void *arg) 
{
    char *letter = arg;
    int i; // stack (private per thread) 
    printf("%s: begin [addr of i: %p]\n", letter, &i);
    for (i = 0; i < max; i++) {
    counter = counter + 1; // shared: only one
    }
    printf("%s: done\n", letter);
    return NULL;
}
   
```

Step 1 : Compile programm for t1  

`make t1`

Step 2 : Run it.

`./t1`



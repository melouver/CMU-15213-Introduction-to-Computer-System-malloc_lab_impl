# Malloc lab 
This is  my solution for malloc lab.Using explicit free list , FIFO and best fit policy , we got util=60% throughput=14000Kops/s
with standard throughput 15000.
Optimization:
For the Best fit, when we meet a block with internal fragment rate lower than 0.2, then we think it is A best fit block and just return
it.This avoids linear search time in a linked-list.
这是我的malloc lab代码，使用显式链表管理空闲块，使用FIFO来进行空闲块的插入和删除，在查找空闲块时，使用略微改进的best fit算法。
# Future
使用segregated free list来管理空闲块，使用first fit来进行空闲块search，有论文表明，在segregated free list上进行first fit，其最终的空间利用率接近于
best fit。

#############################
# CS:APP Malloc Lab
# Handout files for students
#############################

***********
Main Files:
***********

mdriver, mdriver-emulate
        Once you've run make, run ./mdriver to test
        your solution.  Run ./mdriver-emulate to make sure your
        solution can handle 64-bit allocations

traces/
	Directory that contains the trace files that the driver uses
	to test your solution. Files with names of the form XXX-short.rep
	contain very short traces that you can use for debugging.

**********************************
Other support files for the driver
**********************************
config.h	Configures the malloc lab driver
clock.{c,h}	Low-level timing functions
fcyc.{c,h}	Function-level timing functions
memlib.{c,h}	Models the heap and sbrk function
stree.{c,h}     Data structure used by the driver to check for
		overlapping allocations
Contech.so	Code that combines with LLVM compiler infrastructure
		to enable sparse memory emulation
macro-check.pl  Code to check for disallowed macro definitions
driver.pl	Runs both mdriver and mdriver-emulate and generates
		the autolab result.  (Not included with checkpoint)
callibrate.pl   Code to generate benchmark throughput
throughputs.txt Benchmark throughputs, indexed by CPU type

***********************
Example malloc packages
***********************
mm.c            Empty malloc package
mm-naive.c      Fast but extremely memory-inefficient package
mm-baseline.c   Implicit-list allocator to use as starting point

*******************************
Building and running the driver
*******************************
To build the driver, type "make" to the shell.

To run the driver on a tiny test trace:

	unix> ./mdriver -V -f traces/malloc.rep

To get a list of the driver flags:

	unix> ./mdriver -h

The -V option prints out helpful tracing information

You can use mdriver-emulate to test the correctness of your code in
handling 64-bit addresses:

	unix> ./mdriver-emulate

You should see the exact same utilization numbers as you did with the
regular driver.  No timing is done, and so the time and throughput
numbers show up as zeros.


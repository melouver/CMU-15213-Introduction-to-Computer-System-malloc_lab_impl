# Solution to the CMU 15213 Introduction to Computer System Malloc lab 
This is  my solution for malloc lab.
Using explicit free list , FIFO and best fit policy , we got util=60% throughput=14000Kops/s with standard throughput 15000.
# Optimization
For the Best fit, when we meet a block with internal fragment rate lower than 0.2, then we think it is A best fit block and just return
it.This avoids linear search time in a linked-list.

This is my code for the malloc lab. It uses an explicit linked list to manage free blocks and employs FIFO (First-In-First-Out) for inserting and deleting free blocks. When searching for a free block, an improved best fit algorithm is used.
# Future
Using segregated free lists to manage free blocks and employing first fit for free block search. There are studies indicating that using first fit on segregated free lists results in a final space utilization rate close to that of using best fit.












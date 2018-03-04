# Malloc lab 
This is  my solution for malloc lab.
Using explicit free list , FIFO and best fit policy , we got util=60% throughput=14000Kops/s with standard throughput 15000.
# Optimization
For the Best fit, when we meet a block with internal fragment rate lower than 0.2, then we think it is A best fit block and just return
it.This avoids linear search time in a linked-list.

这是我的malloc lab代码，使用显式链表管理空闲块，使用FIFO来进行空闲块的插入和删除，在查找空闲块时，使用略微改进的best fit算法。

# Future
使用segregated free list来管理空闲块，使用first fit来进行空闲块search，有论文表明，在segregated free list上进行first fit，其最终的空间利用率接近于使用best fit。












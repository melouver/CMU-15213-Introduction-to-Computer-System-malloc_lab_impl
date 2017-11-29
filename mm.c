/*
******************************************************************************
*                               mm-baseline.c                                *
*           64-bit struct-based implicit free list memory allocator          *
*                  15-213: Introduction to Computer Systems                  *
*                                                                            *
*  ************************************************************************  *
*                               DOCUMENTATION                                *
*                                                                           *
*  ************************************************************************  *
*  ** ADVICE FOR STUDENTS. **                                                *
*  Step 0: Please read the writeup!                                          *
*  Write your heap checker. Write your heap checker. Write. Heap. checker.   *
*  Good luck, and have fun!                                                  *
*                                                                            *
******************************************************************************
*/

/* Do not change the following! */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
#include <stddef.h>

#include "mm.h"
#include "memlib.h"

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 * If DEBUG is defined, enable printing on dbg_printf and contracts.
 * Debugging macros, with names beginning "dbg_" are allowed.
 * You may not define any other macros having arguments.
 */

//#define DEBUG // uncomment this line to enable debugging

#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_dengassert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disnabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Basic constants */
typedef uint64_t word_t;
static const size_t wsize = sizeof(word_t);   // word and header size (bytes)
static const size_t dsize = 2*wsize;          // double word size (bytes)
static const size_t min_block_size = 2*dsize; // Minimum block size
static const size_t chunksize = (1 << 12);    // requires (chunksize % 16 == 0)

static const word_t alloc_mask = 0x1;
static const word_t size_mask = ~(word_t)0xF;
static bool called_by_free = false;

typedef struct block
{
  /* Header contains size + allocation flag */
  word_t header;
  
  /*
   * We can't declare the footer as part of the struct, since its starting
   * position is unknown
   */
  char payload[0];
  struct block *prev;
  struct block *next;
  /*
   * We don't know how big the payload will be.  Declaring it as an
   * array of size 0 allows computing its starting address using
   * pointer notation.
   */
 
} block_t;


/* Global variables */
/* Pointer to first block */
static block_t *heap_listp = NULL;
static block_t *free_list_start = NULL;

/* Function prototypes for internal helper routines */
static block_t *extend_heap(size_t size);
static void place(block_t *block, size_t asize);
static block_t *find_fit(size_t asize);
static block_t *coalesce(block_t *block);

static size_t max(size_t x, size_t y);
static size_t round_up(size_t size, size_t n);
static word_t pack(size_t size, bool alloc);

static size_t extract_size(word_t header);
static size_t get_size(block_t *block);
static size_t get_payload_size(block_t *block);

static bool extract_alloc(word_t header);
static bool get_alloc(block_t *block);

static void write_header(block_t *block, size_t size, bool alloc);
static void write_footer(block_t *block, size_t size, bool alloc);

static block_t *payload_to_header(void *bp);
static void *header_to_payload(block_t *block);

static block_t *find_next(block_t *block);
static word_t *find_prev_footer(block_t *block);
static block_t *find_prev(block_t *block);
static void remove_from_free_list(block_t *block);
static void insert_in_free_list(block_t *block);
static block_t *get_free_next(block_t *block);
static block_t *get_free_prev(block_t *block);
bool mm_checkheap(int lineno);
static void print_heap() ;
/*
 * mm_init: initializes the heap; it is run once when heap_start == NULL.
 *          prior to any extend_heap operation, this is the heap:
 *              start            start+8           start+16
 *          INIT: | PROLOGUE_FOOTER | EPILOGUE_HEADER |
 * heap_listp ends up pointing to the epilogue header.
 */
bool mm_init(void)
{
  // Create the initial empty heap
  word_t *start = (word_t *)(mem_sbrk(6*wsize));

  if (start == (void *)-1)
    {
      return false;
    }
    
  start[0] = pack(0, false); // Prologue footer
  start[1] = pack(2*dsize, true); // Epilogue header
  start[2] = pack(0, false);
  start[3] = pack(0, false);
  start[4] = pack(2*dsize, true);
  start[5] = pack(0, false);

  // Heap starts with first block header (epilogue)
  heap_listp = (block_t *)&(start[1]);
  free_list_start = (block_t *)&(start[1]);
  // Extend the empty heap with a free block of chunksize bytes
  if (extend_heap(4*dsize) == NULL)
    {
      return false;
    }
  return true;
}

/*
 * malloc: allocates a block with size at least (size + dsize), rounded up to
 *         the nearest 16 bytes, with a minimum of 2*dsize. Seeks a
 *         sufficiently-large unallocated block on the heap to be allocated.
 *         If no such block is found, extends heap by the maximum between
 *         chunksize and (size + dsize) rounded up to the nearest 16 bytes,
 *         and then attempts to allocate all, or a part of, that memory.
 *         Returns NULL on failure, otherwise returns a pointer to such block.
 *         The allocated block will not be used for further allocations until
 *         freed.
 */
void *malloc(size_t size)
{
  dbg_requires(mm_checkheap(__LINE__));

  size_t asize;      // Adjusted block size
  size_t extendsize; // Amount to extend heap if no fit is found
  block_t *block;
  void *bp = NULL;

  if (heap_listp == NULL) // Initialize heap if it isn't initialized
    {
      mm_init();
    }

  if (size == 0) // Ignore spurious request
    {
      dbg_ensures(mm_checkheap(__LINE__));
      return bp;
    }

  // Adjust block size to include overhead and to meet alignment requirements
  asize = round_up(size, dsize) + dsize;

  // Search the free list for a fit
  block = find_fit(asize);

  // If no fit is found, request more memory, and then and place the block
  if (block == NULL)
    {
      extendsize = max(asize, chunksize);
      block = extend_heap(extendsize);
      if (block == NULL) // extend_heap returns an error
        {
          return bp;
        }

    }
  
  place(block, asize);
  bp = header_to_payload(block);

  dbg_printf("Malloc size %zd on address %p.\n", size, bp);
  dbg_ensures(mm_checkheap(__LINE__));
  return bp;
}

/*
 * free: Frees the block such that it is no longer allocated while still
 *       maintaining its size. Block will be available for use on malloc.
 */
void free(void *bp)
{
  if (bp == NULL)
    {
      return;
    }

  block_t *block = payload_to_header(bp);
  size_t size = get_size(block);

  write_header(block, size, false);
  write_footer(block, size, false);
  
  //coalesce(block); wrong code! Coalesce is used for block already in free list
  called_by_free = true;
  coalesce(block);
  called_by_free = false;
  dbg_printf("Free size %zd on address %p\n.", size, bp);
  dbg_ensures(mm_checkheap(__LINE__));
}

/*
 * realloc: returns a pointer to an allocated region of at least size bytes:
 *          if ptrv is NULL, then call malloc(size);
 *          if size == 0, then call free(ptr) and returns NULL;
 *          else allocates new region of memory, copies old data to new memory,
 *          and then free old block. Returns old block if realloc fails or
 *          returns new pointer on success.
 */
void *realloc(void *ptr, size_t size)
{
  block_t *block = payload_to_header(ptr);
  size_t copysize;
  void *newptr;

  // If size == 0, then free block and return NULL
  if (size == 0)
    {
      free(ptr);
      return NULL;
    }

  // If ptr is NULL, then equivalent to malloc
  if (ptr == NULL)
    {
      return malloc(size);
    }

  // Otherwise, proceed with reallocation
  newptr = malloc(size);
  // If malloc fails, the original block is left untouched
  if (!newptr)
    {
      return NULL;
    }

  // Copy the old data
  copysize = get_payload_size(block); // gets size of old payload
  if(size < copysize)
    {
      copysize = size;
    }
  memcpy(newptr, ptr, copysize);

  // Free the old block
  free(ptr);

  return newptr;
}

/*
 * calloc: Allocates a block with size at least (elements * size + dsize)
 *         through malloc, then initializes all bits in allocated memory to 0.
 *         Returns NULL on failure.
 */
void *calloc(size_t nmemb, size_t size)
{
  void *bp;
  size_t asize = nmemb * size;

  if (asize/nmemb != size)
    // Multiplication overflowed
    return NULL;

  bp = malloc(asize);
  if (bp == NULL)
    {
      return NULL;
    }
  // Initialize all bits to 0
  memset(bp, 0, asize);

  return bp;
}

/******** The remaining content below are helper and debug routines ********/

/*
 * extend_heap: Extends the heap with the requested number of bytes, and
 *              recreates epilogue header. Returns a pointer to the result of
 *              coalescing the newly-created block with previous free block, if
 *              applicable, or NULL in failure.
 */
static block_t *extend_heap(size_t size)
{
  void *bp;

  // Allocate an even number of words to maintain alignment
  size = round_up(size, dsize);
  if ((bp = mem_sbrk(size)) == (void *)-1)
    {
      return NULL;
    }

  // Initialize free block header/footer
  block_t *block = payload_to_header(bp);
  write_header(block, size, false);
  write_footer(block, size, false);
  // Create new epilogue header
  block_t *block_next = find_next(block);
  write_header(block_next, 0, true);

  // Coalesce in case the previous block was free
  return coalesce(block);
}

/* Coalesce: Coalesces current block with previous and next blocks if either
 *           or both are unallocated; otherwise the block is not modified.
 *           Returns pointer to the coalesced block. After coalescing, the
 *           immediate contiguous previous and next blocks must be allocated.
 */
static block_t *coalesce(block_t * block)
{
  block_t *block_next = find_next(block);
  block_t *block_prev = find_prev(block);

  bool prev_alloc =
    extract_alloc(*(find_prev_footer(block))) || block_prev == block;

  bool next_alloc = get_alloc(block_next);
  size_t size = get_size(block);
  // next block free
  if (prev_alloc && !next_alloc)        // Case 2
    {
      size += get_size(block_next);
      remove_from_free_list(block_next);
      write_header(block, size, false);
      write_footer(block, size, false);
    }
  // prev block free
  else if (!prev_alloc && next_alloc)        // Case 3
    {
      size += get_size(block_prev);
      block = block_prev;
      remove_from_free_list(block);
      write_header(block, size, false);
      write_footer(block, size, false);
    }
  // both are free
  else if (!prev_alloc && !next_alloc)
    {
      size += get_size(block_next) + get_size(block_prev);
      remove_from_free_list(block_prev);
      remove_from_free_list(block_next);
      block = block_prev;
      write_header(block, size, false);
      write_footer(block, size, false);
    }
  insert_in_free_list(block);
  return block;
}

/*
 * place: Places block with size of asize at the start of bp. If the remaining
 *        size is at least the minimum block size, then split the block to the
 *        the allocated block and the remaining block as free, which is then
 *        inserted into the segregated list. Requires that the block is
 *        initially unallocated.
 */
static void place(block_t *block, size_t asize)
{
  size_t csize = get_size(block);

  if ((csize - asize) >= min_block_size)
    {
      write_header(block, asize, true);
      write_footer(block, asize, true);
      remove_from_free_list(block);
      block = find_next(block);
      // footer depends on info in hdr so we must write header first
      write_header(block, csize-asize, false);
      write_footer(block, csize-asize, false);
      coalesce(block);
    }

  else
    {
      write_header(block, csize, true);
      write_footer(block, csize, true);
      remove_from_free_list(block);
    }
}

/*
 * find_fit: Looks for a free block with at least asize bytes with
 *           first-fit policy. Returns NULL if none is found.
 */
static block_t *find_fit(size_t asize)
{
  block_t *block;

  for (block = free_list_start; get_alloc(block) == false ;
       block = get_free_next(block))
    {
      if (!(get_alloc(block)) && (asize <= get_size(block)))
        {
          return block;
        }
    }
  return NULL; // no fit found
}

/*
 * max: returns x if x > y, and y otherwise.
 */
static size_t max(size_t x, size_t y)
{
  return (x > y) ? x : y;
}


/*
 * round_up: Rounds size up to next multiple of n
 */
static size_t round_up(size_t size, size_t n)
{
  return (n * ((size + (n-1)) / n));
}

/*
 * pack: returns a header reflecting a specified size and its alloc status.
 *       If the block is allocated, the lowest bit is set to 1, and 0 otherwise.
 */
static word_t pack(size_t size, bool alloc)
{
  return alloc ? (size | 1) : size;
}


/*
 * extract_size: returns the size of a given header value based on the header
 *               specification above.
 */
static size_t extract_size(word_t word)
{
  return (word & size_mask);
}

/*
 * get_size: returns the size of a given block by clearing the lowest 4 bits
 *           (as the heap is 16-byte aligned).
 */
static size_t get_size(block_t *block)
{
  return extract_size(block->header);
}

/*
 * get_payload_size: returns the payload size of a given block, equal to
 *                   the entire block size minus the header and footer sizes.
 */
static word_t get_payload_size(block_t *block)
{
  size_t asize = get_size(block);
  return asize - dsize;
}

/*
 * extract_alloc: returns the allocation status of a given header value based
 *                on the header specification above.
 */
static bool extract_alloc(word_t word)
{
  return (bool)(word & alloc_mask);
}

/*
 * get_alloc: returns true when the block is allocated based on the
 *            block header's lowest bit, and false otherwise.
 */
static bool get_alloc(block_t *block)
{
  return extract_alloc(block->header);
}

/*
 * write_header: given a block and its size and allocation status,
 *               writes an appropriate value to the block header.
 */
static void write_header(block_t *block, size_t size, bool alloc)
{
  block->header = pack(size, alloc);
}


/*
 * write_footer: given a block and its size and allocation status,
 *               writes an appropriate value to the block footer by first
 *               computing the position of the footer.
 */
static void write_footer(block_t *block, size_t size, bool alloc)
{
  word_t *footerp = (word_t *)((block->payload) + get_size(block) - dsize);
  *footerp = pack(size, alloc);
}


/*
 * find_next: returns the next consecutive block on the heap by adding the
 *            size of the block.
 */
static block_t *find_next(block_t *block)
{
  dbg_requires(block != NULL);
  block_t *block_next = (block_t *)(((char *)block) + get_size(block));
    
  dbg_ensures(block_next != NULL);
  return block_next;
}

/*
 * find_prev_footer: returns the footer of the previous block.
 */
static word_t *find_prev_footer(block_t *block)
{
  // Compute previous footer position as one word before the header
  return (&(block->header)) - 1;
}

/*
 * find_prev: returns the previous block position by checking the previous
 *            block's footer and calculating the start of the previous block
 *            based on its size.
 */
static block_t *find_prev(block_t *block)
{
  word_t *footerp = find_prev_footer(block);
  size_t size = extract_size(*footerp);
  return (block_t *)((char *)block - size);
}

/*
 * payload_to_header: given a payload pointer, returns a pointer to the
 *                    corresponding block.
 */
static block_t *payload_to_header(void *bp)
{
  return (block_t *)(((char *)bp) - offsetof(block_t, payload));
}

/*
 * header_to_payload: given a block pointer, returns a pointer to the
 *                    corresponding payload.
 */
static void *header_to_payload(block_t *block)
{
  return (void *)(block->payload);
}

/* mm_checkheap: checks the heap for correctness; returns true if
 *               the heap is correct, and false otherwise.
 *               can call this function using mm_checkheap(__LINE__);
 *               to identify the line number of the call site.
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
  if (!heap_listp) {
    printf("NULL heap list pointer!\n");
    return false;
  }

  block_t *curr = heap_listp;
  block_t *next;
  block_t *hi = mem_heap_hi();
  print_heap();
  while ((next = find_next(curr)) + 1 < hi) {
    word_t hdr = curr->header;
    word_t ftr = *find_prev_footer(next);

    if (hdr != ftr) {
      printf(
             "Header (0x%016lX) != footer (0x%016lX) at %p\n",
             hdr, ftr, curr
             );
      return false;
    }
    curr = next;
  }
#endif
  return true;

}
static void print_heap() {
  block_t *p = NULL;
  for (p = heap_listp; get_size(p) > 0; p = find_next(p)) {
    if (get_alloc(p)) {
      printf("[Alloc][sz: %lu][%p-%p]\n", get_size(p), p, ((char*)find_next(p))-wsize);
    } else {
      printf("[Free][sz: %lu][%p---%p] PREV: %p NEXT: %p\n", get_size(p),p, ((char*)find_next(p))-wsize, get_free_prev(p), get_free_next(p));
    }
    
  }
}

static block_t *get_free_prev(block_t *block) {
  return block->prev;
}

static block_t *get_free_next(block_t *block) {
  return block->next;
}

static void remove_from_free_list(block_t *block) {

  block_t *next = get_free_next(block);
  block_t *prev = get_free_prev(block);

  if (prev) {
    prev->next = block->next;
  } else {
    free_list_start = block->next;
  }
  next->prev = prev;
}

static void insert_in_free_list(block_t *block) {
  block_t *p = free_list_start;
  while (p < block && !get_alloc(p)) {
    p = get_free_next(p);
  }
  if (p == free_list_start) {
    // insert before head
    free_list_start = block;
  }
  block->next = p;
  block->prev = p->prev;
  if (block->prev != NULL) {
    block->prev->next = block;
  }
  p->prev = block;
  free_list_start->prev = NULL;
}

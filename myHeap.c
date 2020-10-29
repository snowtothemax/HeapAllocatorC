///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
// Used by permission Fall 2020, CS354-deppeler
//
///////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "myHeap.h"

/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader
{
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *currentBlock = NULL; // the the address of the most recently alloc mem is heapStart first

/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void *myAlloc(int size)
{
    // local variables
    int blockSize = 0;              // actual size of block that needs allocation
    int padSize = 0;                // size of padding if needed
    int allocated = 0;              // int to track whether the block has been allocated or not;
    int curr_size = 0;              // integer to track the size of the current block, used for iterating.
    int counter = 0;                // counts the number of times that the while loop has iterated
    blockHeader *allocBlock = NULL; // new block to add to the heap if necessary

    // check if the current block is still null, set to heapstart if so
    if (currentBlock == NULL)
    {
        currentBlock = heapStart;
    }

    // checks if the size is negative or if  greater than the allocated
    // heap space. returns null if thats the case
    if (size < 0 || size > allocsize)
    {
        return NULL;
    }

    // Add header size
    blockSize = size + sizeof(blockHeader);

    // double word align the blocksize
    if ((blockSize % 8) != 0)
    {
        // the size of the padding necessary for double word alignment
        padSize = 8 - (blockSize % 8);
    }
    else
    {
        // set padSize to 0 if there is no padding necessary
        padSize = 0;
    }

    // set the blockSize += padSize for double word aligned block size
    blockSize += padSize;

    // check if the block size is greater than the allocSize, return null if true
    if (blockSize > allocsize)
    {
        return NULL;
    }

    /**
     * This section now deals with going through the heap and  
     * allocating blocks.
     */

    // while loop for iterating through the heap. Checks to make sure the
    // function hasnt allocated anything yet and the allocBlock is not equal to the current block
    while (allocated == 0 && allocBlock != currentBlock)
    {
        // if the first time iterating, set the allocBlock equal to the current block
        if (counter == 0)
        {
            allocBlock = currentBlock;
        }

        // set curr_size = size of the current block in iteration, and remove
        // check if there are any ones or twos present in the size status
        curr_size = allocBlock->size_status;

        // checks if bit0 == 1. if so, then the block is not free
        // iterate to next block
        if (allocBlock->size_status & 1)
        {
            // decrement curr_size
            curr_size--;

            // checks if bit1 == 1, decrement the curr_size for iteration
            if (allocBlock->size_status & 2)
            {
                curr_size -= 2;
            }

            // set allocBlock = to the next block. if allocBlock is equal to endmark
            // set to the heapstart
            allocBlock = (blockHeader *)((char *)allocBlock + curr_size);
            if (allocBlock->size_status == 1)
            {
                allocBlock = heapStart;
            }

            //increment counter
            counter++;
        }
        else // block is free
        {
            // check if the block is the correct size or more.
            // if not, iterate to next block.
            if (curr_size >= blockSize)
            {
                //check if the size_status bit1 == 1
                if (curr_size & 2)
                {
                    curr_size -= 2;
                }

                // we now check if the curr_size is the same size as blockSize.
                // if not we know we have to split the block
                if (curr_size == blockSize)
                {
                    // add one to the size status to indicate allocated.
                    // indicate allocation has occured
                    allocBlock->size_status += 1;
                    allocated = 1;

                    // update the footer to indicate not free
                    blockHeader *footer = (blockHeader *)((char *)allocBlock + curr_size - sizeof(blockHeader));
                    footer->size_status = 0;

                    // update the next block header (if not the endmark) to indicate
                    // the previous block (the block just allocated) is alloc
                    blockHeader *nextBlock = (blockHeader *)((char *)allocBlock + curr_size);
                    if (nextBlock->size_status != 1)
                    {
                        nextBlock->size_status += 2;
                    }
                    else // nextBlock was endmark. set to heapStart
                    {
                        // set the nextBlock = heapStart
                        nextBlock = heapStart;
                    }

                    // set the currentBlock = nextBlock
                    currentBlock = allocBlock;

                    // return the address of the allocBlock
                    return (void *)allocBlock + sizeof(blockHeader);
                }
                else //split the blocks
                {
                    // size of the block made after splitting
                    int split_size = curr_size - blockSize;

                    // new block made after splitting. update size status and footer for split block
                    blockHeader *splitBlock = (blockHeader *)((char *)allocBlock + blockSize);
                    splitBlock->size_status = split_size + 2;
                    blockHeader *splitFooter = (blockHeader *)((char *)splitBlock + split_size - sizeof(blockHeader));
                    splitFooter->size_status = split_size;

                    // now we update allocBlock and indicate allocation.
                    // check to see if the allocBlock indicated the previous block was allocated.
                    int p_alloc = 0;
                    if (allocBlock->size_status & 2)
                    {
                        p_alloc = 2;
                    }
                    allocBlock->size_status = blockSize + 1 + p_alloc;
                    allocated = 1;

                    // set the currentBlock to be the splitBlock
                    currentBlock = allocBlock;

                    // return pointer to the allocBlock
                    return (void *)allocBlock + sizeof(blockHeader);
                }
            }
            else // block not correct size, move to next block
            {
                // check if size status includes 2
                if (curr_size & 2)
                {
                    curr_size -= 2;
                }

                // set the alloc block = to the next block. if allocBlock is the endmark
                // set allocBlock = heapStart
                allocBlock = (blockHeader *)((char *)allocBlock + curr_size);
                if (allocBlock->size_status == 1)
                {
                    allocBlock = heapStart;
                }

                // increment the counter
                counter++;
            }
        }
    }
    return NULL;
}

/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */
int myFree(void *ptr)
{
    // Local vars
    blockHeader *blockToFree = (blockHeader *)((void *)ptr - sizeof(blockHeader)); // block to be freed
    blockHeader *freeBlockFooter = NULL;                                           // null for now
    int p_bit = 0;                                                                 // blocks p bit
    int free_size = blockToFree->size_status;                                      // blocks size to be freed
    int can_free_right = 0;                                                        // determines if the block to the right can be freed
    int can_free_left = 0;                                                         // determines if the block to the left can be freed
    int free_is_current = 0;                                                       // determines if the block being freed/coalesced was the last block allocated

    //check if null
    if (blockToFree == NULL)
    {
        return -1;
    }

    // calc a bit. if not = 1, then return -1
    if (free_size & 1)
    {
        // subtract a bit from the freeSize
        free_size--;
    }
    else // block is already free
    {
        return -1;
    }

    // calculate the p bit
    if (free_size & 2)
    {
        p_bit = 2;
        free_size -= 2;
    }

    // check if the size to be freed is greater than
    // the allocated size of the heap
    if (free_size > allocsize)
    {
        return -1;
    }

    // check if the block is 8 bit aligned
    if ((free_size % 8) != 0)
    {
        return -1;
    }

    // check if the current block to be freed is the block that was most recently allocated
    if (currentBlock == blockToFree)
    {
        free_is_current = 1;
    }

    // set the footer of the block
    freeBlockFooter = (blockHeader *)((char *)blockToFree + free_size - sizeof(blockHeader));

    // get the block "to the right" of the block
    // to be freed
    blockHeader *blockAdjRight = (blockHeader *)((char *)blockToFree + free_size);
    int block_right_size = 0; // sizeof the block to the right

    // we now determine if the block to the right can be freed.
    // we now check if the block to the right is the endmark. if not, then check if it is free
    if (blockAdjRight->size_status != 1)
    {
        if ((blockAdjRight->size_status & 1) == 0)
        {
            // indicate that the block to the right is free
            can_free_right = 1;

            // get the size of the block to the right
            block_right_size = blockAdjRight->size_status;

            // arithmetic to rid of the p bit from the block right size
            if (block_right_size & 2)
            {
                block_right_size -= 2;
            }
        }
    }

    // block adj left of the block to be freed
    blockHeader *blockAdjLeft = NULL; // null for now.
    blockHeader *prevFooter = NULL;   // null for now
    int block_left_size = 0;          // size of the block adj to the left

    // if the p bit == 0, previous block is free.
    if ((blockToFree->size_status & 2) == 0)
    {
        // indicate block to the left is free
        can_free_left = 1;

        // we get the previous block size by looking into its footer
        prevFooter = (blockHeader *)((char *)blockToFree - sizeof(blockHeader));
        block_left_size = prevFooter->size_status;

        // get the actual previous block
        blockAdjLeft = (blockHeader *)((char *)blockToFree - block_left_size);
    }

    // check if we can coalesce with the block to the right first
    if (can_free_right == 1)
    {
        // increment size of the new free block
        free_size += block_right_size;
    }

    // check if you can free the left
    if (can_free_left == 1)
    {
        // check if p bit in prev block.
        if (block_left_size & 2)
        {
            p_bit = 2;
            block_left_size -= 2;
        }
        else
        {
            p_bit = 0;
        }

        // increment size of the free block.
        free_size += block_left_size;

        // set the new blockHeader equal to the left block
        blockToFree = blockAdjLeft;
    }

    // we now free the block
    blockToFree->size_status = free_size + p_bit;
    freeBlockFooter = (blockHeader *)((char *)blockToFree + free_size - sizeof(blockHeader));
    freeBlockFooter->size_status = free_size;

    // set the header of the block to the right that it is free
    blockAdjRight = (blockHeader *)((char *)blockToFree + free_size);
    if ((blockAdjRight->size_status & 2) && blockAdjRight->size_status != 1)
    {
        blockAdjRight->size_status -= 2;
    }

    // check if the block is the heapstart, if so set to p bit 2 one
    if ((blockToFree == heapStart) && (blockToFree->size_status & 2) == 0)
    {
        blockToFree->size_status += 2;
    }

    // if the freed block was the block to free, set current = block to free
    if (free_is_current == 1)
    {
        currentBlock = blockToFree;
    }

    return 0;
}

/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int myInit(int sizeOfRegion)
{

    static int allocated_once = 0; //prevent multiple myInit calls

    int pagesize;   // page size
    int padsize;    // size of padding when heap size not a multiple of page size
    void *mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader *endMark;

    if (0 != allocated_once)
    {
        fprintf(stderr,
                "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0)
    {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd)
    {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr)
    {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }

    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader *)mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader *)((void *)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader *)((void *)heapStart + allocsize - 4);
    footer->size_status = allocsize;

    return 0;
}

/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */
void dispMem()
{

    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");

    while (current->size_status != 1)
    {
        t_begin = (char *)current;
        t_size = current->size_status;

        if (t_size & 1)
        {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        }
        else
        {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2)
        {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        }
        else
        {
            strcpy(p_status, "Free");
        }

        if (is_used)
            used_size += t_size;
        else
            free_size += t_size;

        t_end = t_begin + t_size - 1;

        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status,
                p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);

        current = (blockHeader *)((char *)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;
}

// end of myHeap.c (fall 2020)

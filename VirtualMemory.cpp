#include <cassert>
#include <algorithm>
#include <cstdio>
#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include "MemoryConstants.h"

// Helper- calculate the number of bits needed to represent a given size
// i am not sure if it is needed- ask ariel
int log2ceil(uint64_t x) {
    int bits = 0;
    --x;
    while (x > 0) {
        bits++;
        x >>= 1;
    }
    return bits;
}

// Initialize the root page table (at frame 0)
void VMinitialize() {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(i, 0);
    }
}

// returns the page table index at the given level for a given virtual address
uint64_t getLevelIndex(uint64_t pageAddress, int level) {

    // calculate the amount of bits in the address per each level
    int bitsPerLevel = (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH) / TABLES_DEPTH;

    // calculate the amount of level-addresses in the hole address that needs "cleaning", so that the next remaining
    // level-address will be the wanted one
    int levelsToClean = TABLES_DEPTH - level - 1;

    // bitwise right shift so that the right-most bits will be the ones that belong to the wanted level.
    pageAddress = pageAddress >> (bitsPerLevel * levelsToClean);

    // use bitwise "and" (conjunction) with a bit mask to isolate the wanted bits
    pageAddress = pageAddress & ((1 << bitsPerLevel) - 1);

    return pageAddress;
}

// int getLevelIndex(uint64_t virtualAddress, int level) {
// //    int offsetBits=OFFSET_WIDTH;
//     int offsetBits = log2ceil(PAGE_SIZE);
//     int levels = TABLES_DEPTH;
//     int totalBits = VIRTUAL_ADDRESS_WIDTH;
//     int bitsPerLevel = (totalBits - offsetBits) / levels;
//     int levelsBefore = (levels - level - 1);
//     // clean the offset and the levels before, now the first bits are the
//     // ones needed
//     virtualAddress=virtualAddress >> (offsetBits+bitsPerLevel *levelsBefore);
//     // takes only the needed part
//     virtualAddress=virtualAddress & ((1 << bitsPerLevel) - 1);
//     return int(virtualAddress);
// }

// return the offset in the page
int getOffset(uint64_t virtualAddress) {
    return int(virtualAddress & (PAGE_SIZE - 1));
}

// Check if a frame is an empty table (all zeros)
bool isEmptyTable(uint64_t frame) {
  word_t val;
  for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
      PMread((frame * PAGE_SIZE) + i, &val);
      if (val != 0) {
          return false;
        }
    }
  return true;
}


// Recursively traverse the page table tree to find best frame according to priorities
void traverse(const uint64_t currentFrame, // current frame looked at in the recursive traversion
              uint64_t& maxFrame, // the maximal frame number in use seen so far
              uint64_t& emptyFrame, // an empty frame that was found during traverse
              const uint64_t targetPage, // the page that needs to be matched with a frame
              uint64_t& evictFrame, // best candidate frame for eviction seen so far
              uint64_t& evictPage, // matching page for evictFrame
              const uint64_t forbiddenFrame, // a forbiden frame to evict - will be the frame that matches the parent of the
                                            // target page (could be all zeros)
              uint64_t& pointerFromParentToEvict, // frame in parent to the evictPage
              const uint64_t depth = 0, // current depth in the tranverse
              const uint64_t currentPage = 0 // current page looked at in the recursive traversion

) {

    word_t val;

    // look at all of the indexes in the given frame, and handle if it is not 0
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(currentFrame * PAGE_SIZE + i, &val);
        if (val == 0) {
            continue;
        }

        // update the maximal frame number encountered
        maxFrame = std::max(maxFrame, (uint64_t)val);
        // printf("in frame: %lu we have val: %d  and maxFrame: %lu\n",frame, val, maxFrame);

        //if we are at the last table
        // uint64_t d = TABLES_DEPTH;
        if (depth + 1 == TABLES_DEPTH) {
            // reconstruct the full page number of the currently looked-at leaf
            uint64_t candidatePage = (currentPage << (uint64_t)log2ceil(PAGE_SIZE)) | i;
            if (evictFrame == UINT64_MAX) {
                evictPage = candidatePage;
                evictFrame = val;
                pointerFromParentToEvict = currentFrame * PAGE_SIZE + i;
            }
            else {
                // calculate cyclical distance of candidate
                // uint64_t candidateDiff = targetPage > candidatePage ? targetPage - candidatePage : candidatePage - targetPage;
                uint64_t candidateDiff = targetPage > candidatePage ? targetPage - candidatePage : candidatePage - targetPage;
                uint64_t distance = std::min(static_cast<uint64_t>(NUM_PAGES - candidateDiff), candidateDiff);

                // uint64_t distance = std::min((uint64_t)NUM_PAGES - std::abs(targetPage - candidatePage),
                //                              std::abs(targetPage - candidatePage));

                // calculate cyclical distance of current evictPage
                uint64_t evictDiff = targetPage > evictPage ? targetPage - evictPage : evictPage - targetPage;
                uint64_t evictDistance = std::min(static_cast<uint64_t>(NUM_PAGES - evictDiff), evictDiff);


                // uint64_t evictDistance = std::min((uint64_t)NUM_PAGES - std::abs(targetPage - evictPage),
                //     std::abs(targetPage - evictPage));
                if (distance > evictDistance) {
                    evictPage = candidatePage;
                    evictFrame = val;
                    pointerFromParentToEvict = currentFrame * PAGE_SIZE + i;
                }
            }
        }

        // Intermediate table
        else
        {
            // if found an empty frame
            if (isEmptyTable (val)
                && emptyFrame == UINT64_MAX
                && static_cast<uint64_t>(val) != forbiddenFrame
                && depth + 1 < TABLES_DEPTH) {

                PMwrite (currentFrame * PAGE_SIZE + i, 0);
                emptyFrame = val;
            }
            else {
                traverse (val, maxFrame, emptyFrame, targetPage, evictFrame, evictPage,
                          forbiddenFrame, pointerFromParentToEvict, depth + 1,
                          (currentPage << log2ceil (PAGE_SIZE)) | i);
            }
        }
    }
}

// finding frame according to the priorities.
// recieve the forbidden frame and target page, return the frame to allocate in
uint64_t allocateFrame(uint64_t forbidden, uint64_t targetPage) {
  uint64_t maxFrame = 0, emptyFrame = UINT64_MAX, evictFrame = UINT64_MAX,
    evictPage = UINT64_MAX, pointerFromParentToEvict = UINT64_MAX;
  traverse(0, maxFrame, emptyFrame, targetPage, evictFrame, evictPage,
      forbidden, pointerFromParentToEvict);

  // // just make sure i am not giving back the main table as empty one.
  // if(emptyFrame==0){
  //     printf ("empty frame is 0- problem");
  // }

    // we have an empty frame to use (was an empty table before)
  if (emptyFrame != UINT64_MAX && emptyFrame != forbidden) {
      // printf ("empty table entrance\n");
//      assert(emptyFrame < NUM_FRAMES);

      return emptyFrame;
    }
    // we have an unused frame at the end of all frames
  if (maxFrame + 1 < NUM_FRAMES && maxFrame + 1 != forbidden) {
      // printf ("unused frame entrance\n");
//      assert(maxFrame + 1 < NUM_FRAMES);
      // printf ("the max frame in use is %lu\n", maxFrame);
      return maxFrame + 1;
    }
    // third praiority, swap exiting one
  if (evictFrame != UINT64_MAX && evictFrame != forbidden) {
      // printf ("swap\n");
//      assert(evictFrame < NUM_FRAMES);
      PMevict(evictFrame, evictPage);
      PMwrite(pointerFromParentToEvict, 0);
      return evictFrame;
    }

  // If all options failed
//  fprintf(stderr, "[ERROR] No available frame found. This should not happen.\n");
  assert(false);
  return 0; // not reached
}

//find a new frame, zeros it if it's a table, and links it to the parent
uint64_t handlePageFault(uint64_t parentFrame,
    uint64_t indexInParent,
    bool isTable,
    uint64_t virtualPage
    ) {

    // get some new frame
    uint64_t newFrame = allocateFrame(parentFrame, virtualPage);
    // assert(newFrame < NUM_FRAMES);

    // set the new parent of the new frame to be the parent frame (with index offset).
    // assume prevoius parent (if exsisted) was already detatched in allocateFrame().
    PMwrite(parentFrame * PAGE_SIZE + indexInParent, newFrame);

    // if the new frame needs to be a table, set all of its values to 0
    if (isTable) {
      for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
          PMwrite((newFrame * PAGE_SIZE) + i, 0);
        }
    }
    else {
        PMrestore(newFrame, virtualPage);
    }
  return newFrame;
}



// Translate virtual address to physical address, creating pages/tables if needed
bool virtualToPhysical(uint64_t virtualAddress, uint64_t &physicalAddress,
                       bool isWrite) {
  // initiate first frame to be 0 - the table's root (always)
  uint64_t frame = 0;
  word_t addr;
  // word_t check;

  // bitwise right shift of OFFSET_WIDTH positions to isolate the address of the page
  uint64_t pageNumber = virtualAddress >> OFFSET_WIDTH;

  // itterate over levels of the page table tree - advance as needed until reaching wanted address
  for (int level = 0; level < TABLES_DEPTH; level++) {
      // get the level index, that is the index within the current table in the tree that we will go into
      uint64_t index = getLevelIndex(pageNumber, level);
      // int index = getLevelIndex(virtualAddress, level);

      // read the information in the wanted index and write into addr
      PMread((frame * PAGE_SIZE) + index, &addr);

      // printf("frame %llu, addr %d \n", (long long int) frame,addr);

      if (addr == 0) {
          bool isTable = (level < TABLES_DEPTH - 1);
          addr = handlePageFault(frame, index, isTable, pageNumber);

          // just a check- the valuse should be equal to addr
          // PMread(frame * PAGE_SIZE + index, &check);
          // printf("value in the new spot %d, addr %d \n", check, addr);
        }
      // update current frame
      frame = addr;
    }

  // generate full physical address (including offset)
  int offset = getOffset(virtualAddress);
  physicalAddress = frame * PAGE_SIZE + offset;
  return true;
}


int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    uint64_t physAddr;
    if (!virtualToPhysical(virtualAddress, physAddr, false)) return 0;
    PMread(physAddr, value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) return 0;
    uint64_t physAddr;
    if (!virtualToPhysical(virtualAddress, physAddr, true)) return 0;
    PMwrite(physAddr, value);
    return 1;
}

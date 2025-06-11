#include <cassert>
#include <algorithm>
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

// return the index represent the given level from the virtual address
int getLevelIndex(uint64_t virtualAddress, int level) {
//    int offsetBits=OFFSET_WIDTH;
    int offsetBits = log2ceil(PAGE_SIZE);
    int levels = TABLES_DEPTH;
    int totalBits = VIRTUAL_ADDRESS_WIDTH;
    int bitsPerLevel = (totalBits - offsetBits) / levels;
    int levelsBefore = (levels - level - 1);
    // clean the offset and the levels before, now the first bits are the
    // ones needed
    virtualAddress=virtualAddress >> (offsetBits+bitsPerLevel *levelsBefore);
    // takes only the needed part
    virtualAddress=virtualAddress & ((1 << bitsPerLevel) - 1);
    return int(virtualAddress);
}

// return the offset in the page
int getOffset(uint64_t virtualAddress) {
    return int(virtualAddress & (PAGE_SIZE - 1));
}

// Check if a frame is an empty table (all zeros)
bool isEmptyTable(uint64_t frame) {
  word_t val;
  for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
      PMread(frame * PAGE_SIZE + i, &val);
      if (val != 0) {
          return false;
        }
    }
  return true;
}

// Recursively traverse the page table tree to find best frame according to priority
void traverse(uint64_t frame, uint64_t& maxFrame, uint64_t& emptyFrame, uint64_t targetPage, uint64_t& evictFrame, uint64_t& evictPage, uint64_t depth = 0, uint64_t virtualPrefix = 0) {
  word_t val;
  for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
      PMread(frame * PAGE_SIZE + i, &val);
      if (val != 0) {
          maxFrame = std::max(maxFrame, (uint64_t)val);
          //if we are at the last table
          if (depth + 1 == TABLES_DEPTH) {
              // Leaf node (page)
//              uint64_t candidatePage = (virtualPrefix << (uint64_t)log2ceil(PAGE_SIZE)) | i;
//              uint64_t distance = std::min((uint64_t)NUM_PAGES - std::abs((int64_t)targetPage - (int64_t)candidatePage), std::abs((int64_t)targetPage - (int64_t)candidatePage));
//              uint64_t evictDistance = std::min((uint64_t)NUM_PAGES - std::abs((int64_t)targetPage - (int64_t)evictPage), std::abs((int64_t)targetPage - (int64_t)evictPage));
//              if (distance > evictDistance) {
//                  evictPage = candidatePage;
//                  evictFrame = val;
//                }
            } else {
              // Intermediate table
              traverse(val, maxFrame, emptyFrame, targetPage, evictFrame, evictPage, depth + 1, (virtualPrefix << log2ceil(PAGE_SIZE)) | i);
              if (isEmptyTable(val) && emptyFrame == UINT64_MAX) {
                  emptyFrame = val;
                  PMwrite(frame * PAGE_SIZE + i, 0);
                }
            }
        }
    }
}

// finding frame according to the priorities.
// recieve the forbidden frame and target page, return the frame to allocate in
uint64_t allocateFrame(uint64_t forbidden, uint64_t targetPage) {
  uint64_t maxFrame = 0, emptyFrame = UINT64_MAX, evictFrame = UINT64_MAX, evictPage = 0;
  traverse(0, maxFrame, emptyFrame, targetPage, evictFrame, evictPage);

  // we have an empty frame to use (was an empty table before)
  if (emptyFrame != UINT64_MAX && emptyFrame != forbidden) {
//      assert(emptyFrame < NUM_FRAMES);
      return emptyFrame;
    }
    // we have an unused frame at the end of all frames
  if (maxFrame + 1 < NUM_FRAMES && maxFrame + 1 != forbidden) {
//      assert(maxFrame + 1 < NUM_FRAMES);
      return maxFrame + 1;
    }
    // third praiority, swap exiting one
  if (evictFrame != UINT64_MAX && evictFrame != forbidden) {
//      assert(evictFrame < NUM_FRAMES);
      return evictFrame;
    }

  // If all options failed
//  fprintf(stderr, "[ERROR] No available frame found. This should not happen.\n");
  assert(false);
  return 0; // not reached
}

//find a new frame, zeros it if it's a table, and links it to the parent
uint64_t handlePageFault(uint64_t parentFrame, int indexInParent, bool isTable, uint64_t virtualPage, uint64_t forbidden = UINT64_MAX) {
//  if(forbidden==0){
//    forbidden=UINT64_MAX;
//  }
  uint64_t newFrame = allocateFrame(forbidden, virtualPage);
  assert(newFrame < NUM_FRAMES);
  if (isTable) {
      for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
          PMwrite(newFrame * PAGE_SIZE + i, 0);
        }
    }
  PMwrite(parentFrame * PAGE_SIZE + indexInParent, newFrame);
  return newFrame;
}



// Translate virtual address to physical address, creating pages/tables if needed
bool virtualToPhysical(uint64_t virtualAddress, uint64_t &physicalAddress,
                       bool isWrite) {
  uint64_t frame = 0;
  word_t addr;
  uint64_t virtualPage = virtualAddress >> OFFSET_WIDTH;

  for (int level = 0; level < TABLES_DEPTH; level++) {
      int index = getLevelIndex(virtualAddress, level);
      PMread(frame * PAGE_SIZE + index, &addr);

      if (addr == 0) {
          bool isTable = (level < TABLES_DEPTH - 1);
          addr = handlePageFault(frame, index, isTable, virtualPage,0);
        }

      frame = addr;
    }

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

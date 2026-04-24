#include "pin.H"
#include <assert.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <stack>
#include <stdio.h>
#include <types.h>

using namespace std;

UINT32 logPoolSize;
UINT32 logPageSize;
UINT32 frameSize;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "results.out",
                            "specify optional output file name");

KNOB<UINT32> KnobLogPageSize(KNOB_MODE_WRITEONCE, "pintool", "p", "12",
                             "specify the log of page size in bytes");

KNOB<UINT32> KnobLogNumRows(KNOB_MODE_WRITEONCE, "pintool", "r", "10",
                            "specify the log of number of rows in the cache");

KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool", "a", "2",
                               "specify the associativity of the cache");

KNOB<UINT32> KnobLogPoolSize(KNOB_MODE_WRITEONCE, "pintool", "s", "5",
                             "specify the size of the frame pool");

class LruTLB {
private:
  UINT32 missCount;
  UINT32 hitCount;
  UINT32 flushCount;
  UINT32 logNumRows;
  UINT32 associativity;
  UINT32 numSets;

  struct Entry {
    bool valid;
    UINT32 tag;
    UINT32 ppn;
    UINT64 lru;
  };

  Entry **sets;
  UINT64 counter;
  PIN_LOCK lock;

public:
  // 2^10 rows, and 2 way
  LruTLB(UINT32 logNumRowsParam, UINT32 associativityParam) {
    PIN_InitLock(&lock);
    logNumRows = logNumRowsParam;
    associativity = associativityParam;
    numSets = 1U << logNumRows;
    missCount = 0;
    hitCount = 0;
    flushCount = 0;
    counter = 0;
    sets = new Entry *[numSets];
    for (UINT32 i = 0; i < numSets; i++) {
      sets[i] = new Entry[associativity];
      for (UINT32 j = 0; j < associativity; j++) {
        sets[i][j].valid = false;
        sets[i][j].tag = 0;
        sets[i][j].ppn = 0;
        sets[i][j].lru = 0;
      }
    }
  }

  void acquireLock() { PIN_GetLock(&lock, 1); }
  void releaseLock() { PIN_ReleaseLock(&lock); }

  UINT32 physicalPage(UINT32 virtualAddr) {
    PIN_GetLock(&lock, 1);
    UINT32 vpn = virtualAddr >> logPageSize;
    UINT32 setIdx = vpn & (numSets - 1);
    UINT32 tag = vpn >> logNumRows;
    for (UINT32 i = 0; i < associativity; i++) {
      if (sets[setIdx][i].valid && sets[setIdx][i].tag == tag) {
        hitCount++;
        sets[setIdx][i].lru = ++counter;
        UINT32 ret = sets[setIdx][i].ppn;
        PIN_ReleaseLock(&lock);
        return ret;
      }
    }
    missCount++;
    PIN_ReleaseLock(&lock);
    return 0;
  }

  void cacheTranslation(UINT32 virtualAddr, UINT32 translation) {
    PIN_GetLock(&lock, 1);
    if (translation == 0) {
      PIN_ReleaseLock(&lock);
      return;
    }
    UINT32 vpn = virtualAddr >> logPageSize;
    UINT32 setIdx = vpn & (numSets - 1);
    UINT32 tag = vpn >> logNumRows;
    // first if match
    for (UINT32 i = 0; i < associativity; i++) {
      if (sets[setIdx][i].valid && sets[setIdx][i].tag == tag) {
        sets[setIdx][i].ppn = translation;
        sets[setIdx][i].lru = ++counter;
        PIN_ReleaseLock(&lock);
        return;
      }
    }
    // second if there is slot
    for (UINT32 i = 0; i < associativity; i++) {
      if (!sets[setIdx][i].valid) {
        sets[setIdx][i].valid = true;
        sets[setIdx][i].tag = tag;
        sets[setIdx][i].ppn = translation;
        sets[setIdx][i].lru = ++counter;
        PIN_ReleaseLock(&lock);
        return;
      }
    }
    // third if no slot the kick out the olddest
    UINT32 victim = 0;
    UINT64 minLru = sets[setIdx][0].lru;
    for (UINT32 i = 1; i < associativity; i++) {
      if (sets[setIdx][i].lru < minLru) {
        minLru = sets[setIdx][i].lru;
        victim = i;
      }
    }
    sets[setIdx][victim].tag = tag;
    sets[setIdx][victim].ppn = translation;
    sets[setIdx][victim].lru = ++counter;
    PIN_ReleaseLock(&lock);
  }

  UINT32 numMisses() { return missCount; }
  UINT32 numHits() { return hitCount; }
  UINT32 numFlushes() { return flushCount; }

  void flushNoLock() {
    flushCount++;
    for (UINT32 i = 0; i < numSets; i++) {
      for (UINT32 j = 0; j < associativity; j++) {
        sets[i][j].valid = false;
      }
    }
  }

  void flush() {
    PIN_GetLock(&lock, 1);
    flushNoLock();
    PIN_ReleaseLock(&lock);
  }

  ~LruTLB() {
    for (UINT32 i = 0; i < numSets; ++i) {
      delete[] sets[i];
    }
    delete[] sets;
  }
};

class Page {
private:
  bool _is_free;
  UINT32 _size;
  UINT32 *_data;
  UINT32 _address;
  friend class PageAllocator;

public:
  Page(UINT32 size, UINT32 address) {
    _size = size;
    _is_free = true;
    // physical address
    _address = address;
    // page frame
    _data = new uint32_t[size];
  }

  UINT32 wordAt(UINT32 index) {
    assert(index < _size);
    assert(!_is_free);
    return _data[index];
  }

  VOID setWordAt(UINT32 word, UINT32 index) {
    assert(!_is_free);
    assert(index < _size);
    _data[index] = word;
  }

  UINT32 address() const { return _address; }
  ~Page() { delete[] _data; }
};

class PageAllocator {
private:
  UINT32 _pool_size;
  UINT32 _frame_size;
  std::map<UINT32, Page *> _page_map;
  std::stack<UINT32> _frame_stack;
  UINT32 _start_address;

public:
  // logPoolSize default is 5
  PageAllocator(UINT32 logFrameSize, UINT32 logPoolSize) {
    Page *temp = nullptr;
    // 2^5 = 32
    _pool_size = 1U << logPoolSize;
    // 4kb
    _frame_size = 1U << logFrameSize;
    // set as page size
    _start_address = _frame_size;
    UINT32 frame_size_in_words = _frame_size / sizeof(UINT32);
    // not start from zero, leave the address from 0 to 4kb
    UINT32 address_ptr = _start_address;
    for (UINT32 i = 0; i < _pool_size; i++) {
      temp = new Page(frame_size_in_words, address_ptr);
      _frame_stack.push(address_ptr);
      _page_map[address_ptr] = temp;
      _page_map[address_ptr]->_is_free = true;
      address_ptr += _frame_size;
    }
  }

  ~PageAllocator() {
    for (auto &[addr, p] : _page_map) {
      delete p;
    }
  }

  UINT32 requestPage() {
    if (_frame_stack.empty())
      return 0;
    UINT32 page_addr = _frame_stack.top();
    _frame_stack.pop();
    _page_map[page_addr]->_is_free = false;
    return page_addr;
  }

  // return the ppn
  Page *pageAtAddress(UINT32 address) {
    if (_page_map.find(address) != _page_map.end())
      return _page_map[address];
    return nullptr;
  }

  void freePage(UINT32 page_addr) {
    if (_page_map.find(page_addr) != _page_map.end())
      _page_map[page_addr]->_is_free = true;
  }
};

LruTLB *tlb;
Page *rootPage;
PageAllocator *pageAllocator;

// frameSize = 12
UINT32 pageTableWalk(UINT32 virtualAddr, UINT32 frameSize) {
  tlb->acquireLock();

  static bool rootInitialized = false;
  static stack<UINT32> spareFrames;
  static UINT32 evictL1 = 0;
  static UINT32 evictL2 = 0;

  UINT32 entriesPerTable = (1U << frameSize) / sizeof(UINT32);
  UINT32 indexBits = frameSize - 2;
  UINT32 indexMask = entriesPerTable - 1;

  // make sure the first level page table is cleared
  if (!rootInitialized) {
    for (UINT32 idx = 0; idx < entriesPerTable; idx++)
      rootPage->setWordAt(0, idx);
    rootInitialized = true;
  }

  UINT32 vpn = virtualAddr >> frameSize;
  UINT32 l2Idx = vpn & indexMask;
  UINT32 l1Idx = (vpn >> indexBits) & indexMask;

  auto getFrame = [&]() -> UINT32 {
    UINT32 addr = pageAllocator->requestPage();
    if (addr != 0)
      return addr;
    if (!spareFrames.empty()) {
      addr = spareFrames.top();
      spareFrames.pop();
      return addr;
    }
    return 0;
  };

  // round-robbin eviction algo
  auto evictOne = [&]() -> bool {
    for (UINT32 i = 0; i < entriesPerTable; i++) {
      UINT32 ei = (evictL1 + i) & indexMask;
      UINT32 l2Addr = rootPage->wordAt(ei);
      if (l2Addr == 0)
        continue;
      Page *l2P = pageAllocator->pageAtAddress(l2Addr);
      UINT32 startJ = (ei == evictL1) ? evictL2 : 0;
      for (UINT32 j = 0; j < entriesPerTable; j++) {
        UINT32 ej = (startJ + j) & indexMask;
        UINT32 d = l2P->wordAt(ej);
        if (d != 0) {
          spareFrames.push(d);
          l2P->setWordAt(0, ej);
          tlb->flushNoLock();
          // don't evit the current l1idx corresponding l2 page
          if (ei != l1Idx) {
            bool empty = true;
            for (UINT32 k = 0; k < entriesPerTable; k++) {
              if (l2P->wordAt(k) != 0) {
                empty = false;
                break;
              }
            }
            if (empty) {
              spareFrames.push(l2Addr);
              rootPage->setWordAt(0, ei);
            }
          }
          // think it as the matrix row and col
          // only col reach the end then you need to increase row
          evictL1 = ei;
          evictL2 = (ej + 1) & indexMask;
          if (evictL2 == 0)
            evictL1 = (evictL1 + 1) & indexMask;
          return true;
        }
      }
    }
    return false;
  };

  UINT32 l2PageAddr = rootPage->wordAt(l1Idx);
  // page fault for the l2 page table
  if (l2PageAddr == 0) {
    l2PageAddr = getFrame();
    while (l2PageAddr == 0) {
      if (!evictOne()) {
        tlb->releaseLock();
        return 0;
      }
      l2PageAddr = getFrame();
    }
    Page *newL2 = pageAllocator->pageAtAddress(l2PageAddr);
    for (UINT32 idx = 0; idx < entriesPerTable; idx++) {
      newL2->setWordAt(0, idx);
    }
    rootPage->setWordAt(l2PageAddr, l1Idx);
  }

  Page *l2Page = pageAllocator->pageAtAddress(l2PageAddr);
  UINT32 physAddr = l2Page->wordAt(l2Idx);
  // page fault for the data page
  if (physAddr == 0) {
    physAddr = getFrame();
    while (physAddr == 0) {
      if (!evictOne()) {
        tlb->releaseLock();
        return 0;
      }
      physAddr = getFrame();
    }
    l2Page->setWordAt(physAddr, l2Idx);
  }

  tlb->releaseLock();
  return physAddr;
}

void translateAddress(UINT32 virtualAddr) {
  UINT32 translation = tlb->physicalPage(virtualAddr);
  if (translation == 0) {
    translation = pageTableWalk(virtualAddr, logPageSize);
    tlb->cacheTranslation(virtualAddr, translation);
  }
}

VOID Instruction(INS ins, VOID *v) {
  if (INS_IsMemoryRead(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)translateAddress,
                   IARG_MEMORYREAD_EA, IARG_END);
  else if (INS_IsMemoryWrite(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)translateAddress,
                   IARG_MEMORYWRITE_EA, IARG_END);
}

VOID Fini(INT32 code, VOID *v) {
  ofstream outfile;
  outfile.open(KnobOutputFile.Value().c_str());
  outfile << "Number of TLB hits: " << tlb->numHits()
          << " | Number of TLB misses: " << tlb->numMisses()
          << " | Number of TLB flushes: " << tlb->numFlushes() << std::endl;
  outfile.close();
}

int main(int argc, char *argv[]) {
  PIN_Init(argc, argv);

  logPageSize = KnobLogPageSize.Value();
  pageAllocator =
      new PageAllocator(KnobLogPageSize.Value(), KnobLogPoolSize.Value());
  tlb = new LruTLB(KnobLogNumRows.Value(), KnobAssociativity.Value());
  rootPage = pageAllocator->pageAtAddress(pageAllocator->requestPage());

  INS_AddInstrumentFunction(Instruction, 0);

  PIN_AddFiniFunction(Fini, 0);

  PIN_StartProgram();

  return 0;
}

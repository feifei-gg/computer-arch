#include "pin.H"
#include <assert.h>
#include <fstream>
#include <iostream>
#include <math.h>
#include <stdio.h>

using namespace std;

// set to 4k by default
UINT32 logPageSize;
UINT32 logPhysicalMemSize;

// Function to obtain physical page number given a virtual page number
UINT64 getPhysicalPageNumber(UINT64 virtualPageNumber) {
  INT32 key = (INT32)virtualPageNumber;
  key = ~key + (key << 15); // key = (key << 15) - key - 1;
  key = key ^ (key >> 12);
  key = key + (key << 2);
  key = key ^ (key >> 4);
  key = key * 2057; // key = (key + (key << 3)) + (key << 11);
  key = key ^ (key >> 16);
  return (UINT32)(key & (((UINT32)(~0)) >> (32 - logPhysicalMemSize)));
}

class CacheModel {
protected:
  UINT32 logNumRows;
  UINT32 logBlockSize;
  UINT32 associativity;
  UINT64 readReqs;
  UINT64 writeReqs;
  UINT64 readHits;
  UINT64 writeHits;
  UINT32 **tag;
  bool **validBit;

public:
  // Constructor for a cache
  CacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam,
             UINT32 associativityParam) {
    logNumRows = logNumRowsParam;
    logBlockSize = logBlockSizeParam;
    associativity = associativityParam;
    readReqs = 0;
    writeReqs = 0;
    readHits = 0;
    writeHits = 0;
    tag = new UINT32 *[1u << logNumRows];
    validBit = new bool *[1u << logNumRows];
    for (UINT32 i = 0; i < 1u << logNumRows; i++) {
      tag[i] = new UINT32[associativity];
      validBit[i] = new bool[associativity];
      for (UINT32 j = 0; j < associativity; j++)
        validBit[i][j] = false;
    }
  }

  // Call this function to update the cache state whenever data is read
  virtual void readReq(UINT32 virtualAddr) = 0;

  // Call this function to update the cache state whenever data is written
  virtual void writeReq(UINT32 virtualAddr) = 0;

  // Do not modify this function
  void dumpResults(ofstream *outfile) {
    *outfile << readReqs << "," << writeReqs << "," << readHits << ","
             << writeHits << "\n";
  }
};

CacheModel *cachePP;
CacheModel *cacheVP;
CacheModel *cacheVV;

// PIPT
class LruPhysIndexPhysTagCacheModel : public CacheModel {
private:
  // array to record the last time access timestamp
  UINT64 **lruStamp;
  // global timestamp, every access will be added 1
  UINT64 timeStamp;

  void access(UINT32 index, UINT32 tagVal, bool isWrite) {
    if (isWrite)
      writeReqs++;
    else
      readReqs++;

    timeStamp++;
    UINT32 hitWay = associativity;
    UINT32 emptyWay = associativity;
    // which way
    UINT32 lruWay = 0;
    // set the the max
    UINT64 minStamp = (UINT64)-1;

    for (UINT32 i = 0; i < associativity; i++) {
      if (validBit[index][i]) {
        if (tag[index][i] == tagVal) {
          hitWay = i;
          break;
        }
        if (lruStamp[index][i] < minStamp) {
          minStamp = lruStamp[index][i];
          lruWay = i;
        }
      } else {
        emptyWay = i;
      }
    }

    if (hitWay != associativity) {
      if (isWrite)
        writeHits++;
      else
        readHits++;
      lruStamp[index][hitWay] = timeStamp;
    } else {
      UINT32 replaceWay = (emptyWay != associativity) ? emptyWay : lruWay;
      validBit[index][replaceWay] = true;
      tag[index][replaceWay] = tagVal;
      lruStamp[index][replaceWay] = timeStamp;
    }
  }

public:
  LruPhysIndexPhysTagCacheModel(UINT32 logNumRowsParam,
                                UINT32 logBlockSizeParam,
                                UINT32 associativityParam)
      : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam) {
    timeStamp = 0;
    // 2^10 rows and 4 way
    lruStamp = new UINT64 *[1u << logNumRows];
    for (UINT32 i = 0; i < (1u << logNumRows); i++) {
      lruStamp[i] = new UINT64[associativity];
      for (UINT32 j = 0; j < associativity; j++)
        lruStamp[i][j] = 0;
    }
  }

  void readReq(UINT32 virtualAddr) {
    UINT64 vpn = virtualAddr >> logPageSize;
    UINT64 offset = virtualAddr & ((1u << logPageSize) - 1);
    UINT64 ppn = getPhysicalPageNumber(vpn);
    UINT64 physicalAddr = (ppn << logPageSize) | offset;

    UINT32 index = (physicalAddr >> logBlockSize) & ((1u << logNumRows) - 1);
    UINT32 tagVal = (UINT32)(physicalAddr >> (logBlockSize + logNumRows));
    access(index, tagVal, false);
  }

  void writeReq(UINT32 virtualAddr) {
    UINT64 vpn = virtualAddr >> logPageSize;
    UINT64 offset = virtualAddr & ((1u << logPageSize) - 1);
    UINT64 ppn = getPhysicalPageNumber(vpn);
    UINT64 physicalAddr = (ppn << logPageSize) | offset;

    UINT32 index = (physicalAddr >> logBlockSize) & ((1u << logNumRows) - 1);
    UINT32 tagVal = (UINT32)(physicalAddr >> (logBlockSize + logNumRows));
    access(index, tagVal, true);
  }
};

// VIPT
class LruVirIndexPhysTagCacheModel : public CacheModel {
private:
  UINT64 **lruStamp;
  UINT64 timeStamp;

  void access(UINT32 index, UINT32 tagVal, bool isWrite) {
    if (isWrite)
      writeReqs++;
    else
      readReqs++;

    timeStamp++;
    UINT32 hitWay = associativity;
    UINT32 emptyWay = associativity;
    UINT32 lruWay = 0;
    UINT64 minStamp = (UINT64)-1;

    for (UINT32 i = 0; i < associativity; i++) {
      if (validBit[index][i]) {
        if (tag[index][i] == tagVal) {
          hitWay = i;
          break;
        }
        if (lruStamp[index][i] < minStamp) {
          minStamp = lruStamp[index][i];
          lruWay = i;
        }
      } else {
        emptyWay = i;
      }
    }

    if (hitWay != associativity) {
      if (isWrite)
        writeHits++;
      else
        readHits++;
      lruStamp[index][hitWay] = timeStamp;
    } else {
      UINT32 replaceWay = (emptyWay != associativity) ? emptyWay : lruWay;
      validBit[index][replaceWay] = true;
      tag[index][replaceWay] = tagVal;
      lruStamp[index][replaceWay] = timeStamp;
    }
  }

public:
  LruVirIndexPhysTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam,
                               UINT32 associativityParam)
      : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam) {
    timeStamp = 0;
    lruStamp = new UINT64 *[1u << logNumRows];
    for (UINT32 i = 0; i < 1u << logNumRows; i++) {
      lruStamp[i] = new UINT64[associativity];
      for (UINT32 j = 0; j < associativity; j++)
        lruStamp[i][j] = 0;
    }
  }

  void readReq(UINT32 virtualAddr) {
    UINT32 index = (virtualAddr >> logBlockSize) & ((1u << logNumRows) - 1);

    UINT64 vpn = virtualAddr >> logPageSize;
    UINT64 offset = virtualAddr & ((1u << logPageSize) - 1);
    UINT64 ppn = getPhysicalPageNumber(vpn);
    UINT64 physicalAddr = (ppn << logPageSize) | offset;
    UINT32 tagVal = (UINT32)(physicalAddr >> (logBlockSize + logNumRows));

    access(index, tagVal, false);
  }

  void writeReq(UINT32 virtualAddr) {
    UINT32 index = (virtualAddr >> logBlockSize) & ((1u << logNumRows) - 1);

    UINT64 vpn = virtualAddr >> logPageSize;
    UINT64 offset = virtualAddr & ((1u << logPageSize) - 1);
    UINT64 ppn = getPhysicalPageNumber(vpn);
    UINT64 physicalAddr = (ppn << logPageSize) | offset;
    UINT32 tagVal = (UINT32)(physicalAddr >> (logBlockSize + logNumRows));

    access(index, tagVal, true);
  }
};

// VIVT
class LruVirIndexVirTagCacheModel : public CacheModel {
private:
  UINT64 **lruStamp;
  UINT64 timeStamp;

  void access(UINT32 index, UINT32 tagVal, bool isWrite) {
    if (isWrite)
      writeReqs++;
    else
      readReqs++;

    timeStamp++;
    UINT32 hitWay = associativity;
    UINT32 emptyWay = associativity;
    UINT32 lruWay = 0;
    UINT64 minStamp = (UINT64)-1;

    for (UINT32 i = 0; i < associativity; i++) {
      if (validBit[index][i]) {
        if (tag[index][i] == tagVal) {
          hitWay = i;
          break;
        }
        if (lruStamp[index][i] < minStamp) {
          minStamp = lruStamp[index][i];
          lruWay = i;
        }
      } else {
        emptyWay = i;
      }
    }

    if (hitWay != associativity) {
      if (isWrite)
        writeHits++;
      else
        readHits++;
      lruStamp[index][hitWay] = timeStamp;
    } else {
      UINT32 replaceWay = (emptyWay != associativity) ? emptyWay : lruWay;
      validBit[index][replaceWay] = true;
      tag[index][replaceWay] = tagVal;
      lruStamp[index][replaceWay] = timeStamp;
    }
  }

public:
  LruVirIndexVirTagCacheModel(UINT32 logNumRowsParam, UINT32 logBlockSizeParam,
                              UINT32 associativityParam)
      : CacheModel(logNumRowsParam, logBlockSizeParam, associativityParam) {
    timeStamp = 0;
    lruStamp = new UINT64 *[1u << logNumRows];
    for (UINT32 i = 0; i < 1u << logNumRows; i++) {
      lruStamp[i] = new UINT64[associativity];
      for (UINT32 j = 0; j < associativity; j++)
        lruStamp[i][j] = 0;
    }
  }

  void readReq(UINT32 virtualAddr) {
    UINT32 index = (virtualAddr >> logBlockSize) & ((1u << logNumRows) - 1);
    UINT32 tagVal = virtualAddr >> (logBlockSize + logNumRows);
    access(index, tagVal, false);
  }

  void writeReq(UINT32 virtualAddr) {
    UINT32 index = (virtualAddr >> logBlockSize) & ((1u << logNumRows) - 1);
    UINT32 tagVal = virtualAddr >> (logBlockSize + logNumRows);
    access(index, tagVal, true);
  }
};

// Cache analysis routine
void cacheLoad(UINT32 virtualAddr) {
  // Here the virtual address is aligned to a word boundary
  virtualAddr = (virtualAddr >> 2) << 2;
  cachePP->readReq(virtualAddr);
  cacheVP->readReq(virtualAddr);
  cacheVV->readReq(virtualAddr);
}

// Cache analysis routine
void cacheStore(UINT32 virtualAddr) {
  // Here the virtual address is aligned to a word boundary
  virtualAddr = (virtualAddr >> 2) << 2;
  cachePP->writeReq(virtualAddr);
  cacheVP->writeReq(virtualAddr);
  cacheVV->writeReq(virtualAddr);
}

// This knob will set the outfile name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "results.out",
                            "specify optional output file name");

// This knob will set the param logPhysicalMemSize
KNOB<UINT32>
    KnobLogPhysicalMemSize(KNOB_MODE_WRITEONCE, "pintool", "m", "16",
                           "specify the log of physical memory size in bytes");

// This knob will set the param logPageSize
KNOB<UINT32> KnobLogPageSize(KNOB_MODE_WRITEONCE, "pintool", "p", "12",
                             "specify the log of page size in bytes");

// This knob will set the cache param logNumRows
KNOB<UINT32> KnobLogNumRows(KNOB_MODE_WRITEONCE, "pintool", "r", "10",
                            "specify the log of number of rows in the cache");

// This knob will set the cache param logBlockSize
KNOB<UINT32>
    KnobLogBlockSize(KNOB_MODE_WRITEONCE, "pintool", "b", "5",
                     "specify the log of block size of the cache in bytes");

// This knob will set the cache param associativity
KNOB<UINT32> KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool", "a", "2",
                               "specify the associativity of the cache");

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID *v) {
  if (INS_IsMemoryRead(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cacheLoad, IARG_MEMORYREAD_EA,
                   IARG_END);
  if (INS_IsMemoryWrite(ins))
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)cacheStore, IARG_MEMORYWRITE_EA,
                   IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v) {
  ofstream outfile;
  outfile.open(KnobOutputFile.Value().c_str());
  outfile.setf(ios::showbase);
  outfile << "physical index physical tag: ";
  cachePP->dumpResults(&outfile);
  outfile << "virtual index physical tag: ";
  cacheVP->dumpResults(&outfile);
  outfile << "virtual index virtual tag: ";
  cacheVV->dumpResults(&outfile);
  outfile.close();
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char *argv[]) {
  // Initialize pin
  PIN_Init(argc, argv);

  logPageSize = KnobLogPageSize.Value();
  logPhysicalMemSize = KnobLogPhysicalMemSize.Value();

  cachePP = new LruPhysIndexPhysTagCacheModel(KnobLogNumRows.Value(),
                                              KnobLogBlockSize.Value(),
                                              KnobAssociativity.Value());
  cacheVP = new LruVirIndexPhysTagCacheModel(KnobLogNumRows.Value(),
                                             KnobLogBlockSize.Value(),
                                             KnobAssociativity.Value());
  cacheVV = new LruVirIndexVirTagCacheModel(KnobLogNumRows.Value(),
                                            KnobLogBlockSize.Value(),
                                            KnobAssociativity.Value());

  // Register Instruction to be called to instrument instructions
  INS_AddInstrumentFunction(Instruction, 0);

  // Register Fini to be called when the application exits
  PIN_AddFiniFunction(Fini, 0);

  // Start the program, never returns
  PIN_StartProgram();

  return 0;
}

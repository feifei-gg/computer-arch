#include "pin.H"
#include <assert.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_set>
#include <vector>

using namespace std;

ofstream OutFile;

// The array storing the spacing frequency between two dependant instructions
UINT64 *dependancySpacing = nullptr;

// Output file name
INT32 maxSize = 0;

// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.csv",
                            "specify the output file name");

// This knob will set the maximum spacing between two dependant instructions in
// the program
KNOB<string> KnobMaxSpacing(KNOB_MODE_WRITEONCE, "pintool", "s", "100",
                            "specify the maximum spacing between two dependant "
                            "instructions in the program");
// -------------------- Per-instruction metadata --------------------
struct InsInfo {
  vector<REG> rregs;
  vector<REG> wregs;
};

// -------------------- Per-thread state --------------------
struct ThreadState {
  UINT64 icount;
  vector<UINT64> lastWrite;
  vector<UINT64> hist;

  ThreadState(INT32 n)
      : icount(0), lastWrite((size_t)REG_LAST + 1, 0), hist((size_t)n, 0) {}
};

static ThreadState *dependencyStates[PIN_MAX_THREADS];
static PIN_LOCK dependencyMergeLock;

static VOID updateSpacingInfo(THREADID tid, const InsInfo *info) {
  ThreadState *ts = dependencyStates[tid];
  if (!ts)
    return;

  const UINT64 cur = ++(ts->icount);

  for (REG r : info->rregs) {
    if (r == REG_INVALID())
      continue;

    const UINT64 lw = ts->lastWrite[(size_t)r];
    if (cur > lw) {
      const UINT64 dist = cur - lw;

      if (dist < (UINT64)maxSize) {
        ts->hist[(size_t)(dist)]++;
      }
    }
  }

  // Update last write times for written regs
  for (REG w : info->wregs) {
    if (w == REG_INVALID())
      continue;
    ts->lastWrite[(size_t)w] = cur;
  }
}

static VOID Instruction(INS ins, VOID *v) {
  // avoid the instruction operate on same registers.
  unordered_set<REG> rset;
  unordered_set<REG> wset;

  const UINT32 nr = INS_MaxNumRRegs(ins);
  for (UINT32 i = 0; i < nr; i++) {
    REG r = INS_RegR(ins, i);
    if (r == REG_INVALID())
      continue;
    // get full register
    r = REG_FullRegName(r);
    if (r != REG_INVALID())
      rset.insert(r);
  }

  const UINT32 nw = INS_MaxNumWRegs(ins);
  for (UINT32 i = 0; i < nw; i++) {
    REG w = INS_RegW(ins, i);
    if (w == REG_INVALID())
      continue;
    w = REG_FullRegName(w);
    if (w != REG_INVALID())
      wset.insert(w);
  }

  if (rset.empty() && wset.empty())
    return;

  InsInfo *info = new InsInfo();
  info->rregs.reserve(rset.size());
  info->wregs.reserve(wset.size());
  for (const auto &r : rset)
    info->rregs.push_back(r);
  for (const auto &w : wset)
    info->wregs.push_back(w);

  INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)updateSpacingInfo, IARG_THREAD_ID,
                 IARG_PTR, info, IARG_END);
}

static VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v) {
  dependencyStates[tid] = new ThreadState(maxSize);
}

static VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v) {
  ThreadState *ts = dependencyStates[tid];
  if (!ts)
    return;

  PIN_GetLock(&dependencyMergeLock, tid + 1);
  for (INT32 i = 0; i < maxSize; i++) {
    dependancySpacing[i] += ts->hist[(size_t)i];
  }
  PIN_ReleaseLock(&dependencyMergeLock);

  delete ts;
  dependencyStates[tid] = nullptr;
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID *v) {
  // Write to a file since cout and cerr maybe closed by the application
  OutFile.open(KnobOutputFile.Value().c_str());
  OutFile.setf(ios::showbase);
  for (INT32 i = 0; i < maxSize; i++)
    OutFile << dependancySpacing[i] << ",";
  OutFile.close();
}

// argc, argv are the entire command line, including pin -t <toolname> -- ...
int main(int argc, char *argv[]) {
  PIN_Init(argc, argv);

  maxSize = atoi(KnobMaxSpacing.Value().c_str());
  if (maxSize <= 0) {
    fprintf(stderr, "Error: -s must be > 0\n");
    return 1;
  }

  dependancySpacing = new UINT64[maxSize];
  for (INT32 i = 0; i < maxSize; i++) {
    dependancySpacing[i] = 0;
  }

  for (int i = 0; i < (int)PIN_MAX_THREADS; i++) {
    dependencyStates[i] = nullptr;
  }

  PIN_InitLock(&dependencyMergeLock);

  PIN_AddThreadStartFunction(ThreadStart, 0);
  PIN_AddThreadFiniFunction(ThreadFini, 0);

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);

  PIN_StartProgram();
  return 0;
}

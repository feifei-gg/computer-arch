#include "pin.H"
#include <assert.h>
#include <fstream>
#include <iostream>
#include <stdio.h>

using namespace std;

static UINT64 takenCorrect = 0;
static UINT64 takenIncorrect = 0;
static UINT64 notTakenCorrect = 0;
static UINT64 notTakenIncorrect = 0;

class myBranchPredictor {
public:
  myBranchPredictor() {
    // Most of the time, the subsequent instruction in x86 is located in the
    // bytes immediately following the current instruction address (sequentia )
    // so initialize the status as weakly not taken 0x55 (01010101)
    for (UINT32 i = 0; i < GSHARE_BYTES; i++) {
      gshare[i] = 0x55;
    }
    for (UINT32 i = 0; i < BIMODAL_BYTES; i++) {
      bimodal[i] = 0x55;
    }
    for (UINT32 i = 0; i < CHOICE_BYTES; i++) {
      choice[i] = 0x55;
    }
    // clear the global history
    ghr = 0;
  }

  inline BOOL makePrediction(ADDRINT address) {
    // the address is 4 byte aligned
    UINT32 pc = (UINT32)(address >> 2);
    // UINT32 pc = (UINT32)(address);

    // pc % BIMODAL_ENTRIES get slot
    UINT32 bim_idx = pc & (BIMODAL_ENTRIES - 1);
    // '10' and '11' is the jump greater than 2
    BOOL p_bim = get_counter(bimodal, bim_idx) >= 2;

    // xor to resolve the aliasing, event the history same but the xor will not
    // same
    UINT32 gsh_idx = (pc ^ ghr) & (GSHARE_ENTRIES - 1);
    BOOL p_gsh = get_counter(gshare, gsh_idx) >= 2;

    // choice?
    UINT32 ch_idx = (pc ^ ghr) & (CHOICE_ENTRIES - 1);
    UINT8 ch_ctr = get_counter(choice, ch_idx);

    // if 10 or 11 then the global hash else the local
    return (ch_ctr >= 2) ? p_gsh : p_bim;
  }

  inline void makeUpdate(BOOL takenActually, BOOL takenPredicted,
                         ADDRINT address) {
    (void)takenPredicted;
    UINT32 pc = (UINT32)(address >> 2);

    UINT32 bim_idx = pc & (BIMODAL_ENTRIES - 1);
    UINT32 gsh_idx = (pc ^ ghr) & (GSHARE_ENTRIES - 1);
    UINT32 ch_idx = (pc ^ ghr) & (CHOICE_ENTRIES - 1);

    UINT8 bim_ctr = get_counter(bimodal, bim_idx);
    UINT8 gsh_ctr = get_counter(gshare, gsh_idx);
    UINT8 ch_ctr = get_counter(choice, ch_idx);

    BOOL p_bim = bim_ctr >= 2;
    BOOL p_gsh = gsh_ctr >= 2;

    BOOL bim_correct = (p_bim == takenActually);
    BOOL gsh_correct = (p_gsh == takenActually);

    // 00 01 is the local
    // 10 11 is the global
    if (gsh_correct && !bim_correct) {
      // global is right( >= 2) and local is wrong
      // the 0b11 is 3
      if (ch_ctr < 3) {
        set_counter(choice, ch_idx, ch_ctr + 1);
      }
    } else if (bim_correct && !gsh_correct) {
      // local is right and global is wrong
      // the 0b00 is 0
      if (ch_ctr > 0) {
        set_counter(choice, ch_idx, ch_ctr - 1);
      }
    }
    // others all right or all wrong don't change the choice

    if (takenActually) {
      if (bim_ctr < 3)
        set_counter(bimodal, bim_idx, bim_ctr + 1);
      if (gsh_ctr < 3)
        set_counter(gshare, gsh_idx, gsh_ctr + 1);
    } else {
      if (bim_ctr > 0)
        set_counter(bimodal, bim_idx, bim_ctr - 1);
      if (gsh_ctr > 0)
        set_counter(gshare, gsh_idx, gsh_ctr - 1);
    }

    // update the global history
    ghr = ((ghr << 1) | (takenActually ? 1 : 0)) & GHR_MASK;
  }

private:
  // entry numbers for global share
  static const UINT32 GSHARE_ENTRIES = 8192;
  // entry numbers for local
  static const UINT32 BIMODAL_ENTRIES = 4096;
  // the final choice for local or global
  static const UINT32 CHOICE_ENTRIES = 4096;

  // one byte can contain 4 2-bits counter
  static const UINT32 GSHARE_BYTES = GSHARE_ENTRIES / 4;
  static const UINT32 BIMODAL_BYTES = BIMODAL_ENTRIES / 4;
  static const UINT32 CHOICE_BYTES = CHOICE_ENTRIES / 4;

  // 13 bits for glocal history bits
  static const UINT32 GHR_BITS = 13;
  // all 111111111
  static const UINT32 GHR_MASK = (1u << GHR_BITS) - 1;

  UINT8 gshare[GSHARE_BYTES];
  UINT8 bimodal[BIMODAL_BYTES];
  UINT8 choice[CHOICE_BYTES];
  // global history record
  UINT32 ghr;

  inline UINT8 get_counter(const UINT8 *arr, UINT32 idx) const {
    UINT32 byteIdx = idx >> 2;
    // shift = idx % 4
    UINT32 shift = (idx & 3u) << 1;
    return (arr[byteIdx] >> shift) & 0x3;
  }

  inline void set_counter(UINT8 *arr, UINT32 idx, UINT8 val) {
    UINT32 byteIdx = idx >> 2;
    // shift = idx % 4
    UINT32 shift = (idx & 3u) << 1;
    UINT8 mask = (UINT8)(0x3u << shift);
    arr[byteIdx] = (UINT8)((arr[byteIdx] & ~mask) | ((val & 0x3u) << shift));
  }
};

myBranchPredictor *BP;

// This knob sets the output file name
KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "result.out",
                            "specify the output file name");

void handleBranch(ADDRINT ip, BOOL direction) {
  BOOL prediction = BP->makePrediction(ip);
  BP->makeUpdate(direction, prediction, ip);

  if (prediction) {
    if (direction) {
      takenCorrect++;
    } else {
      takenIncorrect++;
    }
  } else {
    if (direction) {
      notTakenIncorrect++;
    } else {
      notTakenCorrect++;
    }
  }
}

void instrumentBranch(INS ins, void *v) {
  if (INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)handleBranch, IARG_INST_PTR,
                   IARG_BRANCH_TAKEN, IARG_END);
  }
}

// void instrumentBranch(INS ins, void * v)
// {
//   if(INS_IsBranch(ins) && INS_HasFallThrough(ins)) {
//     INS_InsertCall(
//       ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)handleBranch,
//       IARG_INST_PTR,
//       IARG_BOOL,
//       TRUE,
//       IARG_END);
//
//     INS_InsertCall(
//       ins, IPOINT_AFTER, (AFUNPTR)handleBranch,
//       IARG_INST_PTR,
//       IARG_BOOL,
//       FALSE,
//       IARG_END);
//   }
// }

/* ===================================================================== */
VOID Fini(int, VOID *v) {
  ofstream outfile;
  outfile.open(KnobOutputFile.Value().c_str());
  outfile.setf(ios::showbase);
  outfile << "takenCorrect: " << takenCorrect
          << "  takenIncorrect: " << takenIncorrect
          << " notTakenCorrect: " << notTakenCorrect
          << " notTakenIncorrect: " << notTakenIncorrect << "\n";
  outfile.close();
}

int main(int argc, char *argv[]) {
  // Make a new branch predictor
  BP = new myBranchPredictor();

  PIN_Init(argc, argv);
  INS_AddInstrumentFunction(instrumentBranch, 0);
  PIN_AddFiniFunction(Fini, 0);
  PIN_StartProgram();

  return 0;
}

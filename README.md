# CPU Task Scheduler Simulator (C++ / Windows)

## What is in this folder
| File | What it is |
|------|-----------|
| `scheduler.h` | The brain. All 6 algorithms + metric calculations. No UI. |
| `main.cpp` | The face. The window, buttons, tables, Gantt chart. |
| `build.bat` | Double-click to compile. |
| `TaskScheduler.exe` | Ready-made program (built with MinGW). Just double-click it. |

## How to run (easiest way first)
1. **Just double-click `TaskScheduler.exe`.** If Windows SmartScreen warns, click More info -> Run anyway.
2. **To compile it yourself** (needed if you must show the code works):
   1. Install a C++ compiler: download **Code::Blocks (the "mingw-setup" version)** from codeblocks.org, or **w64devkit**.
   2. Open Command Prompt in this folder. If `g++` is not found, add the compiler's `bin` folder to PATH, e.g.
      `set PATH=%PATH%;C:\Program Files\CodeBlocks\MinGW\bin`
   3. Run `build.bat`  (or type: `g++ -std=c++11 main.cpp -o TaskScheduler.exe -mwindows -lcomctl32 -lgdi32 -static`)
3. **Visual Studio instead:** New Empty Project -> add `main.cpp` and `scheduler.h` -> Project Properties ->
   Linker -> System -> SubSystem = **Windows** -> Build and run.

## How to use the program
1. Click **Load Sample** (5 ready processes) OR type Name, Arrival, Burst, Priority and click **Add Process**.
2. Pick an algorithm in the dropdown, or keep **RUN ALL and COMPARE**.
3. Set **Time Quantum** (only used by Round Robin).
4. Click **RUN**. You get the Gantt chart, per-process table, and summary/comparison.

## How the code is organised (for viva)
- **scheduler.h** = logic only. **main.cpp** = window only. This is "separation of concerns".
- `Process` stores input; `Slice` is one Gantt block; `Result` stores everything for one algorithm.
- `runSimple()` handles FCFS, SJF, SRTF, Priority(NP), Priority(P): the clock ticks 1 unit at a time and
  each tick asks "who is the best ready process?". Only the definition of "best" changes.
- `runRR()` uses a queue. A process runs for min(quantum, remaining) and goes to the back if unfinished.
- IDLE blocks appear when nobody has arrived yet.

## Formulas
- TAT = CT - AT      - WT = TAT - BT      - RT = first CPU start - AT
- CPU utilisation = busy time / total time x 100
- Throughput = processes / total time
- Context switch = CPU moves directly from one process to a *different* process (idle gaps not counted).

## Likely viva questions
- **Why only one process at a time?** Single core.
- **FCFS drawback?** Convoy effect: short jobs wait behind a long one.
- **Why is SJF/SRTF optimal for waiting time?** Short jobs go first so fewer processes wait long. Drawback: starvation of long jobs.
- **SJF vs SRTF?** SJF never interrupts; SRTF interrupts when a shorter remaining job arrives.
- **What is starvation? Fix?** A process waits forever. Fix: aging (slowly raise priority of waiting processes).
- **Round Robin quantum too big / too small?** Too big = behaves like FCFS. Too small = too many context switches.
- **Best algorithm?** None always. It depends on the workload and the metric.
- **What is a Gantt chart?** A timeline of which process has the CPU and when.
- **Preemptive vs non-preemptive?** Whether the OS can take the CPU away from a running process.
- **Tie-breaking rule used?** Same key -> earlier arrival -> earlier entered.
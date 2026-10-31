// ============================================================
//  scheduler.h  -  THE "BRAIN" OF THE PROJECT
//  This file has NO window / UI code. It only does the maths:
//  it takes a list of processes and simulates a single-core CPU.
// ============================================================
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <vector>
#include <queue>
#include <algorithm>

// ---------- 1. DATA STRUCTURES (our "boxes" to store information) ----------

// One process typed in by the user.
struct Process {
    std::string name;   // e.g. "P1"
    int arrival;        // AT : time when process arrives
    int burst;          // BT : total CPU time it needs
    int priority;       // smaller number = higher priority
};

// One block in the Gantt chart: "process pid used CPU from start to end".
// pid = -1 means the CPU was IDLE (nobody to run).
struct Slice {
    int pid;
    int start;
    int end;
};

// Final numbers for ONE process.
struct ProcessResult {
    int pid;   // index of the process in the process list
    int ct;    // Completion Time
    int tat;   // Turnaround Time = CT - AT
    int wt;    // Waiting Time    = TAT - BT
    int rt;    // Response Time   = first CPU start - AT
};

// Everything produced by running ONE algorithm.
struct Result {
    std::string algoName;
    std::vector<Slice> gantt;           // the Gantt chart
    std::vector<ProcessResult> rows;    // one row per process
    double avgCT, avgTAT, avgWT, avgRT; // averages
    double cpuUtil;                     // CPU utilisation in %
    double throughput;                  // processes finished per time unit
    int contextSwitches;                // how many times CPU moved from one process to another
    int totalTime;                      // time when the last process finished
};

// Names for the algorithms (numbers 0..5 - the same order as the dropdown in the UI)
enum Algo { FCFS = 0, SJF, SRTF, PRIORITY_NP, PRIORITY_P, RR };

inline std::string algoName(Algo a) {
    switch (a) {
        case FCFS:        return "FCFS";
        case SJF:         return "SJF";
        case SRTF:        return "SRTF";
        case PRIORITY_NP: return "Priority (NP)";   // NP = non-preemptive
        case PRIORITY_P:  return "Priority (P)";    // P  = preemptive
        default:          return "Round Robin";
    }
}

// ---------- 2. SMALL HELPER FUNCTIONS ----------

// Add a block to the Gantt chart. If the previous block belongs to the same
// process and touches this one, just make the old block longer (merge them).
inline void addSlice(std::vector<Slice>& g, int pid, int start, int end) {
    if (!g.empty() && g.back().pid == pid && g.back().end == start)
        g.back().end = end;
    else {
        Slice s; s.pid = pid; s.start = start; s.end = end;
        g.push_back(s);
    }
}

// After the simulation: calculate CT/TAT/WT/RT, averages, utilisation, etc.
inline void finishResult(Result& r, const std::vector<Process>& ps,
                         const std::vector<int>& ct, const std::vector<int>& startT) {
    int n = (int)ps.size();
    double sCT = 0, sTAT = 0, sWT = 0, sRT = 0;
    for (int i = 0; i < n; i++) {
        ProcessResult pr;
        pr.pid = i;
        pr.ct  = ct[i];
        pr.tat = ct[i] - ps[i].arrival;          // Turnaround = CT - AT
        pr.wt  = pr.tat - ps[i].burst;           // Waiting    = TAT - BT
        pr.rt  = startT[i] - ps[i].arrival;      // Response   = first start - AT
        r.rows.push_back(pr);
        sCT += pr.ct; sTAT += pr.tat; sWT += pr.wt; sRT += pr.rt;
    }
    r.avgCT = sCT / n;  r.avgTAT = sTAT / n;
    r.avgWT = sWT / n;  r.avgRT  = sRT / n;

    // total time = end of the last Gantt block
    r.totalTime = r.gantt.empty() ? 0 : r.gantt.back().end;

    // busy time = sum of all blocks that are not IDLE
    int busy = 0;
    for (size_t i = 0; i < r.gantt.size(); i++)
        if (r.gantt[i].pid != -1) busy += r.gantt[i].end - r.gantt[i].start;

    r.cpuUtil    = (r.totalTime > 0) ? busy * 100.0 / r.totalTime : 0;
    r.throughput = (r.totalTime > 0) ? (double)n / r.totalTime : 0;

    // context switch = two neighbouring blocks, both real processes, different ones
    r.contextSwitches = 0;
    for (size_t i = 1; i < r.gantt.size(); i++)
        if (r.gantt[i].pid != -1 && r.gantt[i - 1].pid != -1 &&
            r.gantt[i].pid != r.gantt[i - 1].pid)
            r.contextSwitches++;
}

// ---------- 3. FCFS, SJF, SRTF, PRIORITY (NP and P) ----------
// These 5 are almost the same, so we use ONE function.
// The clock moves 1 time unit at a time. At each step we ask:
//   "Which ready process should get the CPU?"
// The only difference between the algorithms is HOW WE JUDGE "best":
//   FCFS        -> smallest arrival time
//   SJF         -> smallest burst time           (non-preemptive)
//   SRTF        -> smallest REMAINING time       (preemptive)
//   Priority NP -> smallest priority number      (non-preemptive)
//   Priority P  -> smallest priority number      (preemptive)
// Non-preemptive: once chosen, a process runs until it finishes.
// Preemptive    : we re-choose at every time unit.

inline int keyOf(Algo a, const Process& p, int remaining) {
    switch (a) {
        case SJF:         return p.burst;
        case SRTF:        return remaining;
        case PRIORITY_NP:
        case PRIORITY_P:  return p.priority;
        default:          return p.arrival;   // FCFS
    }
}

inline Result runSimple(const std::vector<Process>& ps, Algo algo) {
    int n = (int)ps.size();
    std::vector<int> rem(n), startT(n, -1), ct(n, 0);
    for (int i = 0; i < n; i++) rem[i] = ps[i].burst;

    bool preemptive = (algo == SRTF || algo == PRIORITY_P);
    Result r;
    r.algoName = algoName(algo);

    int time = 0;       // the clock
    int done = 0;       // how many processes have finished
    int current = -1;   // who is on the CPU now (-1 = nobody)

    while (done < n) {
        // Choose a process only if CPU is free, or if the algorithm is preemptive.
        if (current == -1 || preemptive) {
            int best = -1;
            for (int i = 0; i < n; i++) {
                if (rem[i] > 0 && ps[i].arrival <= time) {      // arrived and not finished
                    if (best == -1) best = i;
                    else {
                        int k1 = keyOf(algo, ps[i], rem[i]);
                        int k2 = keyOf(algo, ps[best], rem[best]);
                        // smaller key wins; tie -> earlier arrival wins
                        if (k1 < k2 || (k1 == k2 && ps[i].arrival < ps[best].arrival))
                            best = i;
                    }
                }
            }
            // If the running process is just as good, keep it (avoids useless switches).
            if (current != -1 && best != -1 &&
                keyOf(algo, ps[current], rem[current]) <= keyOf(algo, ps[best], rem[best]))
                best = current;
            current = best;
        }

        // Nobody is ready -> CPU is IDLE for 1 time unit.
        if (current == -1) {
            addSlice(r.gantt, -1, time, time + 1);
            time++;
            continue;
        }

        // Run the chosen process for 1 time unit.
        if (startT[current] == -1) startT[current] = time;   // first time on CPU
        addSlice(r.gantt, current, time, time + 1);
        rem[current]--;
        time++;

        if (rem[current] == 0) {          // finished!
            ct[current] = time;
            done++;
            current = -1;                 // CPU becomes free
        }
    }
    finishResult(r, ps, ct, startT);
    return r;
}

// ---------- 4. ROUND ROBIN ----------
// Every process gets at most "quantum" time units, then goes to the back of the queue.
inline Result runRR(const std::vector<Process>& ps, int quantum) {
    int n = (int)ps.size();
    std::vector<int> rem(n), startT(n, -1), ct(n, 0);
    for (int i = 0; i < n; i++) rem[i] = ps[i].burst;

    // order[] = process numbers sorted by arrival time
    std::vector<int> order(n);
    for (int i = 0; i < n; i++) order[i] = i;
    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return ps[a].arrival < ps[b].arrival; });

    Result r;
    r.algoName = algoName(RR);
    std::queue<int> ready;     // the ready queue
    int next = 0;              // next process (in order[]) that has not yet entered the queue
    int time = 0, done = 0;

    while (done < n) {
        // put every process that has arrived into the queue
        while (next < n && ps[order[next]].arrival <= time) ready.push(order[next++]);

        if (ready.empty()) {   // nobody ready -> jump the clock to the next arrival (IDLE)
            int t = ps[order[next]].arrival;
            addSlice(r.gantt, -1, time, t);
            time = t;
            continue;
        }

        int p = ready.front(); ready.pop();
        if (startT[p] == -1) startT[p] = time;

        int run = std::min(quantum, rem[p]);       // run for quantum or until finished
        addSlice(r.gantt, p, time, time + run);
        time += run;
        rem[p] -= run;

        // processes that arrived while p was running go in the queue BEFORE p
        while (next < n && ps[order[next]].arrival <= time) ready.push(order[next++]);

        if (rem[p] > 0) ready.push(p);             // not finished -> back of the queue
        else { ct[p] = time; done++; }             // finished
    }
    finishResult(r, ps, ct, startT);
    return r;
}

// ---------- 5. FUNCTIONS THE UI CALLS ----------

inline Result runAlgorithm(const std::vector<Process>& ps, Algo a, int quantum) {
    if (a == RR) return runRR(ps, quantum);
    return runSimple(ps, a);
}

// Run all 6 algorithms on the same processes (for the comparison).
inline std::vector<Result> runAll(const std::vector<Process>& ps, int quantum) {
    std::vector<Result> all;
    for (int a = 0; a <= 5; a++) all.push_back(runAlgorithm(ps, (Algo)a, quantum));
    return all;
}

// Which algorithm(s) have the lowest average?  which: 0 = waiting, 1 = turnaround, 2 = response
inline std::string findBest(const std::vector<Result>& all, int which) {
    double best = 1e18;
    for (size_t i = 0; i < all.size(); i++) {
        double v = (which == 0) ? all[i].avgWT : (which == 1) ? all[i].avgTAT : all[i].avgRT;
        if (v < best) best = v;
    }
    std::string names;
    for (size_t i = 0; i < all.size(); i++) {
        double v = (which == 0) ? all[i].avgWT : (which == 1) ? all[i].avgTAT : all[i].avgRT;
        if (v < best + 1e-9) {                 // equal to best (ties are allowed)
            if (!names.empty()) names += ", ";
            names += all[i].algoName;
        }
    }
    return names;
}

#endif
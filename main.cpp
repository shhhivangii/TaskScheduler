// ============================================================
//  main.cpp  -  THE "FACE" OF THE PROJECT (the window you see)
//  Uses the Windows API to make buttons, text boxes, tables and
//  to draw the Gantt chart. All scheduling maths is in scheduler.h
// ============================================================

// We use the plain "text" (ANSI) version of Windows functions to keep things simple.
#ifdef UNICODE
#undef UNICODE
#endif
#ifdef _UNICODE
#undef _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>      // for the table control (ListView)
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include "scheduler.h"

// (Only for Visual Studio: link the libraries automatically)
#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

using namespace std;

// ---------- IDs: every button / box gets a number so we can recognise it ----------
enum {
    ID_NAME = 101, ID_ARRIVAL, ID_BURST, ID_PRIORITY,
    ID_ADD, ID_REMOVE, ID_CLEAR, ID_SAMPLE,
    ID_ALGO, ID_QUANTUM, ID_RUN,
    ID_PROCLIST, ID_RESULTLIST, ID_SUMMARY
};

// ---------- GLOBAL VARIABLES (shared by all functions) ----------
HINSTANCE g_hInst;
HWND hName, hArrival, hBurst, hPriority;      // the 4 input boxes
HWND hAlgo, hQuantum;                         // dropdown + quantum box
HWND hProcList, hResultList, hSummary, hGantt;// tables, summary text, Gantt panel

vector<Process> g_procs;     // the processes the user has added
vector<Result>  g_results;   // the results of the last Run (1 or 6 algorithms)

// Colours for the Gantt chart bars (process number decides the colour)
COLORREF palette[10] = {
    RGB(255,153,153), RGB(153,204,255), RGB(153,230,153), RGB(255,214,153), RGB(204,170,255),
    RGB(255,238,153), RGB(153,230,230), RGB(255,170,210), RGB(200,200,150), RGB(180,210,255)
};

// ============================================================
//  PART A : small helper functions
// ============================================================

// Show a warning pop-up.
void warn(HWND h, const string& msg) {
    MessageBoxA(h, msg.c_str(), "Input Error", MB_OK | MB_ICONWARNING);
}

// Read the text from a text box (and remove spaces at both ends).
string getText(HWND h) {
    char buf[256];
    GetWindowTextA(h, buf, 256);
    string s = buf;
    while (!s.empty() && isspace((unsigned char)s.back()))  s.pop_back();
    while (!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
    return s;
}

// Convert text to an int. Returns false if it is not a proper whole number.
bool parseInt(const string& s, int& out) {
    if (s.empty() || s.size() > 7) return false;     // empty or too long
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i = 1;
    if (i == s.size()) return false;                  // only a sign
    for (size_t k = i; k < s.size(); k++)
        if (!isdigit((unsigned char)s[k])) return false;   // letters, dots, etc.
    out = atoi(s.c_str());
    return true;
}

// ---- helpers to create controls (so WM_CREATE stays short) ----
HWND makeLabel(HWND parent, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(0, "STATIC", text, WS_CHILD | WS_VISIBLE,
                           x, y, w, h, parent, NULL, g_hInst, NULL);
}
HWND makeEdit(HWND parent, int id, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", text,
                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND makeButton(HWND parent, int id, const char* text, int x, int y, int w, int h) {
    return CreateWindowExA(0, "BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
}
HWND makeList(HWND parent, int id, int x, int y, int w, int h) {
    HWND lv = CreateWindowExA(WS_EX_CLIENTEDGE, "SysListView32", "",
                              WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                              x, y, w, h, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    return lv;
}

// Add one column to a table.
void addColumn(HWND lv, int index, const char* title, int width) {
    LVCOLUMN col;
    ZeroMemory(&col, sizeof(col));
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPSTR)title;
    col.cx = width;
    ListView_InsertColumn(lv, index, &col);
}

// Add one row (a list of texts) to a table.
void addRow(HWND lv, int row, const vector<string>& cells) {
    LVITEM item;
    ZeroMemory(&item, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = row;
    item.iSubItem = 0;
    item.pszText = (LPSTR)cells[0].c_str();
    ListView_InsertItem(lv, &item);
    for (size_t c = 1; c < cells.size(); c++)
        ListView_SetItemText(lv, row, (int)c, (LPSTR)cells[c].c_str());
}

string num(int x) { return to_string(x); }

// Give every control the normal Windows font.
BOOL CALLBACK setFontProc(HWND h, LPARAM) {
    SendMessage(h, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return TRUE;
}

// ============================================================
//  PART B : showing data on the screen
// ============================================================

// Refill the "processes entered" table from g_procs.
void refreshProcList() {
    ListView_DeleteAllItems(hProcList);
    for (size_t i = 0; i < g_procs.size(); i++) {
        vector<string> cells;
        cells.push_back(g_procs[i].name);
        cells.push_back(num(g_procs[i].arrival));
        cells.push_back(num(g_procs[i].burst));
        cells.push_back(num(g_procs[i].priority));
        addRow(hProcList, (int)i, cells);
    }
}

// Clear old results (needed whenever the process list changes).
void clearResults() {
    g_results.clear();
    ListView_DeleteAllItems(hResultList);
    SetWindowTextA(hSummary, "");
    InvalidateRect(hGantt, NULL, TRUE);     // ask Windows to redraw the Gantt panel
}

// Fill the results table: one row per (algorithm, process).
void fillResultTable() {
    ListView_DeleteAllItems(hResultList);
    int row = 0;
    for (size_t k = 0; k < g_results.size(); k++) {
        const Result& r = g_results[k];
        for (size_t i = 0; i < r.rows.size(); i++) {
            const ProcessResult& pr = r.rows[i];
            const Process& p = g_procs[pr.pid];
            vector<string> cells;
            cells.push_back(r.algoName);
            cells.push_back(p.name);
            cells.push_back(num(p.arrival));
            cells.push_back(num(p.burst));
            cells.push_back(num(p.priority));
            cells.push_back(num(pr.ct));
            cells.push_back(num(pr.tat));
            cells.push_back(num(pr.wt));
            cells.push_back(num(pr.rt));
            addRow(hResultList, row++, cells);
        }
    }
}

// Build the summary text shown in the box at the bottom right.
void fillSummary() {
    ostringstream out;
    out << fixed << setprecision(2);

    if (g_results.size() == 1) {                 // ----- ONE algorithm -----
        const Result& r = g_results[0];
        out << "Algorithm: " << r.algoName << "\r\n\r\n";
        out << "Average Completion Time : " << r.avgCT  << "\r\n";
        out << "Average Turnaround Time : " << r.avgTAT << "\r\n";
        out << "Average Waiting Time    : " << r.avgWT  << "\r\n";
        out << "Average Response Time   : " << r.avgRT  << "\r\n";
        out << "CPU Utilisation         : " << r.cpuUtil << " %\r\n";
        out << "Throughput              : " << r.throughput << " process/unit\r\n";
        out << "Context Switches        : " << r.contextSwitches << "\r\n";
        out << "Total Time              : " << r.totalTime << "\r\n";
    } else {                                     // ----- ALL algorithms: comparison table -----
        out << left << setw(14) << "Algorithm" << right
            << setw(6) << "CT" << setw(6) << "TAT" << setw(6) << "WT" << setw(6) << "RT"
            << setw(8) << "CPU%" << setw(7) << "Thru" << setw(4) << "CS" << "\r\n";
        out << string(57, '-') << "\r\n";
        for (size_t k = 0; k < g_results.size(); k++) {
            const Result& r = g_results[k];
            out << left << setw(14) << r.algoName << right
                << setw(6) << r.avgCT << setw(6) << r.avgTAT
                << setw(6) << r.avgWT << setw(6) << r.avgRT
                << setw(8) << r.cpuUtil << setw(7) << setprecision(3) << r.throughput
                << setprecision(2) << setw(4) << r.contextSwitches << "\r\n";
        }
        out << "\r\n(CT,TAT,WT,RT = averages; CS = context switches)\r\n\r\n";
        out << "Lowest avg Waiting Time    : " << findBest(g_results, 0) << "\r\n";
        out << "Lowest avg Turnaround Time : " << findBest(g_results, 1) << "\r\n";
        out << "Lowest avg Response Time   : " << findBest(g_results, 2) << "\r\n\r\n";
        out << "RECOMMENDED (fastest overall, by\r\nlowest avg Turnaround): "
            << findBest(g_results, 1) << "\r\n";
    }
    SetWindowTextA(hSummary, out.str().c_str());
}

// ============================================================
//  PART C : drawing the Gantt chart  (we paint it ourselves)
// ============================================================
const int ROW_H = 40;     // height of one Gantt chart row

void drawGantt(HWND hwnd, HDC hdc) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));       // white background
    SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);

    if (g_results.empty()) {
        const char* msg = "Gantt chart will appear here. Add processes, choose an algorithm and click RUN.";
        TextOutA(hdc, 10, 10, msg, (int)strlen(msg));
        return;
    }

    // Find the longest total time so every row uses the same scale.
    int maxT = 1;
    for (size_t k = 0; k < g_results.size(); k++)
        if (g_results[k].totalTime > maxT) maxT = g_results[k].totalTime;

    int left = 120;                                  // space on the left for the algorithm name
    int usable = rc.right - left - 15;
    double scale = (double)usable / maxT;            // pixels per time unit
    if (scale > 40) scale = 40;

    for (size_t k = 0; k < g_results.size(); k++) {
        const Result& r = g_results[k];
        int y = 6 + (int)k * ROW_H;

        TextOutA(hdc, 8, y + 4, r.algoName.c_str(), (int)r.algoName.size());

        int lastLabelRight = -100;                   // stops time numbers from overlapping
        for (size_t s = 0; s < r.gantt.size(); s++) {
            const Slice& sl = r.gantt[s];
            int x1 = left + (int)(sl.start * scale);
            int x2 = left + (int)(sl.end * scale);

            // the coloured box
            RECT bar = { x1, y, x2, y + 22 };
            HBRUSH br = CreateSolidBrush(sl.pid < 0 ? RGB(215,215,215) : palette[sl.pid % 10]);
            FillRect(hdc, &bar, br);
            DeleteObject(br);
            FrameRect(hdc, &bar, (HBRUSH)GetStockObject(BLACK_BRUSH));

            // the process name inside the box
            string label = (sl.pid < 0) ? "IDLE" : g_procs[sl.pid].name;
            DrawTextA(hdc, label.c_str(), -1, &bar,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // the start time under the box (only if there is room)
            string t = num(sl.start);
            SIZE sz;
            GetTextExtentPoint32A(hdc, t.c_str(), (int)t.size(), &sz);
            if (x1 >= lastLabelRight) {
                TextOutA(hdc, x1, y + 23, t.c_str(), (int)t.size());
                lastLabelRight = x1 + sz.cx + 4;
            }
        }
        // the very last end time
        if (!r.gantt.empty()) {
            string t = num(r.gantt.back().end);
            int x = left + (int)(r.gantt.back().end * scale);
            if (x >= lastLabelRight) TextOutA(hdc, x - 4, y + 23, t.c_str(), (int)t.size());
        }
    }
}

// The "mini window procedure" of the Gantt panel: it only reacts to WM_PAINT.
LRESULT CALLBACK GanttProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        drawGantt(hwnd, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ============================================================
//  PART D : what happens when buttons are clicked
// ============================================================

void onAdd(HWND hwnd) {
    string name = getText(hName);
    int at, bt, pr;

    if (name.empty()) { warn(hwnd, "Please enter a process name."); return; }
    for (size_t i = 0; i < g_procs.size(); i++)
        if (g_procs[i].name == name) { warn(hwnd, "This process name already exists. Use a different name."); return; }
    if (!parseInt(getText(hArrival), at) || at < 0) { warn(hwnd, "Arrival Time must be a whole number (0 or more)."); return; }
    if (!parseInt(getText(hBurst), bt) || bt <= 0)  { warn(hwnd, "Burst Time must be a whole number greater than 0."); return; }
    if (!parseInt(getText(hPriority), pr) || pr < 0){ warn(hwnd, "Priority must be a whole number (0 or more)."); return; }

    Process p;
    p.name = name; p.arrival = at; p.burst = bt; p.priority = pr;
    g_procs.push_back(p);

    refreshProcList();
    clearResults();

    // get ready for the next process
    string nextName = "P" + num((int)g_procs.size() + 1);
    SetWindowTextA(hName, nextName.c_str());
    SetWindowTextA(hArrival, "");
    SetWindowTextA(hBurst, "");
    SetWindowTextA(hPriority, "");
    SetFocus(hArrival);
}

void onRemove(HWND hwnd) {
    int idx = ListView_GetNextItem(hProcList, -1, LVNI_SELECTED);   // which row is selected?
    if (idx < 0) { warn(hwnd, "First click a row in the process table, then press Remove."); return; }
    g_procs.erase(g_procs.begin() + idx);
    refreshProcList();
    clearResults();
}

void onClear() {
    g_procs.clear();
    refreshProcList();
    clearResults();
    SetWindowTextA(hName, "P1");
}

void onSample() {
    g_procs.clear();
    Process s[5] = { {"P1",0,8,2}, {"P2",1,4,3}, {"P3",2,9,1}, {"P4",3,5,4}, {"P5",30,3,2} };
    for (int i = 0; i < 5; i++) g_procs.push_back(s[i]);
    refreshProcList();
    clearResults();
    SetWindowTextA(hName, "P6");
}

void onRun(HWND hwnd) {
    if (g_procs.empty()) { warn(hwnd, "Please add at least one process first (or click 'Load Sample')."); return; }

    int sel = (int)SendMessage(hAlgo, CB_GETCURSEL, 0, 0);   // 0..5 = one algorithm, 6 = run all
    int quantum = 2;
    if (sel == 5 || sel == 6) {                              // Round Robin needs a valid quantum
        if (!parseInt(getText(hQuantum), quantum) || quantum <= 0) {
            warn(hwnd, "Time Quantum must be a whole number greater than 0.");
            return;
        }
    }

    g_results.clear();
    if (sel == 6) g_results = runAll(g_procs, quantum);                    // compare all
    else          g_results.push_back(runAlgorithm(g_procs, (Algo)sel, quantum));

    fillResultTable();
    fillSummary();
    InvalidateRect(hGantt, NULL, TRUE);                                    // redraw Gantt chart
}

// ============================================================
//  PART E : the main window (creates everything, handles clicks)
// ============================================================
LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {   // window is being born -> create all controls
        // --- Row 1 : input boxes and buttons ---
        makeLabel(hwnd, "Process Name", 10, 8, 90, 16);
        hName = makeEdit(hwnd, ID_NAME, "P1", 10, 26, 90, 22);
        SendMessage(hName, EM_LIMITTEXT, 10, 0);

        makeLabel(hwnd, "Arrival Time", 110, 8, 80, 16);
        hArrival = makeEdit(hwnd, ID_ARRIVAL, "", 110, 26, 80, 22);

        makeLabel(hwnd, "Burst Time", 200, 8, 80, 16);
        hBurst = makeEdit(hwnd, ID_BURST, "", 200, 26, 80, 22);

        makeLabel(hwnd, "Priority (small = high)", 290, 8, 140, 16);
        hPriority = makeEdit(hwnd, ID_PRIORITY, "", 290, 26, 80, 22);

        makeButton(hwnd, ID_ADD,    "Add Process",     400, 25, 100, 24);
        makeButton(hwnd, ID_REMOVE, "Remove Selected", 510, 25, 115, 24);
        makeButton(hwnd, ID_CLEAR,  "Clear All",       635, 25, 85, 24);
        makeButton(hwnd, ID_SAMPLE, "Load Sample",     730, 25, 100, 24);

        // --- Row 2 : process table (left) and algorithm controls (right) ---
        hProcList = makeList(hwnd, ID_PROCLIST, 10, 62, 640, 150);
        addColumn(hProcList, 0, "Process",      150);
        addColumn(hProcList, 1, "Arrival Time", 150);
        addColumn(hProcList, 2, "Burst Time",   150);
        addColumn(hProcList, 3, "Priority",     150);

        makeLabel(hwnd, "Choose Algorithm:", 670, 64, 200, 16);
        hAlgo = CreateWindowExA(0, "COMBOBOX", "", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                670, 82, 220, 200, hwnd, (HMENU)(INT_PTR)ID_ALGO, g_hInst, NULL);
        const char* names[7] = { "FCFS", "SJF (non-preemptive)", "SRTF (preemptive SJF)",
                                 "Priority (non-preemptive)", "Priority (preemptive)",
                                 "Round Robin", "RUN ALL and COMPARE" };
        for (int i = 0; i < 7; i++) SendMessageA(hAlgo, CB_ADDSTRING, 0, (LPARAM)names[i]);
        SendMessage(hAlgo, CB_SETCURSEL, 6, 0);       // default: run all

        makeLabel(hwnd, "Time Quantum (Round Robin):", 670, 116, 220, 16);
        hQuantum = makeEdit(hwnd, ID_QUANTUM, "3", 670, 134, 80, 22);

        makeButton(hwnd, ID_RUN, "RUN  >>", 670, 166, 220, 40);

        makeLabel(hwnd,
            "How to use:\r\n"
            "1. Type process details and click Add Process\r\n"
            "   (or click Load Sample).\r\n"
            "2. Choose an algorithm (or RUN ALL).\r\n"
            "3. Click RUN. See Gantt chart, tables and\r\n"
            "   the summary below.\r\n\r\n"
            "Note: smaller priority number = higher priority.",
            915, 64, 235, 150);

        // --- Row 3 : Gantt chart panel ---
        hGantt = CreateWindowExA(WS_EX_CLIENTEDGE, "GanttPanel", "", WS_CHILD | WS_VISIBLE,
                                 10, 222, 1130, 250, hwnd, NULL, g_hInst, NULL);

        // --- Row 4 : result table (left) and summary (right) ---
        hResultList = makeList(hwnd, ID_RESULTLIST, 10, 480, 650, 180);
        addColumn(hResultList, 0, "Algorithm", 105);
        addColumn(hResultList, 1, "Process", 65);
        addColumn(hResultList, 2, "AT",  45);
        addColumn(hResultList, 3, "BT",  45);
        addColumn(hResultList, 4, "Prio", 45);
        addColumn(hResultList, 5, "CT",  55);
        addColumn(hResultList, 6, "TAT", 55);
        addColumn(hResultList, 7, "WT",  55);
        addColumn(hResultList, 8, "RT",  55);

        hSummary = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                   WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL,
                                   670, 480, 470, 180, hwnd, (HMENU)(INT_PTR)ID_SUMMARY, g_hInst, NULL);

        // fonts: normal font everywhere, "Consolas" (equal-width) for the summary so columns line up
        EnumChildWindows(hwnd, setFontProc, 0);
        HFONT mono = CreateFontA(15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0,
                                 FIXED_PITCH, "Consolas");
        SendMessage(hSummary, WM_SETFONT, (WPARAM)mono, TRUE);
        return 0;
    }

    case WM_COMMAND:    // a button was clicked
        switch (LOWORD(wp)) {
            case ID_ADD:    onAdd(hwnd);    break;
            case ID_REMOVE: onRemove(hwnd); break;
            case ID_CLEAR:  onClear();      break;
            case ID_SAMPLE: onSample();     break;
            case ID_RUN:    onRun(hwnd);    break;
        }
        return 0;

    case WM_DESTROY:    // window closed -> end the program
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// ============================================================
//  PART F : the program starts here
// ============================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nShow) {
    g_hInst = hInst;

    INITCOMMONCONTROLSEX icc;                       // switch on the table control
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);

    // register the Gantt panel "type of window"
    WNDCLASSA gc;
    ZeroMemory(&gc, sizeof(gc));
    gc.lpfnWndProc = GanttProc;
    gc.hInstance = hInst;
    gc.hCursor = LoadCursor(NULL, IDC_ARROW);
    gc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    gc.lpszClassName = "GanttPanel";
    RegisterClassA(&gc);

    // register the main window "type"
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = MainProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "SchedulerMain";
    RegisterClassA(&wc);

    // work out the full window size so that the inside (client area) is 1150 x 670
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT r = { 0, 0, 1150, 670 };
    AdjustWindowRect(&r, style, FALSE);

    HWND hwnd = CreateWindowExA(0, "SchedulerMain", "CPU Task Scheduler Simulator (Single Core)",
                                style, CW_USEDEFAULT, CW_USEDEFAULT,
                                r.right - r.left, r.bottom - r.top,
                                NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    // the message loop: keeps the program alive and passes clicks to MainProc
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return (int)m.wParam;
}
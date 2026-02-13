#pragma once

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <vector>
#include <algorithm>
#include <string>
#include <msclr/marshal_cppstd.h>

// Link the Windows System Libraries
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "User32.lib")

using namespace System;
using namespace System::ComponentModel;
using namespace System::Collections::Generic;
using namespace System::Windows::Forms;
using namespace System::Drawing;
using namespace System::Drawing::Drawing2D;
using namespace System::Windows::Forms::DataVisualization::Charting;

namespace SmartSystemProtector {

    // Simple container to hold info about running programs
    struct ProcessInfo {
        std::string name;
        size_t memoryUsage;
        DWORD processId;
    };

    // Helper to sort processes so the biggest ones appear at the top
    static bool SortByHeaviest(const ProcessInfo& a, const ProcessInfo& b) {
        return a.memoryUsage > b.memoryUsage;
    }

    // --- CUSTOM UI COMPONENT: Modern Toggle Switch ---
    public ref class ModernToggle : public CheckBox {
    public:
        ModernToggle() {
            this->SetStyle(ControlStyles::UserPaint | ControlStyles::AllPaintingInWmPaint, true);
            this->Size = Drawing::Size(55, 28);
            this->Cursor = Cursors::Hand;
        }
    protected:
        virtual void OnPaint(PaintEventArgs^ e) override {
            Graphics^ g = e->Graphics;
            g->SmoothingMode = SmoothingMode::AntiAlias;
            g->Clear(this->Parent->BackColor);

            // Create the rounded track path
            GraphicsPath^ path = gcnew GraphicsPath();
            int diameter = this->Height - 1;
            path->AddArc(0, 0, diameter, diameter, 90, 180);
            path->AddArc(this->Width - diameter - 1, 0, diameter, diameter, 270, 180);
            path->CloseFigure();

            // Set color based on State (On/Off)
            Color trackColor = this->Checked ? Color::FromArgb(0, 198, 255) : Color::FromArgb(60, 60, 60);
            g->FillPath(gcnew SolidBrush(trackColor), path);

            // Draw the white knob
            int knobSize = this->Height - 6;
            int xPos = this->Checked ? (this->Width - knobSize - 4) : 4;
            g->FillEllipse(Brushes::White, xPos, 3, knobSize, knobSize);
        }
    };

    public ref class MainForm : public Form {
    private:
        // --- Professional Designer Palette ---
        Color^ colorDark = Color::FromArgb(15, 15, 18);    // Deep Background
        Color^ colorPanel = Color::FromArgb(24, 24, 28);   // Sidebar
        Color^ colorCyan = Color::FromArgb(0, 198, 255);   // CPU Color
        Color^ colorMint = Color::FromArgb(0, 255, 170);   // RAM Color
        Color^ colorRed = Color::FromArgb(255, 60, 100);    // Danger Alert

        // --- Logic Variables ---
        int sustainedLoadTicks = 0; // Tracks consecutive seconds of high usage
        List<double>^ cpuHistory;
        List<double>^ ramHistory;

        // --- UI Elements ---
        System::ComponentModel::IContainer^ components;
        Label^ lblCPU; Label^ lblRAM; Label^ lblStatus;
        ModernToggle^ swProtect;
        Chart^ mainPerformanceChart;
        Timer^ monitorTimer;
        DataGridView^ gridProcesses;
        DataGridView^ gridActivityLogs;
        NotifyIcon^ trayIcon;
        System::Windows::Forms::ContextMenuStrip^ trayMenu;

    public:
        MainForm(void) {
            InitializeComponent();
            cpuHistory = gcnew List<double>();
            ramHistory = gcnew List<double>();

            // Setup the heartbeat of the application
            monitorTimer = gcnew Timer();
            monitorTimer->Interval = 1000; // Tick every 1 second
            monitorTimer->Tick += gcnew EventHandler(this, &MainForm::OnMonitoringTick);
            monitorTimer->Start();
        }

    private:
        void InitializeComponent(void) {
            this->components = gcnew System::ComponentModel::Container();
            this->Size = Drawing::Size(1150, 750);
            this->Text = L"Guardian Pro | Enterprise System Monitor";
            this->BackColor = *colorDark;
            this->FormBorderStyle = Windows::Forms::FormBorderStyle::FixedSingle;
            this->StartPosition = FormStartPosition::CenterScreen;

            // --- Sidebar Creation ---
            Panel^ sidebar = gcnew Panel();
            sidebar->Dock = DockStyle::Left; sidebar->Width = 280;
            sidebar->BackColor = *colorPanel;
            this->Controls->Add(sidebar);

            lblStatus = gcnew Label();
            lblStatus->Text = "SYSTEM SECURE";
            lblStatus->ForeColor = *colorMint;
            lblStatus->Font = gcnew Drawing::Font("Bahnschrift", 16, FontStyle::Bold);
            lblStatus->Location = Point(30, 40); lblStatus->AutoSize = true;
            sidebar->Controls->Add(lblStatus);

            lblCPU = gcnew Label(); lblCPU->Location = Point(25, 120);
            lblCPU->Font = gcnew Drawing::Font("Segoe UI Light", 32);
            lblCPU->ForeColor = *colorCyan; lblCPU->Text = "0%"; lblCPU->AutoSize = true;
            sidebar->Controls->Add(lblCPU);

            lblRAM = gcnew Label(); lblRAM->Location = Point(25, 210);
            lblRAM->Font = gcnew Drawing::Font("Segoe UI Light", 32);
            lblRAM->ForeColor = *colorMint; lblRAM->Text = "0%"; lblRAM->AutoSize = true;
            sidebar->Controls->Add(lblRAM);

            // Protection Toggle
            swProtect = gcnew ModernToggle();
            swProtect->Location = Point(30, 320); swProtect->Checked = true;
            sidebar->Controls->Add(swProtect);

            Label^ lToggle = gcnew Label(); lToggle->Text = "AUTO-PROTECTION MODE";
            lToggle->ForeColor = Color::Gray; lToggle->Font = gcnew Drawing::Font("Segoe UI", 7, FontStyle::Bold);
            lToggle->Location = Point(25, 300); lToggle->AutoSize = true;
            sidebar->Controls->Add(lToggle);

            // Process Grid
            gridProcesses = gcnew DataGridView();
            gridProcesses->Location = Point(15, 410); gridProcesses->Size = Drawing::Size(250, 180);
            gridProcesses->BackgroundColor = *colorPanel; gridProcesses->BorderStyle = BorderStyle::None;
            gridProcesses->RowHeadersVisible = false; gridProcesses->ColumnHeadersVisible = false;
            gridProcesses->AllowUserToAddRows = false; gridProcesses->ReadOnly = true;
            gridProcesses->DefaultCellStyle->BackColor = *colorPanel; gridProcesses->DefaultCellStyle->ForeColor = Color::White;
            gridProcesses->ColumnCount = 2; gridProcesses->Columns[0]->Width = 140; gridProcesses->Columns[1]->Width = 80;
            sidebar->Controls->Add(gridProcesses);

            // --- Activity Log Grid (Bottom Area) ---
            gridActivityLogs = gcnew DataGridView();
            gridActivityLogs->Location = Point(310, 580); gridActivityLogs->Size = Drawing::Size(800, 110);
            gridActivityLogs->BackgroundColor = *colorDark; gridActivityLogs->BorderStyle = BorderStyle::None;
            gridActivityLogs->RowHeadersVisible = false; gridActivityLogs->ColumnHeadersVisible = false;
            gridActivityLogs->AllowUserToAddRows = false; gridActivityLogs->ReadOnly = true;
            gridActivityLogs->DefaultCellStyle->BackColor = *colorDark; gridActivityLogs->DefaultCellStyle->ForeColor = Color::Gray;
            gridActivityLogs->ColumnCount = 3; gridActivityLogs->Columns[0]->Width = 80; gridActivityLogs->Columns[1]->Width = 180; gridActivityLogs->Columns[2]->Width = 500;
            this->Controls->Add(gridActivityLogs);

            // --- Tray Icon & Menu ---
            trayMenu = gcnew System::Windows::Forms::ContextMenuStrip(this->components);
            trayMenu->Items->Add("Open Dashboard", nullptr, gcnew EventHandler(this, &MainForm::ShowUI));
            trayMenu->Items->Add("Exit System Guardian", nullptr, gcnew EventHandler(this, &MainForm::FullExit));

            trayIcon = gcnew NotifyIcon(this->components);
            trayIcon->Icon = SystemIcons::Shield; trayIcon->Visible = true;
            trayIcon->ContextMenuStrip = trayMenu;
            trayIcon->DoubleClick += gcnew EventHandler(this, &MainForm::ShowUI);

            this->FormClosing += gcnew FormClosingEventHandler(this, &MainForm::OnHideToTray);
            SetupVisualGraph();
        }

        void SetupVisualGraph() {
            mainPerformanceChart = gcnew Chart();
            mainPerformanceChart->Location = Point(310, 50); mainPerformanceChart->Size = Drawing::Size(800, 500);
            mainPerformanceChart->BackColor = *colorDark;

            ChartArea^ area = gcnew ChartArea("MainArea"); area->BackColor = *colorDark;
            area->AxisX->MajorGrid->LineColor = Color::FromArgb(30, 30, 35);
            area->AxisY->MajorGrid->LineColor = Color::FromArgb(30, 30, 35);
            area->AxisY->LabelStyle->ForeColor = Color::DimGray;
            mainPerformanceChart->ChartAreas->Add(area);

            // Spline curves look smoother and more modern
            Series^ sCPU = gcnew Series("CPU"); sCPU->ChartType = SeriesChartType::SplineArea;
            sCPU->Color = Color::FromArgb(120, 0, 198, 255); sCPU->BorderWidth = 2; sCPU->BorderColor = *colorCyan;
            mainPerformanceChart->Series->Add(sCPU);

            Series^ sRAM = gcnew Series("RAM"); sRAM->ChartType = SeriesChartType::SplineArea;
            sRAM->Color = Color::FromArgb(80, 0, 255, 170); sRAM->BorderWidth = 2; sRAM->BorderColor = *colorMint;
            mainPerformanceChart->Series->Add(sRAM);

            this->Controls->Add(mainPerformanceChart);
        }

        // --- THE ENGINE LOOP ---
        void OnMonitoringTick(Object^ sender, EventArgs^ e) {
            double currentCpu = GetCpuUsage();
            double usedR, totalR; double currentRam = GetRamUsage(usedR, totalR);

            lblCPU->Text = currentCpu.ToString("F0") + "%";
            lblRAM->Text = currentRam.ToString("F0") + "%";

            // --- SMART SUSTAINED LOAD LOGIC ---
            if (currentCpu > 85.0 || currentRam > 90.0) {
                sustainedLoadTicks++;

                if (sustainedLoadTicks >= 3) {
                    // Trigger protection only after 3 consecutive seconds of high load
                    lblStatus->Text = "CRITICAL LOAD DETECTED"; lblStatus->ForeColor = *colorRed;
                    if (swProtect->Checked) TriggerAutoProtect();
                }
                else {
                    lblStatus->Text = "MONITORING SPIKE (" + sustainedLoadTicks + "s)";
                    lblStatus->ForeColor = Color::Orange;
                }
            }
            else {
                sustainedLoadTicks = 0; // Reset if the spike ends
                lblStatus->Text = "SYSTEM SECURE"; lblStatus->ForeColor = *colorMint;
            }

            // Update Graphing History
            if (cpuHistory->Count >= 60) { cpuHistory->RemoveAt(0); ramHistory->RemoveAt(0); }
            cpuHistory->Add(currentCpu); ramHistory->Add(currentRam);
            mainPerformanceChart->Series["CPU"]->Points->DataBindY(cpuHistory);
            mainPerformanceChart->Series["RAM"]->Points->DataBindY(ramHistory);

            RefreshProcessDisplay();
        }

        void TriggerAutoProtect() {
            // Lower priority for top 3 resource hogs
            std::vector<ProcessInfo> procs = GetSortedProcessList();
            for (int i = 0; i < 3 && i < (int)procs.size(); i++) {
                HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION, FALSE, procs[i].processId);
                if (hProc) {
                    SetPriorityClass(hProc, BELOW_NORMAL_PRIORITY_CLASS);
                    AddActivityLog(gcnew String(procs[i].name.c_str()), "Sustained load: Priority reduced to Below Normal");
                    CloseHandle(hProc);
                }
            }
            System::Media::SystemSounds::Beep->Play();
        }

        void AddActivityLog(String^ app, String^ message) {
            gridActivityLogs->Rows->Insert(0, DateTime::Now.ToString("HH:mm:ss"), app, message);
            if (gridActivityLogs->Rows->Count > 8) gridActivityLogs->Rows->RemoveAt(8); // Keep it clean
        }

        std::vector<ProcessInfo> GetSortedProcessList() {
            std::vector<ProcessInfo> list;
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);

            if (Process32First(hSnap, &pe)) {
                do {
                    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
                    if (h) {
                        PROCESS_MEMORY_COUNTERS m;
                        if (GetProcessMemoryInfo(h, &m, sizeof(m))) {
                            ProcessInfo d; d.name = msclr::interop::marshal_as<std::string>(gcnew String(pe.szExeFile));
                            d.memoryUsage = m.WorkingSetSize / (1024 * 1024); d.processId = pe.th32ProcessID;
                            list.push_back(d);
                        }
                        CloseHandle(h);
                    }
                } while (Process32Next(hSnap, &pe));
            }
            CloseHandle(hSnap);
            std::sort(list.begin(), list.end(), SortByHeaviest);
            return list;
        }

        void RefreshProcessDisplay() {
            std::vector<ProcessInfo> list = GetSortedProcessList();
            gridProcesses->Rows->Clear();
            for (int i = 0; i < 5 && i < (int)list.size(); i++) {
                gridProcesses->Rows->Add(gcnew String(list[i].name.c_str()), list[i].memoryUsage + " MB");
            }
        }

        // --- Lifecycle Management ---
        void OnHideToTray(Object^ sender, FormClosingEventArgs^ e) {
            if (e->CloseReason == CloseReason::UserClosing) { e->Cancel = true; this->Hide(); }
        }
        void ShowUI(Object^ sender, EventArgs^ e) { this->Show(); this->WindowState = FormWindowState::Normal; }
        void FullExit(Object^ sender, EventArgs^ e) { trayIcon->Visible = false; Application::Exit(); }

        // --- CORE OS APIS (Kernel Interaction) ---
        double GetCpuUsage() {
            static FILETIME pIdle, pKernel, pUser; FILETIME idle, kernel, user;
            if (!GetSystemTimes(&idle, &kernel, &user)) return 0;

            ULONGLONG dIdle = FT2ULL(idle) - FT2ULL(pIdle);
            ULONGLONG dKernel = FT2ULL(kernel) - FT2ULL(pKernel);
            ULONGLONG dUser = FT2ULL(user) - FT2ULL(pUser);
            pIdle = idle; pKernel = kernel; pUser = user;

            ULONGLONG total = dKernel + dUser;
            return (total == 0) ? 0 : (double)(total - dIdle) * 100.0 / total;
        }

        double GetRamUsage(double& used, double& total) {
            MEMORYSTATUSEX m; m.dwLength = sizeof(m); GlobalMemoryStatusEx(&m);
            total = m.ullTotalPhys / 1048576.0; used = (m.ullTotalPhys - m.ullAvailPhys) / 1048576.0;
            return (used / total) * 100.0;
        }

        ULONGLONG FT2ULL(const FILETIME& ft) { return (((ULONGLONG)ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }
    };
}
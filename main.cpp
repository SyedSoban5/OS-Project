#include "MainForm.h"

using namespace System;
using namespace System::Windows::Forms;

// The entry point of the software
[STAThreadAttribute]
int main(array<String^>^ args)
{
    // Standard Windows Styles for a clean GUI
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);

    // Start the Guardian Pro Dashboard
    Application::Run(gcnew SmartSystemProtector::MainForm());
    return 0;
}
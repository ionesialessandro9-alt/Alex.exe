#define UNICODE
#define _UNICODE
#include <windows.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "Ole32.lib")

static const COLORREF BG = RGB(12, 12, 16);
static const COLORREF PANEL = RGB(23, 23, 30);
static const COLORREF RED = RGB(220, 38, 55);
static const COLORREF TEXT = RGB(239, 239, 244);
static const COLORREF MUTED = RGB(150, 150, 164);

HWND gMain, gContent, gStatus;
HFONT gFont = nullptr, gTitleFont = nullptr;
std::wstring gSimulatedId;
std::wstring gPage = L"HWID";

std::wstring RunPowerShell(const std::wstring& command) {
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return L"Non disponibile";
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = L"powershell.exe -NoProfile -NonInteractive -Command \"" + command + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe; si.hStdError = writePipe;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    std::string bytes;
    if (ok) {
        WaitForSingleObject(pi.hProcess, 7000);
        char buffer[2048]; DWORD n = 0;
        while (ReadFile(readPipe, buffer, sizeof(buffer), &n, nullptr) && n)
            bytes.append(buffer, n);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    CloseHandle(readPipe);
    if (!ok || bytes.empty()) return L"Non disponibile";
    int needed = MultiByteToWideChar(CP_UTF8, 0, bytes.c_str(), (int)bytes.size(), nullptr, 0);
    if (needed <= 0) needed = MultiByteToWideChar(CP_ACP, 0, bytes.c_str(), (int)bytes.size(), nullptr, 0);
    std::wstring out(needed, L'\0');
    MultiByteToWideChar(CP_ACP, 0, bytes.c_str(), (int)bytes.size(), out.data(), needed);
    while (!out.empty() && (out.back() == L'\r' || out.back() == L'\n' || out.back() == L' '))
        out.pop_back();
    return out.empty() ? L"Non disponibile" : out;
}

std::wstring NewGuid() {
    GUID id{};
    if (CoCreateGuid(&id) != S_OK) return L"Errore generazione";
    wchar_t buffer[40]{};
    StringFromGUID2(id, buffer, 40);
    return buffer;
}

void SetStatus(const std::wstring& s) {
    SetWindowTextW(gStatus, s.c_str());
}

void AddLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h,
              int size = 18, COLORREF color = TEXT, bool bold = false) {
    HWND label = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, nullptr, nullptr);
    SendMessageW(label, WM_SETFONT, (WPARAM)(bold ? gTitleFont : gFont), TRUE);
    (void)size; (void)color;
}

void AddValue(HWND parent, const std::wstring& caption, const std::wstring& value, int y) {
    HWND a = CreateWindowW(L"STATIC", caption.c_str(), WS_CHILD | WS_VISIBLE,
        32, y, 210, 24, parent, nullptr, nullptr, nullptr);
    SendMessageW(a, WM_SETFONT, (WPARAM)gFont, TRUE);
    HWND b = CreateWindowW(L"STATIC", value.c_str(), WS_CHILD | WS_VISIBLE,
        250, y, 600, 38, parent, nullptr, nullptr, nullptr);
    SendMessageW(b, WM_SETFONT, (WPARAM)gFont, TRUE);
}

void AddButton(HWND parent, const wchar_t* text, int id, int x, int y, int w = 190) {
    HWND b = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
        x, y, w, 42, parent, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(b, WM_SETFONT, (WPARAM)gFont, TRUE);
}

void ClearContent() {
    HWND child = GetWindow(gContent, GW_CHILD);
    while (child) {
        HWND next = GetWindow(child, GW_HWNDNEXT);
        DestroyWindow(child);
        child = next;
    }
}

void ShowPage(const std::wstring& page) {
    gPage = page;
    ClearContent();
    if (page == L"HWID") {
        AddValue(gContent, L"BIOS SERIAL", RunPowerShell(L"(Get-CimInstance Win32_BIOS).SerialNumber"), 35);
        AddValue(gContent, L"CPU", RunPowerShell(L"(Get-CimInstance Win32_Processor | Select-Object -First 1).Name"), 95);
        AddValue(gContent, L"MOTHERBOARD", RunPowerShell(L"(Get-CimInstance Win32_BaseBoard).Product"), 155);
        AddValue(gContent, L"SMBIOS UUID", RunPowerShell(L"(Get-CimInstance Win32_ComputerSystemProduct).UUID"), 215);
        AddLabel(gContent, L"LETTURA SOLA — nessun identificativo viene modificato.", 32, 290, 760, 30, 14, MUTED);
    } else if (page == L"SECURITY") {
        AddLabel(gContent, L"SECURITY CHECK", 32, 35, 500, 36, 20, TEXT, true);
        AddValue(gContent, L"WINDOWS", RunPowerShell(L"(Get-CimInstance Win32_OperatingSystem).Caption"), 95);
        AddValue(gContent, L"SECURE BOOT", RunPowerShell(L"try { Confirm-SecureBootUEFI } catch { 'Non disponibile' }"), 155);
        AddValue(gContent, L"TPM", RunPowerShell(L"(Get-Tpm).TpmPresent"), 215);
        AddLabel(gContent, L"Controlli informativi: nessuna impostazione di sicurezza viene cambiata.", 32, 290, 780, 30, 14, MUTED);
    } else if (page == L"SPOOFER") {
        AddLabel(gContent, L"SIMULATORE HWID", 32, 35, 500, 36, 20, TEXT, true);
        AddLabel(gContent, L"Genera un identificativo virtuale locale per test dell'interfaccia.", 32, 85, 760, 30, 15, MUTED);
        HWND value = CreateWindowW(L"STATIC", gSimulatedId.empty() ? L"Nessun ID simulato" : gSimulatedId.c_str(),
            WS_CHILD | WS_VISIBLE, 32, 145, 760, 38, gContent, nullptr, nullptr, nullptr);
        SendMessageW(value, WM_SETFONT, (WPARAM)gTitleFont, TRUE);
        AddButton(gContent, L"GENERA ID VIRTUALE", 201, 32, 215, 220);
        AddButton(gContent, L"SALVA BACKUP", 202, 270, 215, 180);
        AddButton(gContent, L"CARICA BACKUP", 203, 466, 215, 180);
        AddLabel(gContent, L"Nota: l'ID resta nel simulatore e non altera BIOS, CPU, disco, Windows o anti-cheat.", 32, 285, 800, 45, 14, MUTED);
    } else if (page == L"CLEANER") {
        AddLabel(gContent, L"CLEANER", 32, 35, 500, 36, 20, TEXT, true);
        AddLabel(gContent, L"Analizza la cartella temporanea dell'utente senza cancellare file.", 32, 90, 760, 30, 15, MUTED);
        AddButton(gContent, L"ANALIZZA FILE TEMP", 301, 32, 150, 220);
        AddLabel(gContent, L"Per sicurezza questa versione non elimina automaticamente dati.", 32, 220, 760, 30, 14, MUTED);
    } else {
        AddLabel(gContent, L"ALEX", 32, 35, 500, 50, 30, RED, true);
        AddLabel(gContent, L"Hardware information & virtual testing lab", 32, 100, 760, 30, 16, MUTED);
        AddLabel(gContent, L"Build didattica per Windows 11", 32, 145, 760, 30, 16, TEXT);
    }
}

void SaveId() {
    if (gSimulatedId.empty()) { SetStatus(L"Genera prima un ID virtuale."); return; }
    OPENFILENAMEW ofn{}; wchar_t path[MAX_PATH] = L"alex_test_backup.txt";
    ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"File di testo\0*.txt\0Tutti i file\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (GetSaveFileNameW(&ofn)) {
        HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD written; WriteFile(f, gSimulatedId.c_str(), (DWORD)(gSimulatedId.size() * sizeof(wchar_t)), &written, nullptr);
            CloseHandle(f); SetStatus(L"Backup dell'ID virtuale salvato.");
        }
    }
}

void LoadId() {
    OPENFILENAMEW ofn{}; wchar_t path[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn); ofn.lpstrFile = path; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"File di testo\0*.txt\0Tutti i file\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            wchar_t buf[128]{}; DWORD read = 0;
            ReadFile(f, buf, sizeof(buf) - sizeof(wchar_t), &read, nullptr); CloseHandle(f);
            gSimulatedId = buf; SetStatus(L"ID virtuale caricato. Apri di nuovo la pagina Spoofer per visualizzarlo.");
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        gFont = CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        gTitleFont = CreateFontW(25,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        CreateWindowW(L"STATIC", L"ALEX", WS_CHILD|WS_VISIBLE, 28, 20, 200, 42, hwnd, nullptr, nullptr, nullptr);
        HWND logo = GetWindow(hwnd, GW_CHILD); SendMessageW(logo, WM_SETFONT, (WPARAM)gTitleFont, TRUE);
        CreateWindowW(L"STATIC", L"HARDWARE LAB  /  WINDOWS 11", WS_CHILD|WS_VISIBLE, 30, 62, 250, 24, hwnd, nullptr, nullptr, nullptr);
        const wchar_t* names[] = { L"HWID", L"SECURITY", L"SPOOFER", L"CLEANER", L"ABOUT" };
        for (int i=0;i<5;i++) {
            HWND b = CreateWindowW(L"BUTTON", names[i], WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
                24, 115+i*58, 165, 44, hwnd, (HMENU)(INT_PTR)(100+i), nullptr, nullptr);
            SendMessageW(b, WM_SETFONT, (WPARAM)gFont, TRUE);
        }
        gContent = CreateWindowW(L"STATIC", L"", WS_CHILD|WS_VISIBLE, 210, 105, 850, 490, hwnd, nullptr, nullptr, nullptr);
        gStatus = CreateWindowW(L"STATIC", L"READY  |  TEST MODE — READ ONLY", WS_CHILD|WS_VISIBLE, 24, 570, 900, 25, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(gStatus, WM_SETFONT, (WPARAM)gFont, TRUE);
        ShowPage(L"HWID"); return 0;
    }
    case WM_DRAWITEM: {
        auto dis = (DRAWITEMSTRUCT*)lp;
        int id = (int)(INT_PTR)dis->CtlID;
        HBRUSH brush = CreateSolidBrush((id >= 100 && id <= 104) ? (gPage == (id==100?L"HWID":id==101?L"SECURITY":id==102?L"SPOOFER":id==103?L"CLEANER":L"ABOUT") ? RED : PANEL) : RED);
        FillRect(dis->hDC, &dis->rcItem, brush); DeleteObject(brush);
        SetBkMode(dis->hDC, TRANSPARENT); SetTextColor(dis->hDC, TEXT);
        wchar_t text[128]; GetWindowTextW(dis->hwndItem, text, 128);
        DrawTextW(dis->hDC, text, -1, (RECT*)&dis->rcItem, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id >= 100 && id <= 104) {
            const wchar_t* p[] = {L"HWID",L"SECURITY",L"SPOOFER",L"CLEANER",L"ABOUT"};
            ShowPage(p[id-100]); return 0;
        }
        if (id == 201) { gSimulatedId = NewGuid(); ShowPage(L"SPOOFER"); SetStatus(L"Nuovo ID virtuale generato."); return 0; }
        if (id == 202) { SaveId(); return 0; }
        if (id == 203) { LoadId(); return 0; }
        if (id == 301) {
            wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
            std::wstring cmd = L"(Get-ChildItem -LiteralPath '" + std::wstring(temp) + L"' -File -Recurse -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum";
            SetStatus(L"Dimensione temporanei (byte): " + RunPowerShell(cmd)); return 0;
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp; SetBkColor(dc, (COLORREF)BG); SetTextColor(dc, TEXT);
        static HBRUSH b = CreateSolidBrush(BG); return (LRESULT)b;
    }
    case WM_ERASEBKGND: {
        RECT r; GetClientRect(hwnd, &r); HBRUSH b = CreateSolidBrush(BG); FillRect((HDC)wp, &r, b); DeleteObject(b); return 1;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = hInst;
    wc.lpszClassName = L"ALEX_HWID_LAB"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(BG); RegisterClassW(&wc);
    gMain = CreateWindowW(wc.lpszClassName, L"ALEX | Hardware Lab",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
        1100, 680, nullptr, nullptr, hInst, nullptr);
    ShowWindow(gMain, nCmdShow);
    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    CoUninitialize(); return 0;
}

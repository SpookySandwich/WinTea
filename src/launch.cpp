#include "launch.h"

#include <windows.h>
#include <shellapi.h>
#include <process.h>

#include <string>
#include <vector>

#include "app.h"

namespace wintea {
namespace {

std::wstring expand(const std::wstring& s) {
    if (s.empty()) return s;
    wchar_t buf[2048];
    DWORD n = ExpandEnvironmentStringsW(s.c_str(), buf, 2048);
    if (n == 0 || n > 2048) return s;
    return std::wstring(buf, n - 1);
}

bool fileOnPath(const wchar_t* name, std::wstring& resolved) {
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr);
    if (n == 0 || n >= MAX_PATH) return false;
    resolved.assign(buf, n);
    return true;
}

void postBalloon(HWND hwnd, DWORD flags, std::wstring title, std::wstring text,
                 bool openConfig = false) {
    if (!hwnd) return;
    auto* b = new Balloon{ flags, std::move(title), std::move(text), openConfig };
    PostMessageW(hwnd, WM_APP_NOTIFY, static_cast<WPARAM>(flags),
                 reinterpret_cast<LPARAM>(b));
}

// One fully resolved launch attempt.
struct Attempt {
    std::wstring file;
    std::wstring args;
    std::wstring workdir;
    bool         isWt;
};

// What the worker thread receives (owns; frees itself).
struct Job {
    bool                 admin;
    bool                 autoMode;
    std::vector<Attempt> attempts; // tried in order until one starts
    HWND                 notifyHwnd;
    bool                 notifications;
};

// Returns: 1 = started ok, 0 = failed (try next), -1 = user declined UAC (stop).
int runAttempt(const Attempt& a, bool admin) {
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = admin ? L"runas" : L"open";
    sei.lpFile = a.file.c_str();
    sei.lpParameters = a.args.empty() ? nullptr : a.args.c_str();
    sei.lpDirectory  = a.workdir.empty() ? nullptr : a.workdir.c_str();
    sei.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&sei)) {
        return (GetLastError() == ERROR_CANCELLED) ? -1 : 0;
    }

    // wt.exe hands off to WindowsTerminal.exe and exits ~immediately with 0. A
    // *nonzero* quick exit means the alias is broken (see KB5050021 regression).
    if (a.isWt && sei.hProcess) {
        if (WaitForSingleObject(sei.hProcess, 1500) == WAIT_OBJECT_0) {
            DWORD code = 0;
            if (GetExitCodeProcess(sei.hProcess, &code) && code != 0) {
                CloseHandle(sei.hProcess);
                return 0; // treat as failure so the chain falls through
            }
        }
    }
    if (sei.hProcess) CloseHandle(sei.hProcess);
    return 1;
}

unsigned __stdcall worker(void* arg) {
    Job* job = static_cast<Job*>(arg);
    bool declined = false;

    for (const Attempt& a : job->attempts) {
        int r = runAttempt(a, job->admin);
        if (r == 1) { delete job; return 0; }
        if (r == -1) { declined = true; break; } // user said no to UAC: do not fall back
    }

    if (!declined && job->notifications) {
        if (job->autoMode) {
            postBalloon(job->notifyHwnd, NIIF_ERROR, L"WinTea",
                        L"Couldn't start a terminal (wt, pwsh, powershell, cmd all failed). "
                        L"Click to open config and set [launch] command.", true);
        } else {
            postBalloon(job->notifyHwnd, NIIF_ERROR, L"WinTea",
                        L"Your [launch] command failed to start. Click to check config.", true);
        }
    }
    delete job;
    return 0;
}

} // namespace

void LaunchTerminal(const Config& config, bool admin, HWND notifyHwnd) {
    auto* job = new Job{};
    job->admin = admin;
    job->notifyHwnd = notifyHwnd;
    job->notifications = config.notifications;

    std::wstring workdir = expand(config.workdir);

    if (!config.command.empty()) {
        // Override mode: exactly one attempt, user's command/args verbatim.
        job->autoMode = false;
        std::wstring file = expand(config.command);
        std::wstring resolved;
        if (fileOnPath(file.c_str(), resolved)) file = resolved;
        job->attempts.push_back({ file, expand(config.args), workdir, false });
    } else {
        // Auto mode: first terminal that exists on PATH wins.
        job->autoMode = true;
        struct Cand { const wchar_t* name; bool isWt; };
        const Cand cands[] = {
            { L"wt.exe", true }, { L"pwsh.exe", false },
            { L"powershell.exe", false }, { L"cmd.exe", false },
        };
        for (const Cand& c : cands) {
            std::wstring resolved;
            if (!fileOnPath(c.name, resolved)) continue;
            std::wstring args;
            if (c.isWt && !workdir.empty()) {
                // wt ignores lpDirectory when elevated; -d sets the starting dir.
                args = L"-d \"" + workdir + L"\"";
            }
            job->attempts.push_back({ resolved, args, workdir, c.isWt });
        }
        if (job->attempts.empty()) {
            // Nothing resolved at all (very unusual); report immediately.
            if (config.notifications)
                postBalloon(notifyHwnd, NIIF_ERROR, L"WinTea",
                            L"No terminal found on PATH. Click to set [launch] command.", true);
            delete job;
            return;
        }
    }

    uintptr_t t = _beginthreadex(nullptr, 0, worker, job, 0, nullptr);
    if (t == 0) {
        delete job; // thread didn't start; nothing will free it otherwise
        return;
    }
    CloseHandle(reinterpret_cast<HANDLE>(t));
}

} // namespace wintea

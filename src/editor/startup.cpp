#include "editor_app.hpp"
#include "startup.hpp"
#include "engine/log.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <io.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
namespace editor {
namespace {
const char* crash_file="engine_editor.crash.log";
// A process-lifetime duplicate of stderr's OS handle avoids reopening a file
// whose CRT sharing mode may deny the fatal writer (notably MSVC).
HANDLE crash_sink=INVALID_HANDLE_VALUE;
void write_crash(const char* message) noexcept {
    // No engine/CRT logger locks, allocation or stdio in a fatal handler.
    const bool temporary=crash_sink==INVALID_HANDLE_VALUE;
    const auto file=temporary?CreateFileA(crash_file,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr):crash_sink;
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written{};DWORD length{};while(length<4096 && message[length])++length;
    OVERLAPPED append{};append.Offset=MAXDWORD;append.OffsetHigh=MAXDWORD;
    WriteFile(file,message,length,&written,&append);FlushFileBuffers(file);
    if(temporary)CloseHandle(file);
}
LONG WINAPI fatal_exception(EXCEPTION_POINTERS* info) {
    char text[256]{};
    std::snprintf(text,sizeof(text),"Fatal Windows exception 0x%08lX at %p, thread %lu\r\n",
        info->ExceptionRecord->ExceptionCode,info->ExceptionRecord->ExceptionAddress,GetCurrentThreadId());
    write_crash(text);return EXCEPTION_EXECUTE_HANDLER;
}
void abort_signal(int) {write_crash("Fatal SIGABRT (assertion or abort)\r\n");std::_Exit(3);}
#if defined(_MSC_VER) && defined(_DEBUG)
int report_hook(int,char* message,int*) {write_crash(message);return FALSE;}
#endif
}
void configure_startup_diagnostics(bool designer) {
    crash_file=designer?"editor_hud.crash.log":"engine_editor.crash.log";
    // GUI applications normally have no stderr. Preserve assertion/CRT messages.
    FILE* sink{};
#ifdef _MSC_VER
    (void)freopen_s(&sink,crash_file,"a",stderr);
#ifdef _DEBUG
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL,report_hook);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
#else
    sink=std::freopen(crash_file,"a",stderr);
#endif
    // C assert() in a GUI subsystem otherwise opens a modal CRT message box
    // instead of reporting to stderr; report hooks alone do not intercept it.
    _set_error_mode(_OUT_TO_STDERR);
    if(sink) {
        (void)std::setvbuf(sink,nullptr,_IONBF,0);
        const int descriptor=_fileno(sink);
        if(descriptor>=0) {
            const auto native=_get_osfhandle(descriptor);
            HANDLE duplicate{};
            if(native!=-1 && native!=-2 && DuplicateHandle(GetCurrentProcess(),reinterpret_cast<HANDLE>(native),GetCurrentProcess(),&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS))
                crash_sink=duplicate;
        }
    }
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    SetUnhandledExceptionFilter(fatal_exception);
    std::signal(SIGABRT,abort_signal);
    std::set_terminate([] {
        write_crash("Unhandled C++ termination\r\n");
        if(auto error=std::current_exception())try{std::rethrow_exception(error);}catch(const std::exception& e){write_crash(e.what());write_crash("\r\n");}catch(...){}
        std::abort();
    });
}
void show_editor_window(HWND hwnd,int requested_show) {
    // A launcher/IDE can pass SW_HIDE to a GUI entry point. This is an editor,
    // not a background utility: never enter the render loop with a hidden HWND.
    ShowWindow(hwnd,requested_show==SW_HIDE?SW_SHOWNORMAL:requested_show);
    if(!IsWindowVisible(hwnd))ShowWindow(hwnd,SW_SHOW); // STARTF_USESHOWWINDOW can override the first call.
    UpdateWindow(hwnd);
    RECT client{};GetClientRect(hwnd,&client);
    LOG_INFO("startup","Window shown: visible=%d minimized=%d client=%ldx%ld show=%d",
        IsWindowVisible(hwnd)!=FALSE,IsIconic(hwnd)!=FALSE,client.right,client.bottom,requested_show);
}
}

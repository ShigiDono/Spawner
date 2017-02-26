#include "runner.hpp"

#include "inc/error.hpp"

#ifdef _MSC_VER
#pragma comment(lib, "Userenv")
#endif

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <WinBase.h>
#include <UserEnv.h>

const size_t MAX_USER_NAME = 1024;

handle_t runner::main_job_object = handle_default_value;
handle_t runner::main_job_object_access_mutex = CreateMutex(NULL, 0, NULL);
bool runner::allow_breakaway = true;

void runner::set_allow_breakaway(bool allow) {
    if (allow_breakaway == allow) {
        return;
    }
    if (main_job_object == handle_default_value) {
        main_job_object = CreateJobObject(NULL, NULL);
        AssignProcessToJobObject(main_job_object, GetCurrentProcess());
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION extended_limit_information;
    memset(&extended_limit_information, 0, sizeof(extended_limit_information));
    if (allow) {
        extended_limit_information.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
    }

    if (!SetInformationJobObject(main_job_object, JobObjectExtendedLimitInformation, &extended_limit_information, sizeof(extended_limit_information))) {
        DWORD le = GetLastError();
        return;
    }
    allow_breakaway = allow;
}

void runner::copy_environment(TCHAR* dest, const WCHAR* source) const {
    int written = 0;

    for (WCHAR* env = (WCHAR*)source; *env != '\0';)
    {
        TCHAR* ansi = w2a((const WCHAR*)env);

        strcpy(dest, ansi);

        int bytes = strlen(ansi) + 1;

        written += bytes;
        env += bytes;
        dest += bytes;
    }

    *dest = '\0';
}

runner::env_vars_list_t runner::read_environment(const WCHAR* source) const
{
    env_vars_list_t vars;

    for (WCHAR* env = (WCHAR*)source; *env != '\0';)
    {
        std::string envStr(w2a((const WCHAR*)env));

        int pos = envStr.find("=");

        vars.push_back(make_pair(envStr.substr(0, pos), envStr.substr(pos + 1)));

        env += envStr.length() + 1;
    }

    return vars;
}

runner::env_vars_list_t runner::set_environment_for_process() const
{
    auto curr_vars = read_environment(GetEnvironmentStringsW());

    if (options.environmentMode == "user-default")
    {
        LPVOID envBlock = NULL;

        CreateEnvironmentBlock(&envBlock, NULL, FALSE);

        auto default_vars = read_environment((WCHAR*)envBlock);

        DestroyEnvironmentBlock(envBlock);

        for (auto i = default_vars.cbegin(); i != default_vars.cend(); ++i)
        {
            SetEnvironmentVariableA(i->first.c_str(), i->second.c_str());
        }

        for (auto i = curr_vars.cbegin(); i != curr_vars.cend(); ++i)
        {
            if (std::find(default_vars.cbegin(), default_vars.cend(), *i) == default_vars.cend())
            {
                SetEnvironmentVariableA(i->first.c_str(), NULL);
            }
        }
    }
    else if (options.environmentMode == "clear")
    {
        for (auto i = curr_vars.cbegin(); i != curr_vars.cend(); ++i)
        {
            SetEnvironmentVariableA(i->first.c_str(), NULL);
        }
    }

    for (auto i = options.environmentVars.cbegin(); i != options.environmentVars.cend(); ++i) {
        SetEnvironmentVariableA(i->first.c_str(), i->second.c_str());
    }

    return curr_vars;
}

void runner::restore_original_environment(const runner::env_vars_list_t& original) const
{
    auto curr_vars = read_environment(GetEnvironmentStringsW());

    for (auto i = original.cbegin(); i != original.cend(); ++i)
    {
        SetEnvironmentVariableA(i->first.c_str(), i->second.c_str());
    }

    for (auto i = curr_vars.cbegin(); i != curr_vars.cend(); ++i)
    {
        if (std::find(original.cbegin(), original.cend(), *i) == original.cend())
        {
            SetEnvironmentVariableA(i->first.c_str(), NULL);
        }
    }
}

bool runner::init_process(const std::string &cmd, const char *wd) {
    WaitForSingleObject(main_job_object_access_mutex, infinite);
    set_allow_breakaway(true);

    // LPVOID penv = createEnvironmentForProcess();
    env_vars_list_t original = set_environment_for_process();
    char *cmd_copy = _strdup(cmd.c_str()); // CreateProcess requires write access to command line.

    auto create_process_helper = [&](const char *app_name) -> BOOL {
        return CreateProcess(
            app_name, cmd_copy, NULL, NULL, /* inheritHandles */TRUE,
            process_creation_flags, NULL, wd, &si, &process_info);
    };

    BOOL error =
        !create_process_helper(program.c_str()) &&
        (!options.use_cmd || !create_process_helper(NULL));
    ReleaseMutex(main_job_object_access_mutex);
    std::free(cmd_copy);
    if (error) {
        DWORD_PTR args[] = { (DWORD_PTR)program.c_str(), (DWORD_PTR)"", (DWORD_PTR)"", };
        PANIC("CreateProcess \"" + program + "\": " + get_win_last_error_string(args));
        return false;
    }
    restore_original_environment(original);
    get_times(&creation_time, NULL, NULL, NULL);
    return true;
}

bool runner::init_process_with_logon(const std::string &cmd, const char *wd) {
    WaitForSingleObject(main_job_object_access_mutex, infinite);
    set_allow_breakaway(false);

    STARTUPINFOW siw;
    //USES_CONVERSION;
    ZeroMemory(&siw, sizeof(siw));
    siw.cb = sizeof(si);
    siw.dwFlags = si.dwFlags;
    siw.hStdInput = si.hStdInput;
    siw.hStdOutput = si.hStdOutput;
    siw.hStdError = si.hStdError;
    siw.wShowWindow = si.wShowWindow;
    siw.lpDesktop = NULL;//L"";
    std::string run_program = program + " " + options.get_arguments();

    wchar_t *login = a2w(options.login.c_str());
    wchar_t *password = a2w(options.password.c_str());
    wchar_t *wprogram = a2w(run_program.c_str());
    wchar_t *wcmd = a2w(cmd.c_str());
    wchar_t *wwd = a2w(wd);

    DWORD creation_flags = CREATE_SUSPENDED | CREATE_SEPARATE_WOW_VDM | CREATE_NO_WINDOW;

    HANDLE token = NULL;

    auto original = set_environment_for_process();

    if ( !CreateProcessWithLogonW(login, NULL, password, 0,
        wprogram, wcmd, creation_flags,
        NULL, wwd, &siw, &process_info) )
    {
        if (!options.use_cmd || !CreateProcessWithLogonW(login, NULL, password, 0,
            NULL, wcmd, creation_flags,
            NULL, wwd, &siw, &process_info) )
        {
            ReleaseMutex(main_job_object_access_mutex);
            DWORD_PTR args[] = { (DWORD_PTR)program.c_str(), (DWORD_PTR)"", (DWORD_PTR)"", };
            PANIC("CreateProcess \"" + run_program + "\": " + get_win_last_error_string(args));
            // TODO: cleanup below is useless now since we're in panic
            delete[] login;
            delete[] password;
            delete[] wprogram;
            delete[] wcmd;
            delete[] wwd;
            restore_original_environment(original);

            return false;
        }
    }

    set_allow_breakaway(true);
    ReleaseMutex(main_job_object_access_mutex);
    delete[] login;
    delete[] password;
    delete[] wprogram;
    delete[] wcmd;
    delete[] wwd;
    restore_original_environment(original);

    get_times(&creation_time, NULL, NULL, NULL);

    return true;
}

static std::string quote(const std::string &cmd) {
    return cmd.find(' ') == std::string::npos ? cmd : "\"" + cmd + "\"";
}

void runner::create_process() {
    //WaitForSingleObject(init_semaphore, INFINITE);

    if (process_status == process_spawner_crash) {
        ReleaseSemaphore(init_semaphore, 10, NULL);
        return;
    }
    ZeroMemory(&si, sizeof(si));

    si.cb = sizeof(si);
    {//if (!options.delegated) {//#TODO fix this
        si.dwFlags = STARTF_USESTDHANDLES;
        if (pipes.find(STD_OUTPUT_PIPE) != pipes.end())
            si.hStdOutput = pipes[STD_OUTPUT_PIPE]->get_pipe();
        if (pipes.find(STD_ERROR_PIPE) != pipes.end())
            si.hStdError = pipes[STD_ERROR_PIPE]->get_pipe();
        if (pipes.find(STD_INPUT_PIPE) != pipes.end())
            si.hStdInput = pipes[STD_INPUT_PIPE]->get_pipe();
    }
    si.lpDesktop = "";
    process_creation_flags = PROCESS_CREATION_FLAGS;

    if (options.hide_gui)
    {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    if (options.silent_errors) {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    }
    if (options.debug) {
        process_creation_flags |= DEBUG_PROCESS;
    }

    // Extracting program name and generating cmd line
    report.working_directory = options.working_directory;
    const char *wd = (options.working_directory != "")?options.working_directory.c_str():NULL;
    if (!wd)
    {
        char working_directory[MAX_PATH + 1];
        if (GetCurrentDirectoryA(MAX_PATH, working_directory))//error here is not critical
            report.working_directory = working_directory;
    }

    size_t index_path_sep = program.find_last_of("\\/");
    std::string command_line =
        index_path_sep != std::string::npos ? program.substr(index_path_sep + 1) : program;

    if (options.string_arguments != "") {
        command_line += " " + options.string_arguments;
    }
    for (auto arg = options.arguments.begin(); arg != options.arguments.end(); ++arg) {
        command_line += " " + quote(*arg);
    }

    if (options.login != "") {
        report.login = a2w(options.login.c_str());
        running = init_process_with_logon(command_line, wd);
    }
    else {
        //IMPORTANT: if logon option selected & failed signalize it
        DWORD len = MAX_USER_NAME;
        wchar_t user_name[MAX_USER_NAME];
        if (GetUserNameW(user_name, &len)) { // Error here is not critical.
            report.login = user_name;
        }
        running = init_process(command_line, wd);
    }

    ReleaseSemaphore(init_semaphore, 10, NULL);
}

void runner::free() {
    CloseHandleSafe(process_info.hProcess);
    CloseHandleSafe(process_info.hThread);
}

void runner::wait() {
    WaitForSingleObject(process_info.hProcess, INFINITE);
}

void runner::debug() {
    DEBUG_EVENT debug_event;
    while (WaitForDebugEvent(&debug_event, INFINITE)) {
        ContinueDebugEvent(debug_event.dwProcessId,
            debug_event.dwThreadId,
            DBG_CONTINUE);
        if (debug_event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
        {
            debug_event.dwProcessId = 0;
        }
        //std::cout << debug_event.u.DebugString.lpDebugStringData << std::endl;
    }
}

void runner::requisites() {
    if (!start_suspended) {
        if (ResumeThread(process_info.hThread) == (DWORD)-1) {
            PANIC(get_win_last_error_string());
        }
        process_status = process_still_active;
    } else {
        process_status = process_suspended;
    }
    for (auto& it : pipes) {
        std::shared_ptr<pipe_c> pipe = it.second;
        pipe->bufferize();
    }
    if (options.debug) {
        debug();
    }
}

thread_return_t runner::async_body(thread_param_t param) {
    runner *self = (runner*)param;
    self->run_process();
    return 0;
}

runner::runner(const std::string &program, const options_class &options)
    : base_runner(program, options)
    , running_thread(handle_default_value)
    , init_semaphore(handle_default_value) {

    init_semaphore = CreateSemaphore(NULL, 0, 10, NULL);
    ZeroMemory(&process_info, sizeof(process_info));
}

runner::~runner() {
    CloseHandleSafe(process_info.hProcess);
    CloseHandleSafe(process_info.hThread);
}

unsigned long runner::get_exit_code() {
    if (process_status == process_spawner_crash
     || process_status == process_not_started) {
        return 0;
    }
    DWORD dwExitCode = 0;
    if (GetExitCodeProcess(process_info.hProcess, &dwExitCode) == 0) {
        PANIC(get_win_last_error_string());
    }
    return dwExitCode;
}

process_status_t runner::get_process_status_no_side_effects() {
    return process_status;
}

process_status_t runner::get_process_status() {
    //renew process status
    if (process_status & process_finished_normal
     || process_status == process_suspended
     || process_status == process_not_started) {
        return process_status;
    }
    unsigned long exitcode = get_exit_code();
    if (process_status == process_spawner_crash) {
        return process_status;
    }
    if (exitcode == exit_code_still_active) {
        process_status = process_still_active;
    } else {
        process_status = process_finished_abnormally;
    }
    if (exitcode == 0) {
        process_status = process_finished_normal;
    }
    return process_status;
}

exception_t runner::get_exception() {
    if (get_process_status() == process_finished_abnormally) {
        return (exception_t)get_exit_code();
    }
    else return exception_exception_no;
}

unsigned long runner::get_id() {
    return process_info.dwProcessId;
}

std::string runner::get_program() const {
    return program;
}

options_class runner::get_options() const {
    return options;
}

report_class runner::get_report() {
    report.application_name = get_program();

    report.exception = get_exception();
    report.exit_code = get_exit_code();
    return report;
}

unsigned long long runner::get_time_since_create() {
    unsigned long long current = runner::get_current_time();
    if (current < creation_time) {
        current = creation_time;
    }
    return current - creation_time;
}

unsigned long long runner::get_current_time() {
    SYSTEMTIME current_time_sys;
    FILETIME current_time;
    GetSystemTime(&current_time_sys);
    SystemTimeToFileTime(&current_time_sys, &current_time);

    return *reinterpret_cast<unsigned long long*>(&current_time);
}

handle_t runner::get_process_handle() {
    return process_info.hProcess;
}
void runner::get_times(unsigned long long *_creation_time, unsigned long long *exit_time, unsigned long long *kernel_time, unsigned long long *user_time) {
    FILETIME __creation_time;
    FILETIME _exit_time;
    FILETIME _kernel_time;
    FILETIME _user_time;

    GetProcessTimes(get_process_handle(), &__creation_time, &_exit_time, &_kernel_time, &_user_time);
    if (_creation_time) {
        *_creation_time = *reinterpret_cast<unsigned long long*>(&__creation_time);
    }
    if (exit_time) {
        *exit_time = *reinterpret_cast<unsigned long long*>(&_exit_time);
    }
    if (kernel_time) {
        *kernel_time = *reinterpret_cast<unsigned long long*>(&_kernel_time);
    }
    if (user_time) {
        *user_time = *reinterpret_cast<unsigned long long*>(&_user_time);
    }
}
void runner::run_process() {
    if (options.debug && !running_async) {
        run_process_async();
        WaitForSingleObject(running_thread, 100);//may stuck here
        WaitForSingleObject(init_semaphore, INFINITE);//may stuck here
        WaitForSingleObject(process_info.hProcess, INFINITE);//may stuck here
        return;
    }
    create_process();
    if (get_process_status() == process_spawner_crash
     || get_process_status() & process_finished_normal) {
        return;
    }
    running = true;
    requisites();
    if (get_process_status() == process_spawner_crash
     || get_process_status() & process_finished_normal) {
        return;
    }
    wait();
}

void runner::run_process_async() {
    running_async = true;
    running_thread = CreateThread(NULL, 0, async_body, this, 0, NULL);
}

bool runner::wait_for(const unsigned long &interval) {
    if (get_process_status() == process_spawner_crash
     || get_process_status() & process_finished_normal) {
        return true;
    }
    if (!running_async) {
        return false;
    }
    wait_for_init(interval);
    if (WaitForSingleObject(process_info.hProcess, interval) != WAIT_OBJECT_0) {
        return false;
    }
    WaitForSingleObject(running_thread, interval);
    CloseHandleSafe(running_thread);
    return true;
}

bool runner::wait_for_init(const unsigned long &interval) {
    while (init_semaphore == handle_default_value) {//not very good, made for synchro with async(mutex belongs to creator thread)
        Sleep(5);
    }
    return WaitForSingleObject(init_semaphore, interval) == WAIT_OBJECT_0;// TODO: get rid of this
}

void runner::safe_release() {
    process_status = process_spawner_crash;
    free();// make it safe!!!
}

void runner::enumerate_threads_(std::function<void(handle_t)> on_thread) {
    if (!is_running()) {
        return;
    }
    HANDLE thread_snapshot_h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (thread_snapshot_h == handle_default_value) {
        return;
    }
    THREADENTRY32 thread_entry;
    thread_entry.dwSize = sizeof(thread_entry);
    if (!Thread32First(thread_snapshot_h, &thread_entry)) {
        return;
    }
    do {
        if (thread_entry.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) +
            sizeof(thread_entry.th32OwnerProcessID) && thread_entry.th32OwnerProcessID == process_info.dwProcessId) {
            handle_t handle = OpenThread(THREAD_ALL_ACCESS, FALSE, thread_entry.th32ThreadID);
            if (on_thread) {
                on_thread(handle);
            }
            CloseHandle(handle);
        }
        thread_entry.dwSize = sizeof(thread_entry);
    } while (Thread32Next(thread_snapshot_h, &thread_entry));
    CloseHandle(thread_snapshot_h);
}

void runner::suspend()
{
    suspend_mutex_.lock();
    if (get_process_status() != process_still_active) {
        suspend_mutex_.unlock();
        return;
    }
    enumerate_threads_([](handle_t handle) {
        SuspendThread(handle);
    });
    process_status = process_suspended;
    suspend_mutex_.unlock();
}

void runner::resume()
{
    suspend_mutex_.lock();
    if (get_process_status() != process_suspended) {
        suspend_mutex_.unlock();
        return;
    }
    enumerate_threads_([](handle_t handle) {
        ResumeThread(handle);
    });
    process_status = process_still_active;
    get_process_status();
    suspend_mutex_.unlock();
}

bool runner::is_running()
{
    return running || ((get_process_status() & process_still_active) != 0);
}

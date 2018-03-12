#ifndef _WIN_RUNNER_H_
#define _WIN_RUNNER_H_

#include <string>
#include <map>
#include <memory>

#include "inc/status.h"
#include "inc/report.h"
#include "inc/options.h"
#include "inc/base_runner.h"
#include "platform.h"
#include "mutex.h"

class runner : public base_runner {
    mutex_c suspend_mutex_;

    env_vars_list_t read_environment(const WCHAR* source) const;
    env_vars_list_t set_environment_for_process() const;
    void restore_original_environment(const env_vars_list_t& original) const;

    virtual bool try_handle_createproc_error();

    bool process_is_finished();
protected:
    DWORD process_creation_flags;
    STARTUPINFO si;
    PROCESS_INFORMATION process_info;
    thread_t running_thread;
    volatile handle_t init_semaphore; //rename to mutex_init_signal
    static handle_t main_job_object;
    static handle_t main_job_object_access_mutex;
    static bool allow_breakaway;
    void set_allow_breakaway(bool allow);
    bool init_process(const std::string &cmd, const char *wd);
    bool init_process_with_logon(const std::string &cmd, const char *wd);
    virtual void create_process();
    virtual void free();
    virtual void wait();
    virtual void debug();
    virtual void requisites();
    static thread_return_t async_body(thread_param_t param);
    void enumerate_threads_(std::function<void(handle_t)> on_thread);
    void finalize_streams();
public:
    runner(const std::string &program, const options_class &options);
    virtual ~runner();
    unsigned long get_exit_code();
    virtual process_status_t get_process_status();
    terminate_reason_t get_terminate_reason();
    process_status_t get_process_status_no_side_effects();
    exception_t get_exception();
    unsigned long get_id();
    std::string get_program() const;
    options_class get_options() const;
    virtual report_class get_report();
    virtual unsigned long long get_time_since_create();
    static unsigned long long get_current_time();
    virtual handle_t get_process_handle();
    virtual void get_times(unsigned long long *_creation_time, unsigned long long *exit_time, unsigned long long *kernel_time, unsigned long long *user_time);

    virtual void run_process();
    virtual void run_process_async();
    void wait_for(const unsigned long &interval = INFINITE);
    bool wait_for_init(const unsigned long &interval);
    virtual void safe_release();
    bool start_suspended = true;
    void suspend();
    void resume();
    bool is_running();
};

#endif // _WIN_RUNNER_H

#pragma once

#include <string>
#include <vector>
#include <map>

#include "rapidjson/rapidjson.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"

#include "arguments.h"
#include "sp.h"
#include "spawner_base.h"
#include "mutex.h"

class spawner_new_c: public spawner_base_c {
protected:
    restrictions_class restrictions;
    options_class options;
    restrictions_class base_restrictions;
    options_class base_options;
    bool runas;
    bool base_initialized;
    settings_parser_c &parser;
    std::vector<runner*> runners;
    size_t order;
    bool control_mode_enabled;
    std::map<std::shared_ptr<output_buffer_c>, int> buffer_to_runner_index_;
    // std::map<int, std::shared_ptr<output_buffer_c>> runner_index_to_buffer_;
    std::shared_ptr<pipe_buffer_c> controller_buffer_;
    int controller_index_ = -1;
    mutex_c wait_normal_mutex_;
    mutex_c on_terminate_mutex_;
    std::vector<bool> awaited_normals_;
    void setup_stream_(const std::string& stream_str, pipes_t this_pipe_type, runner* this_runner);
    void process_controller_message_(const std::string& message, output_pipe_c* pipe);
    void process_normal_message_(const std::string& message, output_pipe_c* pipe, int runner_index);
    int get_normal_index_(const std::string& message);
    int normal_to_runner_index_(int normal_index);
public:
    spawner_new_c(settings_parser_c &parser);
    virtual ~spawner_new_c();

    void json_report(runner *runner_instance, rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF16<> > &writer);
    virtual bool init();
    bool init_runner();
    virtual void run();
    virtual void print_report();
    virtual std::string help();
    virtual void on_separator(const std::string &_);
    virtual void init_arguments();
};

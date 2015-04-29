#include "spawner_base.h"

output_buffer_class * spawner_base_c::create_output_buffer(const std::string &name, const pipes_t &pipe_type, const size_t buffer_size /*= 4096*/)
{
    output_buffer_class *output_buffer = NULL;
    if (name == "std") {
        unsigned int color = FOREGROUND_BLUE | FOREGROUND_GREEN;
        if (pipe_type == STD_ERROR_PIPE) {
            color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        }
        output_buffer = new output_stdout_buffer_class(4096, color);
    }
    else if (name[0] == '*') {
    }
    else if (name.length()) {
        output_buffer = new output_file_buffer_class(name, 4096);
    }
    return output_buffer;
}

input_buffer_class * spawner_base_c::create_input_buffer(const std::string &name, const size_t buffer_size /*= 4096*/)
{
    input_buffer_class *input_buffer = NULL;
    if (name == "std") {
        input_buffer = new input_stdin_buffer_class(4096);
    }
    else if (name[0] == '*') {
    }
    else if (name.length()) {
        input_buffer = new input_file_buffer_class(name, 4096);
    }
    return input_buffer;
}

spawner_base_c::spawner_base_c()
{

}

spawner_base_c::~spawner_base_c()
{

}

void spawner_base_c::begin_report()
{

}

void spawner_base_c::run(int argc, char *argv[])
{

}

void spawner_base_c::run()
{

}

std::string spawner_base_c::help()
{
    return "Usage:\n\t--legacy=<sp99|sp00|pcms2>\n";
}

void spawner_base_c::init_arguments()
{

}

bool spawner_base_c::init()
{
    return true;
}

void spawner_base_c::print_report()
{

}
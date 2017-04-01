#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include "sp.h"
#include "inc/compatibility.h"
#include "spawner_base.h"
#include "spawner_old.h"
#include "spawner_new.h"
#include "spawner_pcms2.h"

class command_handler_c {
protected:
    settings_parser_c parser;
    void add_default_parser();
    bool legacy_set;
public:
    bool show_help;
    spawner_base_c *spawner;
    command_handler_c();
    ~command_handler_c();
    void reset();
    bool parse(int argc, char *argv[]);
    spawner_base_c *create_spawner(const std::string &s);
    bool set_legacy(const std::string &s);
    void add_parser(abstract_parser_c *p);
};

void command_handler_c::add_default_parser() {
    console_argument_parser_c *console_default_parser =
        new console_argument_parser_c();
    environment_variable_parser_c *environment_default_parser =
        new environment_variable_parser_c();

    console_default_parser->add_flag_parser(c_lst(short_arg("h"), long_arg("help")),
        new boolean_argument_parser_c(show_help))
        ->set_description("Show help");

    console_default_parser->add_argument_parser(
        c_lst(long_arg("legacy")),
        new function_argument_parser_c<command_handler_c*,
            spawner_base_c*(command_handler_c::*)(const std::string&)>(
                this,
                &command_handler_c::create_spawner
            )
    )->set_description("Command-line interface: sp99, sp00 or pcms2 (default: sp00)")->default_error =
        environment_default_parser->add_argument_parser(
            c_lst("SP_LEGACY"),
            new function_argument_parser_c<command_handler_c*,
                spawner_base_c*(command_handler_c::*)(const std::string&)>(
                    this,
                    &command_handler_c::create_spawner
                )
        )->default_error = "Invalid value for legacy argument.";

    add_parser(console_default_parser);
    add_parser(environment_default_parser);
}

command_handler_c::command_handler_c()
    : spawner(NULL)
    , show_help(false)
    , legacy_set(false) {

    add_default_parser();
    if (!spawner) {
        create_spawner("sp00");
    }
}

command_handler_c::~command_handler_c() {
    if (spawner) {
        delete spawner;
    }
}

bool command_handler_c::set_legacy(const std::string &s) {
    if (legacy_set) {
        return false;
    }
    legacy_set = true;
    return create_spawner(s) != NULL;
}

void command_handler_c::reset() {
    while (parser.parsers_count() > 2) {
        parser.pop_back();
    }
    parser.set_dividers(c_lst("=").vector());
}

spawner_base_c *command_handler_c::create_spawner(const std::string &s) {
    reset();
    if (spawner) {
        delete spawner;
        spawner = nullptr;
    }
    if (s == "sp99") {
        spawner = new spawner_old_c(this->parser);
    } else if (s == "sp00") {
        spawner = new spawner_new_c(this->parser);
    } else if (s == "pcms2") {
        spawner = new spawner_pcms2_c(this->parser);
    }

    if (spawner) {
        spawner->init_arguments();
    }

    return spawner;
}

void command_handler_c::add_parser(abstract_parser_c *p) {
    parser.add_parser(p);
    if (!p->invoke_initialization(parser)) {
        parser.stop();
    }
}

bool command_handler_c::parse(int argc, char *argv[]) {
//    reset();
    if (!parser.parse(argc, argv)) {
        return false;
    }
    if (spawner) {
        if (!show_help) { show_help = !spawner->init(); }
        if (show_help) {
            std::cout << spawner->help();
        } else {
            spawner->run();
        }
    }
    return true;
}

command_handler_c* handler = nullptr;

#if defined(_WIN32)
BOOL WINAPI CtrlHandlerRoutine(DWORD dwCtrlType) {
    if (handler != nullptr) {
        set_error_text("Ctrl+C pressed");
        handler->spawner->print_report();
        // don't delete handler here since we can step into infinite loop
        // in case this handler was called while executing destructor
    }
    return FALSE;
}
#endif

int main(int argc, char *argv[]) {
    platform_init();
    handler = new command_handler_c();
    // TODO: codestyle: replace \)\r\n\{ with \) \{\r\n
    // Suppress msg window on abort; TODO: check if it's ms spec
#if defined(_MSC_VER)
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
#if defined(_WIN32)
    SetConsoleCtrlHandler(CtrlHandlerRoutine, TRUE);
#endif
    set_on_panic_action([&]() {
        if (handler != nullptr) {
            handler->spawner->print_report();
            handler = nullptr;
            // don't delete handler here either
        }
    });
    // TODO: report parse errors
    if (handler != nullptr) {
        handler->parse(argc, argv);
        delete handler;
        handler = nullptr;
    }
    return 0;
}

#ifndef _SPAWNER_RESTRICTIONS_H_
#define _SPAWNER_RESTRICTIONS_H_

#include <string>
#include <climits>
#include <limits>

#include <inc/session.h>

enum restriction_kind_t
{
    restriction_user_time_limit         = 0x0,
    restriction_memory_limit            = 0x1,
    restriction_processor_time_limit    = 0x2,
    restriction_security_limit          = 0x3,
    restriction_write_limit             = 0x4,
    restriction_load_ratio              = 0x5,
    restriction_idle_time_limit         = 0x6,
    restriction_processes_count_limit   = 0x7,
    restriction_max                     = 0x8
};

typedef uint64_t restriction_t;

const restriction_t restriction_no_limit = std::numeric_limits<uint64_t>::max();
const restriction_t restriction_limited = 1;

//TODO: move source to platform independent .cpp
//TODO: rewrite as a class
struct restrictions_class
{
    restriction_t restrictions[restriction_max];
    restrictions_class();
    void set_restriction(const restriction_kind_t &kind, const restriction_t &value);
    void set_restriction(const std::string &kind, const restriction_t &value);
    restriction_t get_restriction(const restriction_kind_t &kind) const;
    restriction_t &operator [](const restriction_kind_t &kind);
    restriction_t &operator [](int i);
    static restriction_kind_t restriction_by_name(const std::string &name);
    bool check_restriction(const restriction_kind_t &kind) const;
};

#endif //_SPAWNER_RESTRICTIONS_H_

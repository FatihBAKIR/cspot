#include "doctest.h"
#include "global.h"
#include "woofc.h"

#include <debug.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cspot {
namespace {
TEST_CASE("Cspot") {
//    system("rm -rf /tmp/tests");
    ::mkdir("/tmp/tests", 0700);
    globals::set_namespace("tests");
    globals::set_dir("/tmp/tests");
    globals::set_namelog_dir("/tmp/tests");
    globals::set_namelog_name("log");

    SUBCASE("WooFCreate works") {
        auto res = WooFCreate("foo", 16, 16);
        REQUIRE_EQ(res, 1);
        struct stat data;
        auto stat_res = stat("/tmp/tests/foo", &data);
        REQUIRE_EQ(stat_res, 0);
    }

    SUBCASE("WooFPut & WooFGet works") {
        unsigned long seq;
        SUBCASE("WooFPut works") {
            int arg = 42;
            seq = WooFPut("foo", nullptr, &arg);
            DEBUG_LOG("Sequence num: %d", int(seq));
            REQUIRE_GE(int(seq), 0);
        }
        SUBCASE("WooFGet works") {
            int stored;
            auto res = WooFGet("foo", &stored, seq);
            REQUIRE_EQ(res, 1);
            REQUIRE_EQ(stored, 42);
        }
    }
}
} // namespace
} // namespace cspot
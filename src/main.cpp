#include <iblcli.h>
#include <util.h>

using namespace ibl;

int main(int argc, char* argv[]) {
    CliOptions opts;

    try {
        opts = ParseArgs(argc, argv);
        ExecuteJob(opts);
    } catch (const std::runtime_error& err) {
        util::ExitWithErrorMsg(err.what());
    }
}
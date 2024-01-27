#ifndef IBL_IBLAPP_H
#define IBL_IBLAPP_H

#include <iblenv.h>

namespace ibl {

struct CliOptions;

void ExecuteJob(const CliOptions& opts);
void Cleanup();

} // namespace ibl

#endif
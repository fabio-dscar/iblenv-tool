#ifndef __IBLAPP_H__
#define __IBLAPP_H__

#include <iblenv.h>

namespace ibl {

struct CliOptions;

void ExecuteJob(const CliOptions& opts);
void Cleanup();

} // namespace ibl

#endif // __IBLAPP_H__
#pragma once
#include "GridCore.hpp"
#include <filesystem>
namespace julretsu {
struct XlsxImport { Batch data; std::string summary; };
XlsxImport read_xlsx(const std::filesystem::path&,bool native_formulas=false);
std::string write_xlsx(const std::filesystem::path&,const Sheet&);
}

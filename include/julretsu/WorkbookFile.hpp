#pragma once
#include "GridCore.hpp"
#include <filesystem>
namespace julretsu {
struct WorkbookData { Batch data; std::string script; };
// Versioned native workbook. Loading never mutates the current sheet.
void write_file_atomic(const std::filesystem::path&,std::string_view);
WorkbookData read_workbook(const std::filesystem::path&);
void write_workbook(const std::filesystem::path&,const Sheet&,std::string_view script);
}

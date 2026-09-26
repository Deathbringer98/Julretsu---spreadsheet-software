#pragma once
#include "GridCore.hpp"
#include <filesystem>
namespace julretsu {
struct XlsxImport { Batch data; std::string summary; std::string name; };
struct XlsxWorkbook { std::vector<XlsxImport> sheets; std::string summary; };
struct XlsxSheet { std::string name; const Sheet* sheet; };
XlsxWorkbook read_xlsx_workbook(const std::filesystem::path&,bool native_formulas=false);
std::string write_xlsx_workbook(const std::filesystem::path&,const std::vector<XlsxSheet>&);
XlsxImport read_xlsx(const std::filesystem::path&,bool native_formulas=false);
std::string write_xlsx(const std::filesystem::path&,const Sheet&);
}

#pragma once
#include "GridCore.hpp"
#include <filesystem>
namespace julretsu {
struct WorkbookData { Batch data; std::string script; std::vector<ChangeRecord> journal; };
struct DocumentSheet { std::string name,bytes; bool operator==(const DocumentSheet&) const = default; };
std::string serialize_sheet(const Sheet&,std::string_view script);
WorkbookData deserialize_sheet(std::string_view);
void write_document(const std::filesystem::path&,const std::vector<DocumentSheet>&);
std::vector<DocumentSheet> read_document(const std::filesystem::path&);
// Versioned native workbook. Loading never mutates the current sheet.
void write_file_atomic(const std::filesystem::path&,std::string_view);
WorkbookData read_workbook(const std::filesystem::path&);
void write_workbook(const std::filesystem::path&,const Sheet&,std::string_view script);
}

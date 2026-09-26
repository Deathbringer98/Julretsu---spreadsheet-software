#pragma once
#include "WorkbookFile.hpp"
namespace julretsu {
struct RestorePoint { std::filesystem::path path; std::string label; };
std::vector<RestorePoint> restore_points(const std::filesystem::path& workbook);
std::filesystem::path create_restore_point(const std::filesystem::path& workbook,const std::vector<DocumentSheet>&,std::string label,bool automatic=false);
struct WorkbookDifference { std::size_t cells{},metadata{}; std::vector<std::string> details; };
WorkbookDifference compare_documents(const std::vector<DocumentSheet>& current,const std::vector<DocumentSheet>& target);
}

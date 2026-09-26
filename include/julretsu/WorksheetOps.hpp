#pragma once
#include "GridCore.hpp"
namespace julretsu {
std::string adjust_references(std::string_view,int rows,int columns,int axis=-1,unsigned position=0,bool deleting=false);
Batch create_table(const Sheet&,StructuredTable);
Batch append_table_row(const Sheet&,std::size_t);
Batch expand_tables(const Sheet&,const Batch&);
Batch structural_edit(const Sheet&,bool rows,unsigned position,bool deleting);
Batch sort_range(const Sheet&,CellCoord first,CellCoord last,unsigned key,bool descending);
std::vector<std::vector<std::string>> parse_clipboard(std::string_view);
std::string quote_clipboard(std::string_view);
}

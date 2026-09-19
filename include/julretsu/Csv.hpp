#pragma once
#include "IO.hpp"
namespace julretsu {
class CsvAdapter final:public SheetStreamAdapter {
public:
    StreamResult import_sheet(std::istream&,Sheet&,const StreamOptions&) override;
    StreamResult export_sheet(std::ostream&,const Sheet&,const StreamOptions&) override;
};
bool valid_utf8(std::string_view);
}

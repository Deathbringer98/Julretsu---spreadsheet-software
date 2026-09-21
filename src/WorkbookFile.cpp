#include "julretsu/WorkbookFile.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <sstream>
#include <set>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace julretsu {
namespace {
constexpr std::size_t maximum_file=40*1024*1024;
void number(std::ostream& out,std::uint64_t value,int bytes) {
    for(int i=0;i<bytes;++i) out.put(char((value>>(i*8))&255));
}
std::uint64_t number(std::istream& in,int bytes) {
    std::uint64_t value=0;
    for(int i=0;i<bytes;++i) { int c=in.get(); if(c==EOF) throw std::runtime_error("The workbook is incomplete or damaged."); value|=std::uint64_t(c)<<(i*8); }
    return value;
}
void text(std::ostream& out,std::string_view value) { number(out,value.size(),4); out.write(value.data(),std::streamsize(value.size())); }
std::string text(std::istream& in,std::size_t limit,std::size_t& budget) {
    const auto size=number(in,4);
    if(size>limit||size>budget) throw std::runtime_error("Workbook text exceeds the supported limit.");
    budget-=std::size_t(size); std::string value(std::size_t(size),'\0');
    in.read(value.data(),std::streamsize(size)); if(!in) throw std::runtime_error("The workbook is incomplete or damaged."); return value;
}
void replace_file(const std::filesystem::path& path,std::string_view bytes) {
    static std::atomic<unsigned> sequence{};
    auto temporary=path;
    temporary+=std::string(".saving-")+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(sequence++);
#ifdef _WIN32
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create the file. Choose a writable folder and check free space.");
    DWORD written=0;
    bool ok=WriteFile(file,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size();
    if(ok) ok=FlushFileBuffers(file)!=0;
    if(!CloseHandle(file)) ok=false;
    if(ok) ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
    int file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(file<0) throw std::runtime_error("Cannot create the file. Choose a writable folder and check free space.");
    std::size_t offset=0; bool ok=true;
    while(offset<bytes.size()) { auto n=::write(file,bytes.data()+offset,bytes.size()-offset); if(n<=0) {ok=false;break;} offset+=std::size_t(n); }
    if(ok) ok=::fsync(file)==0;
    if(::close(file)!=0) ok=false;
    if(ok) { std::error_code error; std::filesystem::rename(temporary,path,error); ok=!error; }
#endif
    if(!ok) { std::error_code ignored; std::filesystem::remove(temporary,ignored); throw std::runtime_error("Save failed. The existing workbook was not replaced. Check folder permissions, free space, or whether the file is locked."); }
}
}
void write_file_atomic(const std::filesystem::path& path,std::string_view bytes) { replace_file(path,bytes); }
std::string serialize_sheet(const Sheet& sheet,std::string_view script) {
    if(script.size()>16384) throw std::runtime_error("The Lua script exceeds the supported limit.");
    std::ostringstream out(std::ios::binary);
    out.write("JULRETSU",8); number(out,4,4); number(out,sheet.populated_cells(),4);
    for(const auto& [row,cells]:sheet.populated_rows()) for(const auto& [column,cell]:cells) {
        number(out,row,4); number(out,column,4); number(out,cell.input.index(),1);
        if(auto n=std::get_if<double>(&cell.input)) number(out,std::bit_cast<std::uint64_t>(*n),8);
        else if(auto b=std::get_if<bool>(&cell.input)) number(out,*b?1:0,1);
        else if(auto s=std::get_if<std::string>(&cell.input)) text(out,*s);
        else if(auto f=std::get_if<FormulaInput>(&cell.input)) text(out,f->source);
    }
    number(out,sheet.row_styles().size(),4);
    for(const auto& [row,style]:sheet.row_styles()) {
        number(out,row,4); number(out,style.bold,1); number(out,style.foreground,4); number(out,style.background,4); number(out,style.decimals,1);
    }
    number(out,sheet.cell_styles().size(),4);
    for(const auto& [coord,style]:sheet.cell_styles()) {
        number(out,coord.row,4); number(out,coord.column,4); number(out,style.bold,1);
        number(out,style.foreground,4); number(out,style.background,4); number(out,style.decimals,1);
        number(out,style.alignment,1); number(out,style.number_format,1); number(out,style.font_size,1); number(out,style.border,1); number(out,style.font_family,1);
    }
    text(out,script);
    // Change history, newest records first until a 4 MiB budget, written oldest first.
    const auto& journal=sheet.journal(); std::size_t first=journal.size(), bytes_used=0;
    while(first>0) {
        const auto& record=journal[first-1]; std::size_t size=32+record.label.size();
        for(const auto& change:record.cells) size+=18+change.before.size()+change.after.size();
        if(bytes_used+size>4*1024*1024) break;
        bytes_used+=size; --first;
    }
    number(out,journal.size()-first,4);
    for(std::size_t i=first;i<journal.size();++i) {
        const auto& record=journal[i];
        number(out,std::uint64_t(record.time),8); text(out,record.label.substr(0,128)); number(out,record.total_cells,4); number(out,record.formats,4);
        number(out,record.cells.size(),4);
        for(const auto& change:record.cells) {
            number(out,change.coord.row,4); number(out,change.coord.column,4); number(out,change.before_kind,1); number(out,change.after_kind,1);
            text(out,change.before); text(out,change.after);
        }
    }
    const auto bytes=out.str();
    if(bytes.size()>maximum_file) throw std::runtime_error("Workbook exceeds the supported file size.");
    return bytes;
}
void write_workbook(const std::filesystem::path& path,const Sheet& sheet,std::string_view script) { replace_file(path,serialize_sheet(sheet,script)); }
WorkbookData deserialize_sheet(std::string_view bytes) {
    std::istringstream in(std::string(bytes),std::ios::binary);
    if(bytes.size()<16||bytes.size()>maximum_file)throw std::runtime_error("Invalid workbook size.");

    in.seekg(0); char magic[8]{}; in.read(magic,8);
    const auto version=number(in,4);
    if(std::string_view(magic,8)!="JULRETSU"||(version<1||version>4)) throw std::runtime_error("This is not a supported Julretsu workbook. Choose a .julretsu file.");
    WorkbookData result; Limits limits; std::size_t budget=limits.sheet_input_bytes;
    const auto count=number(in,4); if(count>limits.populated_cells) throw std::runtime_error("Workbook has too many cells.");
    std::set<CellCoord> seen;
    for(std::uint64_t i=0;i<count;++i) {
        CellCoord coord{std::uint32_t(number(in,4)),std::uint32_t(number(in,4))};
        if(!coord.valid()||!seen.insert(coord).second) throw std::runtime_error("Invalid or duplicate cell address in workbook.");
        Input value;
        switch(number(in,1)) {
        case 0:break;
        case 1: {const double n=std::bit_cast<double>(number(in,8)); if(!std::isfinite(n)) throw std::runtime_error("Invalid number in workbook."); value=n;break;}
        case 2: {const auto b=number(in,1);if(b>1) throw std::runtime_error("Invalid Boolean in workbook.");value=b!=0;break;}
        case 3:value=text(in,limits.text_bytes,budget);break;
        case 4:value=FormulaInput{text(in,limits.formula_bytes,budget)};break;
        default:throw std::runtime_error("Invalid cell type in workbook.");
        }
        result.data.cells.push_back({coord,std::move(value)});
    }
    const auto styles=number(in,4); if(styles>max_rows) throw std::runtime_error("Workbook has too many row styles.");
    std::set<std::uint32_t> styled;
    for(std::uint64_t i=0;i<styles;++i) {
        const auto row=std::uint32_t(number(in,4)); const auto bold=number(in,1);
        if(row>=max_rows||bold>1||!styled.insert(row).second) throw std::runtime_error("Invalid row style in workbook.");
        Style style;style.bold=bold!=0;style.foreground=std::uint32_t(number(in,4));style.background=std::uint32_t(number(in,4));style.decimals=std::uint8_t(number(in,1));
        if(style.decimals>12) throw std::runtime_error("Unsupported decimal format in workbook.");
        result.data.rows.push_back({row,style});
    }
    if(version>=2) {
        const auto count=number(in,4); if(count>limits.populated_cells) throw std::runtime_error("Too many cell formats.");
        std::set<CellCoord> seen_formats;
        for(std::uint64_t i=0;i<count;++i) {
            CellCoord c{std::uint32_t(number(in,4)),std::uint32_t(number(in,4))};
            Style style; const auto bold=number(in,1); style.bold=bold!=0;
            style.foreground=std::uint32_t(number(in,4)); style.background=std::uint32_t(number(in,4)); style.decimals=std::uint8_t(number(in,1));
            style.alignment=std::uint8_t(number(in,1)); style.number_format=std::uint8_t(number(in,1)); style.font_size=std::uint8_t(number(in,1)); const auto border=number(in,1); style.border=border!=0;if(version>=3)style.font_family=std::uint8_t(number(in,1));
            if(!c.valid()||!seen_formats.insert(c).second||bold>1||border>1||style.decimals>12||style.alignment>3||style.number_format>5||style.font_size<8||style.font_size>36||style.font_family>2) throw std::runtime_error("Invalid cell format.");
            result.data.formats.push_back({c,style});
        }
    }
    budget=16384;result.script=text(in,16384,budget);
    if(version>=4) {
        const auto records=number(in,4); if(records>journal_records) throw std::runtime_error("Too many history records in workbook.");
        std::size_t history_budget=8*1024*1024;
        for(std::uint64_t i=0;i<records;++i) {
            ChangeRecord record; record.time=std::int64_t(number(in,8)); record.label=text(in,128,history_budget);
            record.total_cells=std::uint32_t(number(in,4)); record.formats=std::uint32_t(number(in,4));
            const auto cells=number(in,4); if(cells>journal_cells_per_record) throw std::runtime_error("Invalid history record in workbook.");
            for(std::uint64_t j=0;j<cells;++j) {
                CellChange change; change.coord={std::uint32_t(number(in,4)),std::uint32_t(number(in,4))};
                change.before_kind=std::uint8_t(number(in,1)); change.after_kind=std::uint8_t(number(in,1));
                if(!change.coord.valid()||change.before_kind>4||change.after_kind>4) throw std::runtime_error("Invalid history record in workbook.");
                change.before=text(in,journal_text+8,history_budget); change.after=text(in,journal_text+8,history_budget);
                record.cells.push_back(std::move(change));
            }
            result.journal.push_back(std::move(record));
        }
    }
    if(result.script.find('\0')!=std::string::npos||in.peek()!=EOF) throw std::runtime_error("Unexpected data in workbook.");
    return result;
}
std::string file_bytes(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in||in.tellg()<0||in.tellg()>std::streamoff(maximum_file))throw std::runtime_error("Cannot read workbook or size limit exceeded.");
    std::string bytes(std::size_t(in.tellg()),'\0');in.seekg(0);in.read(bytes.data(),std::streamsize(bytes.size()));if(!in)throw std::runtime_error("Cannot read complete workbook.");return bytes;
}
WorkbookData read_workbook(const std::filesystem::path& path) {return deserialize_sheet(file_bytes(path));}
void write_document(const std::filesystem::path& path,const std::vector<DocumentSheet>& sheets) {
    if(sheets.empty()||sheets.size()>64)throw std::runtime_error("A workbook supports 1 to 64 sheets.");
    std::ostringstream out(std::ios::binary);out.write("JULBOOK1",8);number(out,sheets.size(),4);std::set<std::string> names;
    for(const auto& sheet:sheets) {
        if(sheet.name.empty()||sheet.name.size()>64||!names.insert(sheet.name).second)throw std::runtime_error("Sheet names must be unique and at most 64 bytes.");
        text(out,sheet.name);text(out,sheet.bytes);
    }
    auto bytes=out.str();if(bytes.size()>maximum_file)throw std::runtime_error("Workbook exceeds 40 MiB.");replace_file(path,bytes);
}
std::vector<DocumentSheet> read_document(const std::filesystem::path& path) {
    auto bytes=file_bytes(path);if(bytes.substr(0,8)=="JULRETSU") { (void)deserialize_sheet(bytes);return {{"Sheet 1",std::move(bytes)}}; }
    if(bytes.substr(0,8)!="JULBOOK1")throw std::runtime_error("Unsupported workbook.");
    std::istringstream in(bytes,std::ios::binary);in.seekg(8);auto count=number(in,4);if(count<1||count>64)throw std::runtime_error("Invalid worksheet count.");
    std::size_t budget=maximum_file;std::vector<DocumentSheet> result;std::set<std::string> names;
    for(unsigned i=0;i<count;++i) {
        auto name=text(in,64,budget),data=text(in,maximum_file,budget);
        if(name.empty()||name.find('\0')!=std::string::npos||!names.insert(name).second)throw std::runtime_error("Invalid worksheet name.");
        (void)deserialize_sheet(data);result.push_back({std::move(name),std::move(data)});
    }
    if(in.peek()!=EOF)throw std::runtime_error("Unexpected workbook data.");return result;
}

}

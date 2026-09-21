#include "julretsu/Csv.hpp"
#include <charconv>
#include <cmath>
#include <array>
#include <stdexcept>
namespace julretsu {
bool valid_utf8(std::string_view text) {
    for(std::size_t i=0;i<text.size();) {
        auto c=static_cast<unsigned char>(text[i++]); if(c==0)return false;if(c<128)continue;
        unsigned n=0,code=0,min=0;
        if(c>=0xc2&&c<=0xdf){n=1;code=c&31;min=128;}else if(c>=0xe0&&c<=0xef){n=2;code=c&15;min=2048;}else if(c>=0xf0&&c<=0xf4){n=3;code=c&7;min=65536;}else return false;
        while(n--){if(i==text.size())return false;auto b=static_cast<unsigned char>(text[i++]);if((b&0xc0)!=0x80)return false;code=(code<<6)|(b&63);}
        if(code<min||code>0x10ffff||(code>=0xd800&&code<=0xdfff))return false;
    }return true;
}
StreamResult CsvAdapter::import_sheet(std::istream& in,Sheet& sheet,const StreamOptions& options) {
    StreamResult result;Batch batch;std::string field;unsigned row=0,column=0;std::size_t fields=0;
    enum class State{Start,Plain,Quoted,Closed};State state=State::Start;bool record=false;
    auto check=[&]{if(options.cancellation.stop_requested()){result.cancelled=true;throw std::runtime_error("Import cancelled; workbook unchanged.");}};
    auto get=[&](){int c=in.get();if(c!=EOF){++result.bytes;if(result.bytes>32*1024*1024)throw std::runtime_error("CSV exceeds 32 MiB.");if(result.bytes%4096==0){check();if(options.progress)options.progress(result.bytes,batch.cells.size());}}return c;};
    auto cell=[&] {
        if(row>=max_rows||column>=max_columns||++fields>2'000'000)throw std::runtime_error("CSV dimensions exceed supported limits.");
        if(!valid_utf8(field))throw std::runtime_error("CSV must contain valid UTF-8 without NUL characters.");
        if(!field.empty()) {
            Input value=field;
            if(options.interpretation==ImportInterpretation::Values) {
                double n{};auto [end,error]=parse_double(field.data(),field.data()+field.size(),n);
                if(error==std::errc{}&&end==field.data()+field.size()&&std::isfinite(n))value=n;
                else if(field=="TRUE"||field=="FALSE")value=field=="TRUE";
            } else if(options.interpretation==ImportInterpretation::NativeFormulas&&field.front()=='=') {
                // Formula activation is deliberately not offered by the CSV importer.
                throw std::runtime_error("CSV formula activation is unsupported. Import as text or values.");
            }
            batch.cells.push_back({{row,column},std::move(value)});
            if(batch.cells.size()>100'000)throw std::runtime_error("CSV exceeds 100,000 populated cells.");
        }
        field.clear();++column;state=State::Start;
    };
    try {
        check(); if(in.peek()==0xef){if(get()!=0xef||get()!=0xbb||get()!=0xbf)throw std::runtime_error("Invalid UTF-8 BOM.");}
        int c;
        while((c=get())!=EOF) {
            record=true;
            if(state==State::Quoted) {if(c=='"')state=State::Closed;else field.push_back(char(c));}
            else if(state==State::Closed&&c=='"'){field.push_back('"');state=State::Quoted;}
            else if(c==','){cell();}
            else if(c=='\r'||c=='\n') {cell();++row;column=0;record=false;if(c=='\r'&&in.peek()=='\n')get();}
            else if(state==State::Start&&c=='"')state=State::Quoted;
            else if(state==State::Closed||c=='"')throw std::runtime_error("Malformed CSV quoting.");
            else {field.push_back(char(c));state=State::Plain;}
            if(field.size()>1'048'576)throw std::runtime_error("CSV field exceeds 1 MiB.");
        }
        if(in.bad())throw std::runtime_error("Could not read CSV.");
        if(state==State::Quoted)throw std::runtime_error("Unclosed quoted CSV field.");
        if(record)cell();check();
        const auto applied=sheet.apply(batch);if(!applied.accepted)throw std::runtime_error(applied.error->context);
        result.cells=batch.cells.size();if(options.progress)options.progress(result.bytes,result.cells);
    }catch(const std::exception& error){result.error=CellError{ErrorCode::Value,error.what(),{}};}
    return result;
}
StreamResult CsvAdapter::export_sheet(std::ostream& out,const Sheet& sheet,const StreamOptions& options) {
    StreamResult result;
    try {
        unsigned rows=0,columns=0;
        for(const auto& [r,cells]:sheet.populated_rows())if(!cells.empty()){rows=std::max(rows,r+1);columns=std::max(columns,cells.rbegin()->first+1);}
        if(std::uint64_t(rows)*columns>2'000'000)throw std::runtime_error("CSV export rectangle exceeds two million fields. Copy a smaller range first.");
        auto put=[&](std::string_view s){result.bytes+=s.size();if(result.bytes>64*1024*1024)throw std::runtime_error("CSV output exceeds 64 MiB.");out.write(s.data(),std::streamsize(s.size()));if(!out)throw std::runtime_error("Cannot write CSV.");};
        for(unsigned r=0;r<rows;++r) {
            if(options.cancellation.stop_requested()){result.cancelled=true;throw std::runtime_error("Export cancelled.");}
            for(unsigned c=0;c<columns;++c) {
                if(c)put(",");auto cell=sheet.cell({r,c});if(!cell)continue;
                std::string value;
                if(auto n=std::get_if<double>(&cell->cached)){std::array<char,64> b{};auto [end,ec]=std::to_chars(b.data(),b.data()+b.size(),*n);if(ec!=std::errc{})throw std::runtime_error("Cannot format number.");value.assign(b.data(),end);}
                else value=display(cell->cached);
                if(!valid_utf8(value))throw std::runtime_error("A cell contains invalid UTF-8 or NUL.");
                // Prefix potentially executable text for safe opening in spreadsheet apps.
                if(options.safe_text_export&&std::holds_alternative<std::string>(cell->cached)) {
                    auto first=value.find_first_not_of(" \r\n\t");
                    if((first!=std::string::npos&&std::string_view("=+-@").find(value[first])!=std::string_view::npos)||(!value.empty()&&(value.front()=='\t'||value.front()=='\r')))value.insert(value.begin(),'\'');
                }
                put("\"");for(char ch:value){if(ch=='"')put("\"");put(std::string_view(&ch,1));}put("\"");++result.cells;
            }put("\r\n");if(options.progress)options.progress(result.bytes,result.cells);
        }
    }catch(const std::exception& e){result.error=CellError{ErrorCode::Value,e.what(),{}};}return result;
}
}

#include "julretsu/Xlsx.hpp"
#include "julretsu/WorkbookFile.hpp"
#include "julretsu/Csv.hpp"
#include <miniz.h>
#include <pugixml.hpp>
#include <fstream>
#include <sstream>
#include <charconv>
#include <cmath>
#include <set>
#include <array>
#include <memory>
#include <cstring>
#include <algorithm>
namespace julretsu {
namespace {
constexpr std::size_t max_zip=64*1024*1024,max_xml=32*1024*1024;
struct Zip {
    mz_zip_archive zip{};bool writer=false;std::string input;
    explicit Zip(const std::filesystem::path& path) {
        std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()<0||in.tellg()>std::streamoff(max_zip))throw std::runtime_error("Cannot read XLSX, or file exceeds 64 MiB.");
        input.resize(std::size_t(in.tellg()));in.seekg(0);in.read(input.data(),std::streamsize(input.size()));
        if(!mz_zip_reader_init_mem(&zip,input.data(),input.size(),0))throw std::runtime_error("Not a supported XLSX ZIP workbook (encrypted files are unsupported).");
    }
    Zip(){writer=true;if(!mz_zip_writer_init_heap(&zip,0,0))throw std::runtime_error("Cannot create XLSX archive.");}
    ~Zip(){if(writer)mz_zip_writer_end(&zip);else mz_zip_reader_end(&zip);}
    Zip(const Zip&)=delete;Zip& operator=(const Zip&)=delete;
    std::string get(const std::string& name,bool required=true){int i=mz_zip_reader_locate_file(&zip,name.c_str(),nullptr,0);if(i<0){if(required)throw std::runtime_error("XLSX is missing "+name);return {};}
        mz_zip_archive_file_stat stat{};if(!mz_zip_reader_file_stat(&zip,unsigned(i),&stat)||stat.m_uncomp_size>max_xml)throw std::runtime_error("XLSX XML part exceeds 32 MiB.");
        std::string result(std::size_t(stat.m_uncomp_size),'\0');if(!mz_zip_reader_extract_to_mem(&zip,unsigned(i),result.data(),result.size(),0))throw std::runtime_error("Damaged XLSX ZIP entry.");return result;}
    void add(const char* name,const std::string& xml){if(xml.size()>max_xml||!mz_zip_writer_add_mem(&zip,name,xml.data(),xml.size(),MZ_BEST_SPEED))throw std::runtime_error("XLSX output exceeded limits.");}
};
const char* local(const char* name){const char* colon=std::strchr(name,':');return colon?colon+1:name;}
pugi::xml_node child(pugi::xml_node n,std::string_view name){for(auto c:n.children())if(local(c.name())==name)return c;return {};}
void xml(pugi::xml_document& doc,const std::string& bytes){
    if(bytes.find("<!DOCTYPE")!=std::string::npos||bytes.find("<!ENTITY")!=std::string::npos)throw std::runtime_error("XLSX DTD/entity declarations are unsupported.");
    if(!doc.load_buffer(bytes.data(),bytes.size(),pugi::parse_default,pugi::encoding_utf8))throw std::runtime_error("Invalid XLSX XML.");
}
unsigned index(std::string_view text){unsigned n{};auto [end,e]=std::from_chars(text.data(),text.data()+text.size(),n);if(e!=std::errc{}||end!=text.data()+text.size())throw std::runtime_error("Invalid XLSX index.");return n;}
std::string content(pugi::xml_node n){std::string result;for(auto c:n.children()){if(std::string_view(local(c.name()))=="t")result+=c.text().as_string();else if(std::string_view(local(c.name()))=="r")result+=child(c,"t").text().as_string();}if(!valid_utf8(result)||result.size()>1'048'576)throw std::runtime_error("Invalid or oversized XLSX text.");return result;}
bool formula_ok(const std::string& source){if(source.find_first_of("\"\\![]$")!=std::string::npos)return false;auto parsed=parse_formula(source,{});if(parsed.error)return false;for(auto& n:parsed.nodes)if(n.kind==NodeKind::Call&&n.op!="SUM"&&n.op!="AVERAGE"&&n.op!="IF")return false;return true;}
std::uint32_t color(pugi::xml_node node,std::uint32_t fallback){auto rgb=node.attribute("rgb");if(!rgb)return fallback;std::string_view s=rgb.value();if(s.size()!=8)return fallback;unsigned n{};auto [end,e]=std::from_chars(s.data(),s.data()+8,n,16);return e==std::errc{}&&end==s.data()+8?(n<<8)|255:fallback;}
std::string decimal(double n){std::array<char,64>b{};auto[end,e]=std::to_chars(b.data(),b.data()+b.size(),n);if(e!=std::errc{})throw std::runtime_error("Number conversion failed.");return {b.data(),end};}
std::string serialized(pugi::xml_document& doc){std::ostringstream out;doc.save(out,"",pugi::format_raw,pugi::encoding_utf8);return out.str();}
std::string rgb(std::uint32_t color){char b[16]{};std::snprintf(b,sizeof b,"FF%06X",color>>8);return b;}
}
XlsxImport read_xlsx(const std::filesystem::path& path,bool formulas){
    Zip zip(path);std::uint64_t expanded=0;std::set<std::string> names;const auto entries=mz_zip_reader_get_num_files(&zip.zip);
    if(entries>2048)throw std::runtime_error("Too many XLSX ZIP parts.");
    for(unsigned i=0;i<entries;++i){mz_zip_archive_file_stat s{};if(!mz_zip_reader_file_stat(&zip.zip,i,&s)||s.m_is_encrypted)throw std::runtime_error("Encrypted XLSX is unsupported.");expanded+=s.m_uncomp_size;if(expanded>max_zip)throw std::runtime_error("Expanded XLSX exceeds 64 MiB.");if(!names.insert(s.m_filename).second)throw std::runtime_error("Duplicate XLSX ZIP part.");}
    pugi::xml_document workbook,rels;xml(workbook,zip.get("xl/workbook.xml"));xml(rels,zip.get("xl/_rels/workbook.xml.rels"));
    auto sheets=child(workbook.document_element(),"sheets");auto first=sheets.first_child();if(!first)throw std::runtime_error("No worksheet found.");
    std::string id;for(auto a:first.attributes())if(std::string_view(local(a.name()))=="id")id=a.value();
    std::string target;for(auto r:rels.document_element().children())if(id==r.attribute("Id").value()){if(std::string_view(r.attribute("TargetMode").value())=="External")throw std::runtime_error("External sheets are unsupported.");target=r.attribute("Target").value();}
    if(target.empty())throw std::runtime_error("Worksheet relationship missing.");
    if(target.front()=='/')target.erase(0,1);else target="xl/"+target;
    if(target.find("..")!=std::string::npos||target.find('\\')!=std::string::npos||target.find(':')!=std::string::npos)throw std::runtime_error("Unsupported worksheet path.");
    std::vector<std::string> strings;auto shared=zip.get("xl/sharedStrings.xml",false);if(!shared.empty()){pugi::xml_document doc;xml(doc,shared);for(auto item:doc.document_element().children()){if(strings.size()>=100'000)throw std::runtime_error("Too many shared strings.");strings.push_back(content(item));}}
    std::vector<Style> styles(1);std::vector<bool> dates(1);auto style_xml=zip.get("xl/styles.xml",false);
    if(!style_xml.empty()){
        pugi::xml_document doc;xml(doc,style_xml);auto root=doc.document_element();std::vector<pugi::xml_node> fonts,fills;
        for(auto n:child(root,"fonts").children())fonts.push_back(n);for(auto n:child(root,"fills").children())fills.push_back(n);
        styles.clear();dates.clear();for(auto xf:child(root,"cellXfs").children()){
            if(styles.size()>4096)throw std::runtime_error("Too many XLSX styles.");Style s;
            auto font=xf.attribute("fontId").as_uint(),fill=xf.attribute("fillId").as_uint(),num=xf.attribute("numFmtId").as_uint();
            if(font<fonts.size()){s.bold=bool(child(fonts[font],"b"));s.foreground=color(child(fonts[font],"color"),s.foreground);}
            if(fill<fills.size())s.background=color(child(child(fills[fill],"patternFill"),"fgColor"),s.background);
            if(num==1)s.decimals=0;else if(num==2)s.decimals=2;
            styles.push_back(s);dates.push_back((num>=14&&num<=22)||(num>=45&&num<=47)||num>=164);
        }if(styles.empty()){styles.push_back({});dates.push_back(false);}
    }
    pugi::xml_document document;xml(document,zip.get(target));auto root=document.document_element();XlsxImport result;
    std::size_t cached=0,active=0,missing=0,serials=0,mixed=0;std::set<CellCoord> seen;
    for(auto row:child(root,"sheetData").children()){
        if(std::string_view(local(row.name()))!="row")continue;
        std::optional<Style> uniform;bool inconsistent=false;unsigned r=row.attribute("r")?index(row.attribute("r").value())-1:0;
        if(r>=max_rows)throw std::runtime_error("Worksheet exceeds one million rows.");
        if(row.attribute("s")){auto id=index(row.attribute("s").value());if(id>=styles.size())throw std::runtime_error("Invalid row style.");uniform=styles[id];}
        for(auto cell:row.children()){
            if(std::string_view(local(cell.name()))!="c")continue;
            auto parsed=from_a1(cell.attribute("r").value());auto coord=std::get_if<CellCoord>(&parsed);if(!coord||coord->row!=r||!seen.insert(*coord).second)throw std::runtime_error("Invalid XLSX cell coordinates.");
            if(seen.size()>100'000)throw std::runtime_error("XLSX exceeds 100,000 cells.");
            auto style=cell.attribute("s").as_uint();if(style>=styles.size())throw std::runtime_error("Invalid cell style.");if(!uniform)uniform=styles[style];else if(*uniform!=styles[style])inconsistent=true;
            std::string type=cell.attribute("t").value();auto v=child(cell,"v");auto f=child(cell,"f");Input value;
            if(f&&formulas&&!*f.attribute("t").value()&&formula_ok("="+std::string(f.text().as_string()))){value=FormulaInput{"="+std::string(f.text().as_string())};++active;}
            else {
                if(f){++cached;if(!v)++missing;}
                if(type=="inlineStr")value=content(child(cell,"is"));
                else if(type=="s"){auto n=index(v.text().as_string());if(n>=strings.size())throw std::runtime_error("Invalid shared string index.");value=strings[n];}
                else if(type=="b"){std::string b=v.text().as_string();if(b!="0"&&b!="1")throw std::runtime_error("Invalid XLSX Boolean.");value=b=="1";}
                else if(type=="str"||type=="e"||type=="d")value=std::string(v.text().as_string());
                else if(v){std::string text=v.text().as_string();double n{};auto[end,e]=parse_double(text.data(),text.data()+text.size(),n);if(e!=std::errc{}||end!=text.data()+text.size()||!std::isfinite(n))throw std::runtime_error("Invalid XLSX number.");value=n;if(dates[style])++serials;}
                else if(f)value=std::string("[Formula has no cached value]");
            }
            if(auto text=std::get_if<std::string>(&value);text&&!valid_utf8(*text))throw std::runtime_error("Invalid XLSX text.");
            result.data.cells.push_back({*coord,std::move(value)});
        }
        if(inconsistent)++mixed;else if(uniform&&*uniform!=Style{})result.data.rows.push_back({r,*uniform});
    }
    std::size_t sheet_count=0;for(auto n:sheets.children()){(void)n;++sheet_count;}
    result.summary="Import worksheet: "+std::string(first.attribute("name").value())+" (first of "+std::to_string(sheet_count)+").\n"+std::to_string(result.data.cells.size())+" cells. "+std::to_string(active)+" supported formulas activated; "+std::to_string(cached)+" formulas imported as cached values ("+std::to_string(missing)+" missing).\n"+std::to_string(mixed)+" rows have mixed cell formatting that cannot be preserved. "+std::to_string(serials)+" date/custom-format numbers remain Excel serial/numeric values.\nOnly bold, direct RGB colors, and basic decimal row styles are supported. Merges, charts, other sheets, column sizing and advanced styles are not imported. Lua never activates from XLSX. Continue?";
    return result;
}
std::string write_xlsx(const std::filesystem::path& path,const Sheet& sheet){
    Zip zip;std::vector<Style> styles{Style{}};std::map<unsigned,unsigned> row_styles;
    for(auto [row,style]:sheet.row_styles()){auto it=std::find(styles.begin(),styles.end(),style);if(it==styles.end()){styles.push_back(style);it=styles.end()-1;}if(styles.size()>4096)throw std::runtime_error("Too many distinct row styles for XLSX.");row_styles[row]=unsigned(it-styles.begin());}
    pugi::xml_document style_doc;auto sr=style_doc.append_child("styleSheet");sr.append_attribute("xmlns")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";
    auto fonts=sr.append_child("fonts"),fills=sr.append_child("fills"),borders=sr.append_child("borders");
    fonts.append_attribute("count")=unsigned(styles.size());fills.append_attribute("count")=unsigned(styles.size()+2);
    fills.append_child("fill").append_child("patternFill").append_attribute("patternType")="none";fills.append_child("fill").append_child("patternFill").append_attribute("patternType")="gray125";
    for(auto s:styles){auto f=fonts.append_child("font");f.append_child("sz").append_attribute("val")=11;f.append_child("name").append_attribute("val")="Calibri";if(s.bold)f.append_child("b");f.append_child("color").append_attribute("rgb")=rgb(s.foreground).c_str();auto pattern=fills.append_child("fill").append_child("patternFill");pattern.append_attribute("patternType")="solid";pattern.append_child("fgColor").append_attribute("rgb")=rgb(s.background).c_str();}
    borders.append_attribute("count")=1;auto border=borders.append_child("border");for(auto name:{"left","right","top","bottom","diagonal"})border.append_child(name);
    auto base=sr.append_child("cellStyleXfs");base.append_attribute("count")=1;base.append_child("xf").append_attribute("numFmtId")=0;
    auto xfs=sr.append_child("cellXfs");xfs.append_attribute("count")=unsigned(styles.size());
    for(unsigned i=0;i<styles.size();++i){auto xf=xfs.append_child("xf");xf.append_attribute("fontId")=i;xf.append_attribute("fillId")=i+2;xf.append_attribute("borderId")=0;xf.append_attribute("xfId")=0;xf.append_attribute("numFmtId")=styles[i].decimals==0?1:2;xf.append_attribute("applyFont")=1;xf.append_attribute("applyFill")=1;xf.append_attribute("applyNumberFormat")=1;}
    auto named=sr.append_child("cellStyles");named.append_attribute("count")=1;auto normal=named.append_child("cellStyle");normal.append_attribute("name")="Normal";normal.append_attribute("xfId")=0;normal.append_attribute("builtinId")=0;
    zip.add("xl/styles.xml",serialized(style_doc));
    pugi::xml_document doc;auto root=doc.append_child("worksheet");root.append_attribute("xmlns")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";auto data=root.append_child("sheetData");std::set<unsigned> rows;
    for(auto& [r,c]:sheet.populated_rows()){(void)c;rows.insert(r);}for(auto&[r,s]:row_styles){(void)s;rows.insert(r);}std::size_t flattened=0;
    for(auto r:rows){auto row=data.append_child("row");row.append_attribute("r")=r+1;auto si=row_styles.contains(r)?row_styles[r]:0;row.append_attribute("s")=si;row.append_attribute("customFormat")=1;
        for(auto&[column,cell]:sheet.row_view(r)){auto c=row.append_child("c");c.append_attribute("r")=std::get<std::string>(to_a1({r,column})).c_str();c.append_attribute("s")=si;
            bool formula=false;if(auto f=std::get_if<FormulaInput>(&cell.input)){formula=formula_ok(f->source);if(formula)c.append_child("f").text().set(f->source.substr(1).c_str());else ++flattened;}
            if(auto n=std::get_if<double>(&cell.cached))c.append_child("v").text().set(decimal(*n).c_str());
            else if(auto b=std::get_if<bool>(&cell.cached)){c.append_attribute("t")="b";c.append_child("v").text().set(*b?"1":"0");}
            else if(auto e=std::get_if<CellError>(&cell.cached)){c.append_attribute("t")="e";auto error=error_display(e->code);c.append_child("v").text().set((error=="#CYCLE!"||error=="#LIMIT!"||error=="#PARSE!"||error=="#UNSUPPORTED!"?std::string("#VALUE!"):std::string(error)).c_str());}
            else {auto text=display(cell.cached);if(!valid_utf8(text))throw std::runtime_error("Invalid text for XLSX.");if(formula){c.append_attribute("t")="str";c.append_child("v").text().set(text.c_str());}else{c.append_attribute("t")="inlineStr";auto t=c.append_child("is").append_child("t");t.append_attribute("xml:space")="preserve";t.text().set(text.c_str());}}
        }
    }
    zip.add("xl/worksheets/sheet1.xml",serialized(doc));
    zip.add("[Content_Types].xml",R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)");
    zip.add("_rels/.rels",R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
    zip.add("xl/workbook.xml",R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Sheet 1" sheetId="1" r:id="rId1"/></sheets><calcPr fullCalcOnLoad="1"/></workbook>)");
    zip.add("xl/_rels/workbook.xml.rels",R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>)");
    void* memory=nullptr;std::size_t size=0;if(!mz_zip_writer_finalize_heap_archive(&zip.zip,&memory,&size))throw std::runtime_error("Cannot finalize XLSX.");
    std::unique_ptr<void,decltype(&mz_free)> owned(memory,mz_free);if(size>max_zip)throw std::runtime_error("XLSX exceeds 64 MiB.");write_file_atomic(path,std::string_view(static_cast<char*>(memory),size));
    return "XLSX exported. "+std::to_string(flattened)+" Lua/unsupported formulas were exported as values. Decimal formats use 0 or 2 places. Lua scripts are not included; keep a .julretsu copy.";
}
}

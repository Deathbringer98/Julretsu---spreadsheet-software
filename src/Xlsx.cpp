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
#include <regex>
#include <chrono>
#include "julretsu/I18n.hpp"
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
std::uint32_t color(pugi::xml_node node,std::uint32_t fallback){auto rgb=node.attribute("rgb");if(!rgb)return fallback;std::string_view s=rgb.value();if(s.size()!=8)return fallback;unsigned n{};auto [end,e]=std::from_chars(s.data(),s.data()+8,n,16);return e==std::errc{}&&end==s.data()+8?(n<<8)|255:fallback;}
std::string decimal(double n){std::array<char,64>b{};auto[end,e]=std::to_chars(b.data(),b.data()+b.size(),n);if(e!=std::errc{})throw std::runtime_error("Number conversion failed.");return {b.data(),end};}
std::string serialized(pugi::xml_document& doc){std::ostringstream out;doc.save(out,"",pugi::format_raw,pugi::encoding_utf8);return out.str();}
std::string rgb(std::uint32_t color){char b[16]{};std::snprintf(b,sizeof b,"FF%06X",color>>8);return b;}
std::string address(CellCoord c){return std::get<std::string>(to_a1(c));}
std::string range(CellCoord a,CellCoord z){return address(a)+":"+address(z);}
bool range_parse(std::string text,CellCoord& a,CellCoord& z){
    text.erase(std::remove(text.begin(),text.end(),'$'),text.end());auto colon=text.find(':');
    auto first=from_a1(text.substr(0,colon)),last=from_a1(colon==std::string::npos?text:text.substr(colon+1));
    auto x=std::get_if<CellCoord>(&first),y=std::get_if<CellCoord>(&last);if(!x||!y||x->row>y->row||x->column>y->column)return false;a=*x;z=*y;return true;
}
double numeric(const std::string& s){double value{};auto r=parse_double(s.data(),s.data()+s.size(),value);if(r.ec!=std::errc{}||r.ptr!=s.data()+s.size()||!std::isfinite(value))throw std::runtime_error(tr("Unsupported Excel validation formula."));return value;}
int date_serial(const std::string& s){
    if(s.size()!=10)throw std::runtime_error(tr("Unsupported Excel validation formula."));
    int y=int(numeric(s.substr(0,4))),m=int(numeric(s.substr(5,2))),d=int(numeric(s.substr(8,2)));
    std::chrono::year_month_day date{std::chrono::year(y),std::chrono::month(unsigned(m)),std::chrono::day(unsigned(d))};
    if(!date.ok())throw std::runtime_error(tr("Unsupported Excel validation formula."));
    auto days=(std::chrono::sys_days(date)-std::chrono::sys_days(std::chrono::year(1899)/12/30)).count();return int(days)-(y==1900&&m<3?1:0);
}
std::string serial_date(double serial){
    if(serial<1||serial>2958465||std::floor(serial)!=serial||serial==60)throw std::runtime_error(tr("Unsupported Excel validation formula."));
    auto date=std::chrono::year_month_day(std::chrono::sys_days(std::chrono::year(1899)/12/30)+std::chrono::days(int(serial)+(serial<60?1:0)));
    char out[16];std::snprintf(out,sizeof out,"%04d-%02u-%02u",int(date.year()),unsigned(date.month()),unsigned(date.day()));return out;
}
void read_validation(pugi::xml_node root,Batch& b){
    auto validations=child(root,"dataValidations");if(!validations)return;b.rules.emplace();
    for(auto dv:validations.children()){
        std::istringstream refs(dv.attribute("sqref").value());std::string ref;
        while(refs>>ref){
            ValidationRule r;if(!range_parse(ref,r.first,r.last))throw std::runtime_error(tr("Unsupported Excel validation formula."));
            r.required=!dv.attribute("allowBlank").as_bool();r.warning=!dv.attribute("showErrorMessage").as_bool()||std::string(dv.attribute("errorStyle").value())=="warning"||std::string(dv.attribute("errorStyle").value())=="information";
            std::string type=dv.attribute("type").value(),f1=child(dv,"formula1").text().as_string(),f2=child(dv,"formula2").text().as_string();
            if(type=="whole"||type=="decimal"||type=="date"){
                std::string op=dv.attribute("operator").value();if(!op.empty()&&op!="between")throw std::runtime_error(tr("Unsupported Excel validation formula."));
                r.kind=type=="whole"?ValidationKind::WholeNumber:type=="date"?ValidationKind::Date:ValidationKind::Number;
                if(type=="date"){r.date_min=serial_date(numeric(f1));r.date_max=serial_date(numeric(f2));}
                else {r.minimum=numeric(f1);r.maximum=numeric(f2);}
            }else if(type=="list"){
                r.kind=ValidationKind::List;if(f1.size()<2||f1.front()!='"'||f1.back()!='"')throw std::runtime_error(tr("Unsupported Excel validation formula."));
                std::istringstream list(f1.substr(1,f1.size()-2));std::string item;while(std::getline(list,item,','))r.choices.push_back(item);
            }else if(type=="custom"){
                auto expected="COUNTIF("+range(r.first,r.last)+","+address(r.first)+")<=1";
                std::string plain=f1;plain.erase(std::remove(plain.begin(),plain.end(),'$'),plain.end());
                if(plain==expected)r.unique=true;
                else if(plain!="LEN("+address(r.first)+")>0")throw std::runtime_error(tr("Unsupported Excel validation formula."));
            }else if(type!="none"&&!type.empty())throw std::runtime_error(tr("Unsupported Excel validation formula."));
            if(!valid_rule(r)||b.rules->size()>=1000)throw std::runtime_error(tr("Unsupported Excel validation formula."));
            b.rules->push_back(r);
        }
    }
}
std::string excel_formula(std::string source){
    // Direct worksheet references have an explicit, reversible native representation.
    static const std::regex link(R"LINK(^=SHEET\("([^"]+)","([A-Za-z]+[0-9]+)"\)$)LINK",std::regex::icase);
    std::smatch m;if(std::regex_match(source,m,link)){std::string name;for(char c:m[1].str()){name+=c;if(c=='\'')name+=c;}return "='"+name+"'!"+m[2].str();}
    return source;
}
std::string native_formula(std::string source){
    static const std::regex link(R"LINK(^=(?:'((?:[^']|'')+)'|([A-Za-z_][A-Za-z_0-9]*))!([A-Za-z]+[0-9]+)$)LINK");
    std::smatch m;if(std::regex_match(source,m,link)){std::string name=m[1].matched?m[1].str():m[2].str();for(std::size_t pos=0;(pos=name.find("''",pos))!=std::string::npos;++pos)name.erase(pos,1);if(name.find('"')==std::string::npos)return "=SHEET(\""+name+"\",\""+m[3].str()+"\")";}
    return source;
}
bool supported_formula(const std::string& source){
    auto native=native_formula(source);auto parsed=parse_formula(native,{});if(parsed.error)return false;
    for(const auto& n:parsed.nodes)if(n.kind==NodeKind::Call&&n.op!="SUM"&&n.op!="AVERAGE"&&n.op!="IF"&&n.op!="SHEET")return false;
    if(native.find("SHEET")!=std::string::npos)return excel_formula(native)!=native;
    return native.find_first_of("![]") == std::string::npos;
}

}
XlsxImport read_part(Zip& zip,bool formulas,unsigned sheet_index){
    std::uint64_t expanded=0;std::set<std::string> names;const auto entries=mz_zip_reader_get_num_files(&zip.zip);
    if(entries>2048)throw std::runtime_error("Too many XLSX ZIP parts.");
    for(unsigned i=0;i<entries;++i){mz_zip_archive_file_stat s{};if(!mz_zip_reader_file_stat(&zip.zip,i,&s)||s.m_is_encrypted)throw std::runtime_error("Encrypted XLSX is unsupported.");expanded+=s.m_uncomp_size;if(expanded>max_zip)throw std::runtime_error("Expanded XLSX exceeds 64 MiB.");if(!names.insert(s.m_filename).second)throw std::runtime_error("Duplicate XLSX ZIP part.");}
    pugi::xml_document workbook,rels;xml(workbook,zip.get("xl/workbook.xml"));xml(rels,zip.get("xl/_rels/workbook.xml.rels"));
    auto sheets=child(workbook.document_element(),"sheets");auto first=sheets.first_child();for(unsigned i=0;i<sheet_index&&first;++i)first=first.next_sibling();if(!first)throw std::runtime_error("No worksheet found.");
    std::string id;for(auto a:first.attributes())if(std::string_view(local(a.name()))=="id")id=a.value();
    std::string target;for(auto r:rels.document_element().children())if(id==r.attribute("Id").value()){if(std::string_view(r.attribute("TargetMode").value())=="External")throw std::runtime_error("External sheets are unsupported.");target=r.attribute("Target").value();}
    if(target.empty())throw std::runtime_error("Worksheet relationship missing.");
    if(target.front()=='/')target.erase(0,1);else target="xl/"+target;
    if(target.find("..")!=std::string::npos||target.find('\\')!=std::string::npos||target.find(':')!=std::string::npos)throw std::runtime_error("Unsupported worksheet path.");
    std::vector<std::string> strings;auto shared=zip.get("xl/sharedStrings.xml",false);if(!shared.empty()){pugi::xml_document doc;xml(doc,shared);for(auto item:doc.document_element().children()){if(strings.size()>=100'000)throw std::runtime_error("Too many shared strings.");strings.push_back(content(item));}}
    std::vector<Style> styles(1);std::vector<bool> dates(1),locks(1,true);auto style_xml=zip.get("xl/styles.xml",false);
    if(!style_xml.empty()){
        pugi::xml_document doc;xml(doc,style_xml);auto root=doc.document_element();std::vector<pugi::xml_node> fonts,fills,borders;std::map<unsigned,std::string> formats;
        for(auto n:child(root,"borders").children())borders.push_back(n);for(auto n:child(root,"numFmts").children())formats[n.attribute("numFmtId").as_uint()]=n.attribute("formatCode").value();
        for(auto n:child(root,"fonts").children())fonts.push_back(n);for(auto n:child(root,"fills").children())fills.push_back(n);
        styles.clear();dates.clear();locks.clear();for(auto xf:child(root,"cellXfs").children()){
            if(styles.size()>4096)throw std::runtime_error("Too many XLSX styles.");Style s;
            auto font=xf.attribute("fontId").as_uint(),fill=xf.attribute("fillId").as_uint(),num=xf.attribute("numFmtId").as_uint();
            if(font<fonts.size()){s.bold=bool(child(fonts[font],"b"));s.foreground=color(child(fonts[font],"color"),s.foreground);}
            if(fill<fills.size())s.background=color(child(child(fills[fill],"patternFill"),"fgColor"),s.background);
            if(font<fonts.size()) {s.font_size=std::uint8_t(std::clamp(child(fonts[font],"sz").attribute("val").as_int(16),8,36));std::string name=child(fonts[font],"name").attribute("val").value();s.font_family=name=="Consolas"?2:name=="Georgia"?1:0;}
            auto bid=xf.attribute("borderId").as_uint();if(bid<borders.size())for(auto edge:{"left","right","top","bottom"})if(*child(borders[bid],edge).attribute("style").value())s.border=true;
            std::string align=child(xf,"alignment").attribute("horizontal").value();s.alignment=align=="left"?1:align=="center"?2:align=="right"?3:0;
            if(num==1||num==2){s.number_format=1;s.decimals=num==1?0:2;}else if(num==3||num==4){s.number_format=5;s.decimals=num==3?0:2;}else if(num==9||num==10){s.number_format=3;s.decimals=num==9?0:2;}else if(num>=14&&num<=22)s.number_format=4;
            if(formats.contains(num)){auto code=formats[num];if(code.find("yy")!=std::string::npos||code.find("dd")!=std::string::npos)s.number_format=4;else {s.number_format=code.find('%')!=std::string::npos?3:code.find('$')!=std::string::npos?2:code.find(',')!=std::string::npos?5:1;auto dot=code.find('.');s.decimals=0;if(dot!=std::string::npos)for(auto i=dot+1;i<code.size()&&code[i]=='0'&&s.decimals<12;++i)++s.decimals;}}

            locks.push_back(!child(xf,"protection").attribute("locked")||child(xf,"protection").attribute("locked").as_bool());styles.push_back(s);dates.push_back((num>=14&&num<=22)||(num>=45&&num<=47)||num>=164);
        }if(styles.empty()){styles.push_back({});dates.push_back(false);locks.push_back(true);}
    }
    pugi::xml_document document;xml(document,zip.get(target));auto root=document.document_element();XlsxImport result;
    std::map<unsigned,std::vector<unsigned>> protected_cells;
    std::size_t cached=0,active=0,missing=0;std::set<CellCoord> seen;
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
            if(child(root,"sheetProtection").attribute("sheet").as_bool()&&locks[style])protected_cells[r].push_back(coord->column);
            std::string type=cell.attribute("t").value();auto v=child(cell,"v");auto f=child(cell,"f");Input value;
            if(f&&formulas&&!*f.attribute("t").value()&&supported_formula("="+std::string(f.text().as_string()))){value=FormulaInput{native_formula("="+std::string(f.text().as_string()))};++active;}
            else {
                if(f){++cached;if(!v)++missing;}
                if(type=="inlineStr")value=content(child(cell,"is"));
                else if(type=="s"){auto n=index(v.text().as_string());if(n>=strings.size())throw std::runtime_error("Invalid shared string index.");value=strings[n];}
                else if(type=="b"){std::string b=v.text().as_string();if(b!="0"&&b!="1")throw std::runtime_error("Invalid XLSX Boolean.");value=b=="1";}
                else if(type=="str"||type=="e"||type=="d")value=std::string(v.text().as_string());
                else if(v){std::string text=v.text().as_string();double n{};auto[end,e]=parse_double(text.data(),text.data()+text.size(),n);if(e!=std::errc{}||end!=text.data()+text.size()||!std::isfinite(n))throw std::runtime_error("Invalid XLSX number.");value=n;}
                else if(f)value=std::string("[Formula has no cached value]");
            }
            if(auto text=std::get_if<std::string>(&value);text&&!valid_utf8(*text))throw std::runtime_error("Invalid XLSX text.");
            result.data.cells.push_back({*coord,std::move(value)});
            if(styles[style]!=Style{})result.data.formats.push_back({*coord,styles[style]});
        }
        if(!inconsistent&&uniform&&*uniform!=Style{})result.data.rows.push_back({r,*uniform});
    }
    result.name=first.attribute("name").value();
    result.summary=trf("%s: %zu cells, %zu supported formulas, %zu cached formulas, %zu missing cached values.",result.name.c_str(),result.data.cells.size(),active,cached,missing);
    read_validation(root,result.data);
    for(const auto& [r,columns]:protected_cells){if(!result.data.rules)result.data.rules.emplace();
        for(std::size_t i=0;i<columns.size();){auto first=columns[i],last=first;while(i+1<columns.size()&&columns[i+1]==last+1)last=columns[++i];++i;ValidationRule rule;rule.first={r,first};rule.last={r,last};rule.locked=true;result.data.rules->push_back(rule);}
    }
    if(result.data.rules)for(const auto& rule:*result.data.rules)if(rule.kind==ValidationKind::Date)for(auto& e:result.data.cells)if(rule.contains(e.coord))if(auto n=std::get_if<double>(&e.input))e.input=serial_date(*n);

    return result;
}

XlsxWorkbook read_xlsx_workbook(const std::filesystem::path& path,bool formulas){
    Zip zip(path);pugi::xml_document doc;xml(doc,zip.get("xl/workbook.xml"));
    auto root=doc.document_element();if(child(root,"workbookPr").attribute("date1904").as_bool())throw std::runtime_error(tr("Excel workbooks using the 1904 date system are not supported."));
    XlsxWorkbook result;std::set<std::string> names;
    for(auto n:child(root,"sheets").children()){
        std::string name=n.attribute("name").value();if(result.sheets.size()>=64||name.empty()||name.size()>64||!names.insert(name).second)throw std::runtime_error(tr("Unsupported Excel worksheet names or sheet count."));
        result.sheets.push_back(read_part(zip,formulas,unsigned(result.sheets.size())));result.summary+=result.sheets.back().summary;result.summary+=char(10);
    }
    if(result.sheets.empty())throw std::runtime_error(tr("Unsupported Excel worksheet names or sheet count."));
    result.summary+=tr("Imports all worksheets and supported cell styles and rules. Charts, merged cells, dimensions and unsupported formulas are not preserved. Keep the original Excel file.");return result;
}
XlsxImport read_xlsx(const std::filesystem::path& path,bool formulas){return read_xlsx_workbook(path,formulas).sheets.front();}
std::string write_xlsx(const std::filesystem::path& path,const Sheet& sheet){return write_xlsx_workbook(path,{{"Sheet 1",&sheet}});}
std::string write_xlsx_workbook(const std::filesystem::path& path,const std::vector<XlsxSheet>& sheets){
    if(sheets.empty()||sheets.size()>64)throw std::runtime_error(tr("Unsupported Excel worksheet names or sheet count."));
    std::set<std::string> names;for(const auto& s:sheets){unsigned units=0;std::string key=s.name;for(unsigned char c:s.name)if((c&192)!=128)units+=c>=240?2:1;for(char& c:key)if(c>='A'&&c<='Z')c+=32;
        if(!s.sheet||s.name.empty()||units>31||s.name.find_first_of("[]:*?/\\")!=std::string::npos||s.name.front()=='\''||s.name.back()=='\''||!names.insert(key).second)throw std::runtime_error(tr("Excel sheet names must be unique and at most 31 characters."));}
    Zip zip;using Entry=std::pair<Style,bool>;std::vector<Entry> styles{{Style{},false}};
    auto style_id=[&](Style style,bool locked){Entry entry{style,locked};auto it=std::find(styles.begin(),styles.end(),entry);if(it==styles.end()){if(styles.size()>=4096)throw std::runtime_error("Too many Excel styles.");styles.push_back(entry);return unsigned(styles.size()-1);}return unsigned(it-styles.begin());};
    pugi::xml_document workbook,rels,types;auto wb=workbook.append_child("workbook");wb.append_attribute("xmlns")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";wb.append_attribute("xmlns:r")="http://schemas.openxmlformats.org/officeDocument/2006/relationships";
    auto ws=wb.append_child("sheets");auto rr=rels.append_child("Relationships");rr.append_attribute("xmlns")="http://schemas.openxmlformats.org/package/2006/relationships";
    auto ct=types.append_child("Types");ct.append_attribute("xmlns")="http://schemas.openxmlformats.org/package/2006/content-types";
    auto def=ct.append_child("Default");def.append_attribute("Extension")="rels";def.append_attribute("ContentType")="application/vnd.openxmlformats-package.relationships+xml";def=ct.append_child("Default");def.append_attribute("Extension")="xml";def.append_attribute("ContentType")="application/xml";
    auto part=[&](const std::string& name,const char* type){auto n=ct.append_child("Override");n.append_attribute("PartName")=name.c_str();n.append_attribute("ContentType")=type;};
    part("/xl/workbook.xml","application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml");part("/xl/styles.xml","application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml");
    std::size_t flattened=0;
    for(unsigned si=0;si<sheets.size();++si){
        const auto& sheet=*sheets[si].sheet;auto path="worksheets/sheet"+std::to_string(si+1)+".xml",rid="rId"+std::to_string(si+1);
        auto sn=ws.append_child("sheet");sn.append_attribute("name")=sheets[si].name.c_str();sn.append_attribute("sheetId")=si+1;sn.append_attribute("r:id")=rid.c_str();
        auto rel=rr.append_child("Relationship");rel.append_attribute("Id")=rid.c_str();rel.append_attribute("Type")="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet";rel.append_attribute("Target")=path.c_str();part("/xl/"+path,"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml");
        pugi::xml_document doc;auto root=doc.append_child("worksheet");root.append_attribute("xmlns")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";
        std::map<unsigned,std::set<unsigned>> cells;std::set<CellCoord> locked,dates;
        for(const auto& [r,row]:sheet.populated_rows())for(const auto& [c,cell]:row)cells[r].insert(c);
        for(const auto& [c,style]:sheet.cell_styles())cells[c.row].insert(c.column);
        for(const auto& [r,style]:sheet.row_styles())cells[r];
        for(const auto& rule:sheet.validation_rules())if(rule.locked||rule.kind==ValidationKind::Date)for(unsigned r=rule.first.row;r<=rule.last.row;++r)for(unsigned c=rule.first.column;c<=rule.last.column;++c){if(rule.locked){locked.insert({r,c});cells[r].insert(c);}if(rule.kind==ValidationKind::Date)dates.insert({r,c});}
        auto data=root.append_child("sheetData");
        for(const auto& [r,columns]:cells){auto row=data.append_child("row");row.append_attribute("r")=r+1;
            if(sheet.row_styles().contains(r)){row.append_attribute("s")=style_id(sheet.row_style(r),false);row.append_attribute("customFormat")=1;}
            for(unsigned col:columns){CellCoord coord{r,col};auto c=row.append_child("c");c.append_attribute("r")=address(coord).c_str();auto style=sheet.cell_style(coord);if(dates.contains(coord))style.number_format=4;c.append_attribute("s")=style_id(style,locked.contains(coord));
                auto cell=sheet.cell(coord);if(!cell)continue;bool formula=false;Value value=cell->cached;
                if(auto f=std::get_if<FormulaInput>(&cell->input)){auto source=excel_formula(f->source);formula=supported_formula(source);if(formula)c.append_child("f").text().set(source.substr(1).c_str());else ++flattened;}
                if(dates.contains(coord))if(auto text=std::get_if<std::string>(&value);text&&!text->empty())value=double(date_serial(*text));
                if(auto n=std::get_if<double>(&value))c.append_child("v").text().set(decimal(*n).c_str());
                else if(auto b=std::get_if<bool>(&value)){c.append_attribute("t")="b";c.append_child("v").text().set(*b?"1":"0");}
                else if(std::holds_alternative<CellError>(value)){c.append_attribute("t")="e";c.append_child("v").text().set("#VALUE!");}
                else {auto text=display(value);if(!valid_utf8(text))throw std::runtime_error("Invalid Excel text.");if(formula){c.append_attribute("t")="str";c.append_child("v").text().set(text.c_str());}else{c.append_attribute("t")="inlineStr";auto t=c.append_child("is").append_child("t");t.append_attribute("xml:space")="preserve";t.text().set(text.c_str());}}
            }
        }
        if(!locked.empty()){auto protect=root.append_child("sheetProtection");protect.append_attribute("sheet")=1;protect.append_attribute("objects")=1;protect.append_attribute("scenarios")=1;}
        pugi::xml_node dvs;std::vector<const ValidationRule*> exported;
        for(const auto& rule:sheet.validation_rules()){
            if(rule.kind==ValidationKind::Any&&!rule.required&&!rule.unique)continue;
            for(const auto* other:exported)if(rule.first.row<=other->last.row&&rule.last.row>=other->first.row&&rule.first.column<=other->last.column&&rule.last.column>=other->first.column)throw std::runtime_error(tr("Excel export requires nonoverlapping validation ranges."));
            exported.push_back(&rule);if(!dvs)dvs=root.append_child("dataValidations");auto dv=dvs.append_child("dataValidation");dv.append_attribute("sqref")=range(rule.first,rule.last).c_str();dv.append_attribute("allowBlank")=!rule.required;dv.append_attribute("showErrorMessage")=1;dv.append_attribute("errorStyle")=rule.warning?"warning":"stop";
            std::string type="none",a,b;
            if(rule.unique&&rule.kind!=ValidationKind::Any)throw std::runtime_error(tr("Export uniqueness as a separate rule on its own range."));
            if(rule.kind==ValidationKind::Number||rule.kind==ValidationKind::WholeNumber){type=rule.kind==ValidationKind::Number?"decimal":"whole";a=decimal(rule.minimum);b=decimal(rule.maximum);dv.append_attribute("operator")="between";}
            else if(rule.kind==ValidationKind::Date){type="date";a=std::to_string(date_serial(rule.date_min));b=std::to_string(date_serial(rule.date_max));dv.append_attribute("operator")="between";}
            else if(rule.kind==ValidationKind::List){type="list";a="\"";for(const auto& choice:rule.choices){if(choice.find_first_of(",\"")!=std::string::npos)throw std::runtime_error(tr("Excel dropdown choices must fit 255 bytes and contain no commas or quotes."));if(a.size()>1)a+=',';a+=choice;}a+='"';if(a.size()>257)throw std::runtime_error(tr("Excel dropdown choices must fit 255 bytes and contain no commas or quotes."));}
            else if(rule.unique){type="custom";auto abs=[](CellCoord c){auto a=address(c);auto pos=a.find_first_of("0123456789");return "$"+a.substr(0,pos)+"$"+a.substr(pos);};a="COUNTIF("+abs(rule.first)+":"+abs(rule.last)+","+address(rule.first)+")<=1";}
            else if(rule.required){type="custom";a="LEN("+address(rule.first)+")>0";}
            dv.append_attribute("type")=type.c_str();if(!a.empty())dv.append_child("formula1").text().set(a.c_str());if(!b.empty())dv.append_child("formula2").text().set(b.c_str());
        }
        if(dvs)dvs.append_attribute("count")=unsigned(exported.size());zip.add(("xl/"+path).c_str(),serialized(doc));
    }
    pugi::xml_document style_doc;auto sr=style_doc.append_child("styleSheet");sr.append_attribute("xmlns")="http://schemas.openxmlformats.org/spreadsheetml/2006/main";
    auto formats=sr.append_child("numFmts");formats.append_attribute("count")=unsigned(styles.size());
    for(unsigned i=0;i<styles.size();++i){auto s=styles[i].first;std::string code=s.number_format==0?"General":s.number_format==4?"yyyy-mm-dd":(s.number_format==2?"$":"")+std::string(s.number_format==2||s.number_format==5?"#,##0":"0")+(s.decimals?"."+std::string(s.decimals,'0'):"")+(s.number_format==3?"%":"");auto n=formats.append_child("numFmt");n.append_attribute("numFmtId")=164+i;n.append_attribute("formatCode")=code.c_str();}
    auto fonts=sr.append_child("fonts"),fills=sr.append_child("fills"),borders=sr.append_child("borders");fonts.append_attribute("count")=unsigned(styles.size());fills.append_attribute("count")=unsigned(styles.size()+2);borders.append_attribute("count")=2;
    fills.append_child("fill").append_child("patternFill").append_attribute("patternType")="none";fills.append_child("fill").append_child("patternFill").append_attribute("patternType")="gray125";
    for(auto [s,locked]:styles){(void)locked;auto font=fonts.append_child("font");font.append_child("sz").append_attribute("val")=unsigned(s.font_size);font.append_child("name").append_attribute("val")=s.font_family==2?"Consolas":s.font_family==1?"Georgia":"Calibri";if(s.bold)font.append_child("b");font.append_child("color").append_attribute("rgb")=rgb(s.foreground).c_str();auto pattern=fills.append_child("fill").append_child("patternFill");pattern.append_attribute("patternType")="solid";pattern.append_child("fgColor").append_attribute("rgb")=rgb(s.background).c_str();}
    for(int i=0;i<2;++i){auto b=borders.append_child("border");for(auto edge:{"left","right","top","bottom","diagonal"}){auto e=b.append_child(edge);if(i&&std::string_view(edge)!="diagonal")e.append_attribute("style")="thin";}}
    auto base=sr.append_child("cellStyleXfs");base.append_attribute("count")=1;base.append_child("xf").append_attribute("numFmtId")=0;
    auto xfs=sr.append_child("cellXfs");xfs.append_attribute("count")=unsigned(styles.size());
    for(unsigned i=0;i<styles.size();++i){auto [s,locked]=styles[i];auto xf=xfs.append_child("xf");xf.append_attribute("fontId")=i;xf.append_attribute("fillId")=i+2;xf.append_attribute("borderId")=s.border?1:0;xf.append_attribute("xfId")=0;xf.append_attribute("numFmtId")=s.number_format?164+i:0;for(auto name:{"applyFont","applyFill","applyBorder","applyNumberFormat","applyAlignment","applyProtection"})xf.append_attribute(name)=1;auto align=xf.append_child("alignment");const char* aligns[]{"general","left","center","right"};align.append_attribute("horizontal")=aligns[s.alignment];xf.append_child("protection").append_attribute("locked")=locked;}
    auto named=sr.append_child("cellStyles");named.append_attribute("count")=1;auto normal=named.append_child("cellStyle");normal.append_attribute("name")="Normal";normal.append_attribute("xfId")=0;normal.append_attribute("builtinId")=0;
    zip.add("xl/styles.xml",serialized(style_doc));auto rel=rr.append_child("Relationship");rel.append_attribute("Id")="rStyles";rel.append_attribute("Type")="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles";rel.append_attribute("Target")="styles.xml";
    wb.append_child("calcPr").append_attribute("fullCalcOnLoad")=1;zip.add("xl/workbook.xml",serialized(workbook));zip.add("xl/_rels/workbook.xml.rels",serialized(rels));zip.add("[Content_Types].xml",serialized(types));
    zip.add("_rels/.rels",R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
    void* memory=nullptr;std::size_t size=0;if(!mz_zip_writer_finalize_heap_archive(&zip.zip,&memory,&size))throw std::runtime_error("Cannot finalize XLSX.");std::unique_ptr<void,decltype(&mz_free)> owned(memory,mz_free);if(size>max_zip)throw std::runtime_error("XLSX exceeds 64 MiB.");write_file_atomic(path,std::string_view(static_cast<char*>(memory),size));
    return trf("Exported %zu worksheets. %zu unsupported formulas became values. Keep a native workbook copy.",sheets.size(),flattened);
}
}

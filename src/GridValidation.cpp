#include "julretsu/GridUI.hpp"
#include "julretsu/Glyphs.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <stdexcept>
namespace julretsu {
bool GridUI::smoke_validation_import() {
    sheet_=Sheet({},&lua_);ValidationRule rule;rule.first=rule.last={0,0};rule.kind=ValidationKind::Number;rule.maximum=10;
    Batch b;b.rules=std::vector<ValidationRule>{rule};if(!sheet_.apply(b).accepted)return false;
    if(!sheet_.set({0,0},3.0).accepted)return false;
    Sheet good;if(!good.set({0,0},4.0).accepted)return false;
    auto imported=checked_import(good);if(imported.validation_rules()!=sheet_.validation_rules()||std::get<double>(imported.read({0,0}))!=4)return false;
    Sheet bad;if(!bad.set({0,0},11.0).accepted)return false;
    bool rejected=false;try{checked_import(bad);}catch(const std::exception&){rejected=true;}
    if(!rejected||std::get<double>(sheet_.read({0,0}))!=3)return false;
    rule.first=rule.last={1,0};rule.required=true;b.rules->push_back(rule);if(!sheet_.apply(b).accepted)return false;
    try{checked_import(good);return false;}catch(const std::exception&){}
    return true;
}
void GridUI::smoke_validation(bool show) {
    if(!show){validation_open_=false;return;}
    open_validation();validation_draft_.kind=ValidationKind::List;
    std::snprintf(validation_choices_.data(),validation_choices_.size(),"%s",tr("Ready"));
}
void GridUI::open_validation() {
    validation_open_=true; validation_draft_={};
    validation_draft_.first={std::min(active_.row,anchor_.row),std::min(active_.column,anchor_.column)};
    validation_draft_.last={std::max(active_.row,anchor_.row),std::max(active_.column,anchor_.column)};
    std::snprintf(validation_first_.data(),validation_first_.size(),"%s",std::get<std::string>(to_a1(validation_draft_.first)).c_str());
    std::snprintf(validation_last_.data(),validation_last_.size(),"%s",std::get<std::string>(to_a1(validation_draft_.last)).c_str());
    std::snprintf(validation_date_min_.data(),validation_date_min_.size(),"1900-01-01");
    std::snprintf(validation_date_max_.data(),validation_date_max_.size(),"2100-12-31");
    validation_choices_[0]=0;
}
Sheet GridUI::checked_import(Sheet incoming) {
    if(sheet_.validation_rules().empty())return incoming;
    Sheet candidate=sheet_; Batch b;
    for(const auto& [r,row]:sheet_.populated_rows())for(const auto& [c,cell]:row)b.cells.push_back({{r,c},Input{}});
    for(const auto& [r,row]:incoming.populated_rows())for(const auto& [c,cell]:row)b.cells.push_back({{r,c},cell.input});
    // Last write wins, but do not double the edit budget when replacing a large sheet.
    std::map<CellCoord,Input> final;for(auto& e:b.cells)final[e.coord]=std::move(e.input);b.cells.clear();
    for(auto& [c,input]:final)b.cells.push_back({c,std::move(input)});
    auto result=candidate.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);
    for(const auto& issue:candidate.validation_issues())if(!issue.warning)
        throw std::runtime_error(trf("%s: %s",std::get<std::string>(to_a1(issue.cell)).c_str(),tr(issue.reason.c_str())));
    return candidate;
}
void GridUI::draw_validation() {
    if(validation_revision_!=sheet_.revision()||validation_instance_!=sheet_.instance()) {
        validation_issues_=sheet_.validation_issues();validation_revision_=sheet_.revision();validation_instance_=sheet_.instance();
        if(std::any_of(validation_issues_.begin(),validation_issues_.end(),[](const auto& i){return i.warning;}))validation_open_=true;
    }
    if(!validation_open_)return;
    ImGui::SetNextWindowSize({720*scale_,std::min(820*scale_,ImGui::GetIO().DisplaySize.y-80*scale_)},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(tr("Validation and protection"),&validation_open_)) {ImGui::End();return;}
    if(ImGui::CollapsingHeader(tr("About these rules"))) {
    ImGui::TextWrapped("%s",tr("Rules are saved in .julretsu files. CSV and Excel exports do not preserve these rules."));
    ImGui::TextWrapped("%s",tr("Protection prevents accidental edits. Anyone with this file can remove rules."));
    ImGui::TextWrapped("%s",tr("Imports into this sheet retain its rules and formatting. Use a new workbook to import without these rules."));
    }
    for(const auto& rule:sheet_.validation_rules())if(rule.kind==ValidationKind::List&&rule.contains(active_)) {
        if(ImGui::BeginCombo(tr("Choose a value for the active cell"),display(sheet_.read(active_)).c_str())) {
            for(const auto& choice:rule.choices)if(ImGui::Selectable(choice.c_str())) {
                auto coord=active_;pending_=[this,coord,choice]{auto r=sheet_.set(coord,choice);if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;selection_changed_=true;};
            }
            ImGui::EndCombo();
        }
        break;
    }
    if(ImGui::CollapsingHeader(tr("Add a rule"),ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText(tr("First cell"),validation_first_.data(),validation_first_.size());
        ImGui::InputText(tr("Last cell"),validation_last_.data(),validation_last_.size());
        const char* kinds[]{"Any value","Number","Whole number","Date (YYYY-MM-DD)","Dropdown list"};
        if(ImGui::BeginCombo(tr("Allowed values"),tr(kinds[int(validation_draft_.kind)]))) {
            for(int i=0;i<5;++i)if(ImGui::Selectable(tr(kinds[i]),int(validation_draft_.kind)==i))validation_draft_.kind=ValidationKind(i);
            ImGui::EndCombo();
        }
        if(validation_draft_.kind==ValidationKind::Number||validation_draft_.kind==ValidationKind::WholeNumber) {
            ImGui::InputDouble(tr("Minimum"),&validation_draft_.minimum);ImGui::InputDouble(tr("Maximum"),&validation_draft_.maximum);
        }
        if(validation_draft_.kind==ValidationKind::Date) {
            ImGui::InputText(tr("Earliest date"),validation_date_min_.data(),validation_date_min_.size());
            ImGui::InputText(tr("Latest date"),validation_date_max_.data(),validation_date_max_.size());
        }
        if(validation_draft_.kind==ValidationKind::List) {
            ImGui::TextWrapped("%s",tr("Enter one choice per line. Up to 100 choices, 256 UTF-8 bytes each."));
            ImGui::InputTextMultiline("##choices",validation_choices_.data(),validation_choices_.size(),{-1,90*scale_});
        }
        ImGui::Checkbox(tr("Require a value"),&validation_draft_.required);
        ImGui::Checkbox(tr("Unique values within this range"),&validation_draft_.unique);
        ImGui::Checkbox(tr("Protect contents and formatting"),&validation_draft_.locked);
        ImGui::Checkbox(tr("Warn instead of rejecting invalid values"),&validation_draft_.warning);
        ImGui::TextWrapped("%s",tr("Warnings allow the edit and appear here. Protection always blocks edits."));
        if(ImGui::Button(tr("Add rule to range"))) {
            auto first=from_a1(validation_first_.data()),last=from_a1(validation_last_.data());
            auto a=std::get_if<CellCoord>(&first),z=std::get_if<CellCoord>(&last);
            auto rule=validation_draft_;rule.date_min=validation_date_min_.data();rule.date_max=validation_date_max_.data();
            std::istringstream lines(validation_choices_.data());std::string line;
            while(std::getline(lines,line)) {if(!line.empty()&&line.back()==char(13))line.pop_back();if(!line.empty())rule.choices.push_back(line);}
            if(a&&z) {rule.first=*a;rule.last=*z;}
            if(!a||!z||!valid_rule(rule))message_=tr("Invalid validation rule. Check the range, limits and choices.");
            else pending_=[this,rule]{Batch b;b.rules=sheet_.validation_rules();b.rules->push_back(rule);sheet_.label_next_change("Validation rules");auto r=sheet_.apply(b);if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;message_=tr("Validation rule saved. Existing problems are listed in the validation window.");};
        }
    }
    ImGui::SeparatorText(tr("Saved rules"));
    for(std::size_t i=0;i<sheet_.validation_rules().size();++i) {
        const auto& r=sheet_.validation_rules()[i];ImGui::PushID(int(i));
        ImGui::Text("%s : %s",std::get<std::string>(to_a1(r.first)).c_str(),std::get<std::string>(to_a1(r.last)).c_str());
        ImGui::SameLine();if(ImGui::SmallButton(tr("Remove rule")))pending_=[this,i]{Batch b;b.rules=sheet_.validation_rules();if(i<b.rules->size())b.rules->erase(b.rules->begin()+i);auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;};
        const char* kinds[]{"Any value","Number","Whole number","Date (YYYY-MM-DD)","Dropdown list"};
        ImGui::TextUnformatted(tr(kinds[int(r.kind)]));
        if(r.kind==ValidationKind::Number||r.kind==ValidationKind::WholeNumber)ImGui::Text("%.15g ... %.15g",r.minimum,r.maximum);
        if(r.kind==ValidationKind::Date)ImGui::Text("%s ... %s",r.date_min.c_str(),r.date_max.c_str());
        if(r.kind==ValidationKind::List)for(const auto& choice:r.choices){ImGui::Bullet();ImGui::SameLine();ImGui::TextUnformatted(choice.c_str());}
        if(r.required)ImGui::TextUnformatted(tr("Require a value"));
        if(r.unique)ImGui::TextUnformatted(tr("Unique values within this range"));
        if(r.locked)ImGui::TextUnformatted(tr("Protect contents and formatting"));
        if(r.warning)ImGui::TextUnformatted(tr("Warn instead of rejecting invalid values"));
        ImGui::Separator();ImGui::PopID();
    }
    ImGui::SeparatorText(tr("Validation results"));
    if(validation_issues_.empty())ImGui::TextUnformatted(tr("No validation problems found."));
    for(std::size_t i=0;i<std::min<std::size_t>(validation_issues_.size(),200);++i) {
        const auto& issue=validation_issues_[i];ImGui::PushID(int(i));
        if(ImGui::SmallButton(std::get<std::string>(to_a1(issue.cell)).c_str()))jump(issue.cell);
        ImGui::SameLine();ImGui::TextWrapped("%s: %s",tr(issue.warning?"Warning":"Invalid value"),tr(issue.reason.c_str()));ImGui::PopID();
    }
    if(validation_issues_.size()>200)ImGui::TextUnformatted(tr("Showing the first 200 problems."));
    ImGui::TextWrapped("%s",tr_text(message_).c_str());
    ImGui::End();
}
}

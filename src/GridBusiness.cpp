#include "julretsu/GridUI.hpp"
#include "julretsu/WorksheetOps.hpp"
#include <imgui.h>
#include <algorithm>
#include <stdexcept>
namespace julretsu {
void GridUI::show_restore_points(){restore_open_=true;restore_selected_.reset();restore_target_.clear();try{restore_points_=file_path_.empty()?std::vector<RestorePoint>{}:restore_points(file_path_);}catch(const std::exception& e){message_=e.what();}}
void GridUI::draw_business_tools(){
    if(restore_open_){ImGui::SetNextWindowSize({760*scale_,640*scale_},ImGuiCond_FirstUseEver);
        if(ImGui::Begin(tr("Restore points"),&restore_open_)){
            ImGui::TextWrapped("%s",tr("History is stored beside the workbook. Keep that history folder when moving the file. The last 20 automatic points and up to 100 named points are retained."));
            ImGui::InputText(tr("Checkpoint name"),checkpoint_name_.data(),checkpoint_name_.size());
            if(ImGui::Button(tr("Create restore point")))pending_=[this]{create_restore_point(file_path_,current_document(),checkpoint_name_[0]?checkpoint_name_.data():tr("Restore point"));show_restore_points();message_=tr("Restore point created.");};
            ImGui::BeginChild("points",{0,150*scale_},ImGuiChildFlags_Borders);
            for(std::size_t i=0;i<restore_points_.size();++i){const auto point=restore_points_[i];ImGui::PushID(int(i));
                if(ImGui::Selectable(point.label.c_str(),restore_selected_&&restore_selected_->path==point.path))pending_=[this,point]{auto target=read_document(point.path);auto baseline=current_document();auto diff=compare_documents(baseline,target);restore_target_=std::move(target);restore_baseline_=std::move(baseline);restore_difference_=std::move(diff);restore_selected_=point;};ImGui::PopID();}
            ImGui::EndChild();
            if(restore_selected_){ImGui::TextWrapped("%s",trf("%zu cell changes and %zu worksheet or formatting changes. Showing up to 100 details.",restore_difference_.cells,restore_difference_.metadata).c_str());
                ImGui::BeginChild("diff",{0,-80*scale_},ImGuiChildFlags_Borders);for(const auto& line:restore_difference_.details)ImGui::TextWrapped("%s",line.c_str());ImGui::EndChild();
                ImGui::TextWrapped("%s",tr("Restoring first creates a safety copy of your current work. Save afterward to replace the workbook file."));
                if(ImGui::Button(tr("Restore selected version")))pending_=[this]{
                    if(current_document()!=restore_baseline_)throw std::runtime_error(tr("The workbook changed. Select the restore point again to refresh its preview."));
                    const auto original=file_path_;const auto name=workbook_name_,label=file_label_;create_restore_point(original,current_document(),tr("Before restore"));
                    if(!open_from(restore_selected_->path))throw std::runtime_error(file_error_);file_path_=original;workbook_name_=name;file_label_=label;modified_=true;restore_open_=false;message_=tr("Version restored. Save to keep it, or restore the safety copy.");
                };
            }
            ImGui::TextWrapped("%s",tr_text(message_).c_str());
        }ImGui::End();
    }
    if(tables_open_){ImGui::SetNextWindowSize({630*scale_,480*scale_},ImGuiCond_FirstUseEver);
        if(ImGui::Begin(tr("Structured tables"),&tables_open_)){
            ImGui::TextWrapped("%s",tr("Select unique headers and at least one data row. New rows inherit formulas, formatting and rules. Tables with totals use Add table row; tables without totals also expand when you type directly below them."));
            ImGui::InputText(tr("Table name"),table_name_.data(),table_name_.size());ImGui::Checkbox(tr("Include a totals row"),&table_totals_);
            if(ImGui::Button(tr("Create table from selection"))){StructuredTable table;table.name=table_name_[0]?table_name_.data():"Table"+std::to_string(sheet_.tables().size()+1);table.first={std::min(active_.row,anchor_.row),std::min(active_.column,anchor_.column)};table.last={std::max(active_.row,anchor_.row),std::max(active_.column,anchor_.column)};table.totals=table_totals_;pending_=[this,table]{auto b=create_table(sheet_,table);sheet_.label_next_change("Create table");auto r=sheet_.apply(b);if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;};}
            ImGui::Separator();
            for(std::size_t i=0;i<sheet_.tables().size();++i){const auto& table=sheet_.tables()[i];ImGui::PushID(int(i));ImGui::Text("%s (%s:%s)",table.name.c_str(),std::get<std::string>(to_a1(table.first)).c_str(),std::get<std::string>(to_a1(table.last)).c_str());
                if(ImGui::Button(tr("Add table row")))pending_=[this,i]{auto b=append_table_row(sheet_,i);sheet_.label_next_change("Add table row");auto r=sheet_.apply(b);if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;select({sheet_.tables()[i].last.row,sheet_.tables()[i].first.column});};
                ImGui::SameLine();if(ImGui::Button(tr("Remove table definition")))pending_=[this,i]{Batch b;b.tables=sheet_.tables();b.tables->erase(b.tables->begin()+i);auto r=sheet_.apply(b);if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;};ImGui::PopID();
            }
            ImGui::TextWrapped("%s",tr("Removing a table definition keeps its cells, formatting and rules. Undo restores the definition."));
            ImGui::TextWrapped("%s",tr_text(message_).c_str());
        }ImGui::End();
    }
}
}

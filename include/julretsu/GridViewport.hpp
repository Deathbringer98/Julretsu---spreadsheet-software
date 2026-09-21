#pragma once
#include "Types.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace julretsu {
struct GridViewport {
    int first_row{}, first_column{};
    bool freeze_row=false,freeze_column=false;
    bool filtered=false;
    std::vector<unsigned> filtered_rows;
    unsigned row_at(int i) const {int index=freeze_row&&i==0?0:first_row+i;if(filtered)return index>=0&&index<int(filtered_rows.size())?filtered_rows[index]:max_rows;return unsigned(std::clamp(index,0,int(max_rows)));}
    unsigned column_at(int i) const {return unsigned(freeze_column&&i==0?0:first_column+i);}
    int screen_row(unsigned r) const {int index=int(r);if(filtered){auto it=std::find(filtered_rows.begin(),filtered_rows.end(),r);if(it==filtered_rows.end())return -1;index=int(it-filtered_rows.begin());}return freeze_row&&index==0?0:index-first_row;}
    int screen_column(unsigned c) const {return freeze_column&&c==0?0:int(c)-first_column;}
    int visible_rows=1, visible_columns=1;
    float row_height=30, column_width=140, row_header=56, column_header=30;
    void resize(float width,float height) {
        visible_rows=std::max(1,int(std::floor(std::max(0.0f,height-column_header)/row_height)));
        visible_columns=std::max(1,int(std::floor(std::max(0.0f,width-row_header)/column_width)));
        clamp();
    }
    void clamp() {
        first_row=std::clamp(first_row,0,std::max(0,(filtered?int(filtered_rows.size()):int(max_rows))-visible_rows));
        first_column=std::clamp(first_column,0,std::max(0,int(max_columns)-visible_columns));
    }
    [[nodiscard]] CellCoord hit(float x,float y) const {
        return {std::min(max_rows-1,row_at(std::max(0,int(std::floor((y-column_header)/row_height))))),
                std::min(max_columns-1,column_at(std::max(0,int(std::floor((x-row_header)/column_width)))))};
    }
    void reveal(CellCoord c) {
        if(filtered){auto it=std::find(filtered_rows.begin(),filtered_rows.end(),c.row);if(it==filtered_rows.end())return;c.row=unsigned(it-filtered_rows.begin());}
        if(freeze_row&&c.row==0)c.row=unsigned(first_row);
        if(freeze_column&&c.column==0)c.column=unsigned(first_column);
        if(int(c.row)<first_row) first_row=int(c.row);
        if(int(c.row)>=first_row+visible_rows-1) first_row=int(c.row)-std::max(0,visible_rows-2);
        if(int(c.column)<first_column) first_column=int(c.column);
        if(int(c.column)>=first_column+visible_columns-1) first_column=int(c.column)-std::max(0,visible_columns-2);
        clamp();
    }
};
}

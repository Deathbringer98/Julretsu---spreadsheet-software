#pragma once
#include "Types.hpp"
#include <algorithm>
#include <cmath>
namespace julretsu {
struct GridViewport {
    int first_row{}, first_column{};
    int visible_rows=1, visible_columns=1;
    float row_height=30, column_width=140, row_header=56, column_header=30;
    void resize(float width,float height) {
        visible_rows=std::max(1,int(std::floor(std::max(0.0f,height-column_header)/row_height)));
        visible_columns=std::max(1,int(std::floor(std::max(0.0f,width-row_header)/column_width)));
        clamp();
    }
    void clamp() {
        first_row=std::clamp(first_row,0,std::max(0,int(max_rows)-visible_rows));
        first_column=std::clamp(first_column,0,std::max(0,int(max_columns)-visible_columns));
    }
    [[nodiscard]] CellCoord hit(float x,float y) const {
        return {static_cast<std::uint32_t>(std::clamp(first_row+int(std::floor((y-column_header)/row_height)),0,int(max_rows)-1)),
                static_cast<std::uint32_t>(std::clamp(first_column+int(std::floor((x-row_header)/column_width)),0,int(max_columns)-1))};
    }
    void reveal(CellCoord c) {
        if(int(c.row)<first_row) first_row=int(c.row);
        if(int(c.row)>=first_row+visible_rows-1) first_row=int(c.row)-std::max(0,visible_rows-2);
        if(int(c.column)<first_column) first_column=int(c.column);
        if(int(c.column)>=first_column+visible_columns-1) first_column=int(c.column)-std::max(0,visible_columns-2);
        clamp();
    }
};
}

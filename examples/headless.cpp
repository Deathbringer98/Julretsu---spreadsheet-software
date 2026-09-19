#include "julretsu/GridCore.hpp"
#include <iostream>
int main() {
    using namespace julretsu;
    Sheet sheet;
    auto result=sheet.apply(Batch{{{{0,0},10.0},{{0,1},FormulaInput{"=A1*2"}}},{}});
    if(!result.accepted) { std::cerr<<result.error->context<<'\n'; return 1; }
    std::cout<<"Julretsu 0.1 | "<<max_rows<<" logical rows\nB1 = "<<display(sheet.read({0,1}))<<'\n';
    result=sheet.set({0,0},7.0);
    if(!result.accepted) return 1;
    std::cout<<"After A1 = 7, B1 = "<<display(sheet.read({0,1}))<<"\nPopulated cells: "<<sheet.populated_cells()<<'\n';
    return 0;
}

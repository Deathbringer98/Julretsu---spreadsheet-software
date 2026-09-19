#include "julretsu/GridCore.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace julretsu;
using Clock=std::chrono::steady_clock;
template<class F> void bench(const char* name,F action) {
    std::vector<double> times;
    for(int i=0;i<7;++i) {
        auto start=Clock::now(); action();
        times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
    }
    std::sort(times.begin(),times.end());
    std::cout<<name<<": p50_ms="<<times[3]<<" p95_ms="<<times[6]<<" samples=7\n";
}
int main() {
    Limits limits; limits.undo_depth=0; Sheet sheet(limits);
    Batch fixture;
    for(std::uint32_t i=0;i<10'000;++i) fixture.cells.push_back({{i*100,i%16},double(i)});
    if(!sheet.apply(fixture).accepted) return 1;
    std::cout<<"Julretsu benchmark | build="<<JULRETSU_BUILD_CONFIG
             <<" | logical_rows="<<max_rows<<" logical_columns="<<max_columns
             <<" populated_cells="<<sheet.populated_cells()<<" undo_depth=0\n";
    std::size_t count=0; double checksum=0;
    bench("borrowed_row_traversal",[&] {
        for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) {
            (void)r; (void)c; ++count; checksum+=std::get<double>(cell.cached);
        }
    });
    bench("100000_absent_reads",[&] {
        for(std::uint32_t i=0;i<100'000;++i) if(std::holds_alternative<std::monostate>(sheet.read({i,100}))) ++count;
    });
    Batch edits; for(std::uint32_t i=0;i<1000;++i) edits.cells.push_back({{i*100,i%16},double(i+1)});
    bench("1000_cell_batch_in_10000_populated_sheet",[&] { if(!sheet.apply(edits).accepted) throw std::runtime_error("Batch failed"); });
    Sheet chain(limits); Batch links; links.cells.push_back({{0,0},1.0});
    for(unsigned i=1;i<10'000;++i) links.cells.push_back({{i,0},FormulaInput{"=A"+std::to_string(i)+"+1"}});
    if(!chain.apply(links).accepted) return 1;
    double source=2;
    bench("10000_cell_chain_recalculation",[&] { auto r=chain.set({0,0},source++); if(!r.accepted||r.formulas_evaluated!=9999) throw std::runtime_error("Chain failed"); });
    std::cout<<"chain_populated_cells="<<chain.populated_cells()<<" chain_edges="<<chain.edge_count()
             <<" checksum="<<checksum<<" visits="<<count<<"\n"
             <<"Memory and allocation counts: not instrumented. UI frame time: not applicable (headless).\n";
}

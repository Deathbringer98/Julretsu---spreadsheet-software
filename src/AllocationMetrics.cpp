// Benchmark-only allocation interception for the native application.
// This deliberately counts allocation calls, not OS/driver internal allocations.
#include "julretsu/AllocationMetrics.hpp"
#include <cstdlib>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
namespace {
void* allocate(std::size_t size) {
    if(auto p=std::malloc(size?size:1)) {
        ++julretsu::metrics::cpp_allocations; julretsu::metrics::cpp_bytes+=size; return p;
    }
    throw std::bad_alloc{};
}
void* aligned_allocate(std::size_t size,std::size_t alignment) {
#ifdef _WIN32
    auto p=_aligned_malloc(size?size:1,alignment);
#else
    void* p=nullptr;
    if(posix_memalign(&p,alignment,size?size:1)!=0) p=nullptr;
#endif
    if(!p) throw std::bad_alloc{};
    ++julretsu::metrics::cpp_allocations; julretsu::metrics::cpp_bytes+=size; return p;
}
void aligned_free(void* p) noexcept {
#ifdef _WIN32
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n,std::align_val_t a) { return aligned_allocate(n,std::size_t(a)); }
void* operator new[](std::size_t n,std::align_val_t a) { return aligned_allocate(n,std::size_t(a)); }
void operator delete(void* p,std::align_val_t) noexcept { aligned_free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { aligned_free(p); }
void operator delete(void* p,std::size_t,std::align_val_t) noexcept { aligned_free(p); }
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept { aligned_free(p); }

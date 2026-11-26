#include "AllocationProbe.hpp"
#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace {
thread_local bool watching=false;
thread_local cyberator::test::AllocationCounts counts{};
void* allocate(std::size_t n){
    if(watching)++counts.allocations;
    if(auto* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void* allocateAligned(std::size_t n,std::size_t alignment){
    if(watching)++counts.allocations;
#if defined(_WIN32)
    if(auto* p=_aligned_malloc(n?n:1,alignment))return p;
#else
    void* p=nullptr;
    if(posix_memalign(&p,alignment,n?n:1)==0)return p;
#endif
    throw std::bad_alloc();
}
void release(void* p)noexcept{if(p&&watching)++counts.frees;std::free(p);}
void releaseAligned(void* p)noexcept{
    if(p&&watching)++counts.frees;
#if defined(_WIN32)
    _aligned_free(p);
#else
    std::free(p);
#endif
}
}
namespace cyberator::test {
AllocationProbe::AllocationProbe()noexcept{if(watching)std::abort();counts={};watching=true;}
AllocationProbe::~AllocationProbe()noexcept{if(active_)watching=false;}
AllocationCounts AllocationProbe::finish()noexcept{watching=false;active_=false;return counts;}
}
void* operator new(std::size_t n){return allocate(n);}
void* operator new[](std::size_t n){return allocate(n);}
void* operator new(std::size_t n,const std::nothrow_t&)noexcept{try{return allocate(n);}catch(...){return nullptr;}}
void* operator new[](std::size_t n,const std::nothrow_t&)noexcept{try{return allocate(n);}catch(...){return nullptr;}}
void* operator new(std::size_t n,std::align_val_t a){return allocateAligned(n,std::size_t(a));}
void* operator new[](std::size_t n,std::align_val_t a){return allocateAligned(n,std::size_t(a));}
void* operator new(std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return allocateAligned(n,std::size_t(a));}catch(...){return nullptr;}}
void* operator new[](std::size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return allocateAligned(n,std::size_t(a));}catch(...){return nullptr;}}
void operator delete(void* p)noexcept{release(p);}
void operator delete[](void* p)noexcept{release(p);}
void operator delete(void* p,std::size_t)noexcept{release(p);}
void operator delete[](void* p,std::size_t)noexcept{release(p);}
void operator delete(void* p,const std::nothrow_t&)noexcept{release(p);}
void operator delete[](void* p,const std::nothrow_t&)noexcept{release(p);}
void operator delete(void* p,std::align_val_t)noexcept{releaseAligned(p);}
void operator delete[](void* p,std::align_val_t)noexcept{releaseAligned(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{releaseAligned(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{releaseAligned(p);}
void operator delete(void* p,std::align_val_t,const std::nothrow_t&)noexcept{releaseAligned(p);}
void operator delete[](void* p,std::align_val_t,const std::nothrow_t&)noexcept{releaseAligned(p);}

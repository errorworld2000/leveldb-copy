#include "util/arena.h"

#include "gtest/gtest.h"
#include "util/random.h"
namespace leveldb {

    TEST(ArenaTest,Empty){Arena arena;}
    TEST(ArenaTest,Simple){
        std::vector<std::pair<size_t,char*>> allocated;
        Arena arena;
        const int N=100000;
        size_t bytes=0;
        Random rnd(301);
        for(int i=0;i<N;i++){
            size_t s = (i % (N / 10) == 0) 
             ? i 
             : (rnd.OneIn(4000) 
                ? rnd.Uniform(6000) 
                : (rnd.OneIn(10) ? rnd.Uniform(100) : rnd.Uniform(20)));
                if(s==0)s=1;
            char* r;
            if(rnd.OneIn(10)){
                r=arena.AllocateAligned(s);
            }else{
                r=arena.Allocate(s);
            }
            for(size_t b=0;b<s;b++){
                r[b]=i%256;
            }
            bytes+=s;
            allocated.push_back({s,r});
            ASSERT_GE(arena.MemoryUsage(),bytes);
            if(i>N/10) {
      ASSERT_LE(arena.MemoryUsage(), bytes * 1.10);
    }
  }
  for(size_t i=0;i<allocated.size();i++){
    size_t num_bytes=allocated[i].first;
    const char* p=allocated[i].second;
    for(size_t b=0;b<num_bytes;b++){
      ASSERT_EQ(int(p[b]) & 0xff, i % 256);
    }
  }
    }
  
}
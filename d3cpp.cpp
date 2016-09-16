#include "data_visualizer.h"
#include <string>

// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
struct TVisual {
  int x0;
  int y0;
  float k;
  TVisual() : x0(100), y0(0), k( -1.f) {}
  void destroy() {
    printf("  Destroying visual %dx%d %f\n", x0, y0, k);
    x0 = y0 = -1;
  }
  void set(uint32_t prop_id, float new_k) {
    k = new_k;
  }
  float get(uint32_t prop_id) {
    return k;
  }
};

struct TUserData {
  const char* name;
  int         key;
  bool operator == (const TUserData& other) const {
    return key == other.key;
  }
  bool operator < (const TUserData& other) const {
    return strcmp(name, other.name) < 0;
  }
};

void dump(const TUserData& s, const TVisual& v) {
  printf("  dump   : %-16s %dx%d %1.3f\n", s.name, v.x0, v.y0, v.k);
};

template< class CVisualizer >
void dumpAll(const char* title, CVisualizer& d) {
  assert(d.isValid());
  printf("%s\n", title);
  d.enter().each([](auto s, auto v) {
    printf("  enter  : %-16s %dx%d %1.3f\n", s.name, v.x0, v.y0, v.k);
  });
  d.updated().each([](auto s, auto v) {
    printf("  updated: %-16s %dx%d %1.3f\n", s.name, v.x0, v.y0, v.k);
  });
  d.exit().each([](auto s, auto v) {
    printf("  exit   : %-16s %dx%d %1.3f\n", s.name, v.x0, v.y0, v.k);
  });
}

int main()
{

  std::vector< TUserData > names;

  CDataVisualizer< TUserData, TVisual > d;

  for (int i = 0; i < 2; ++i) {
    printf("Changing data..... %d\n", i);
    if (i == 0) {

      names.push_back({ "laia", 10 });
      names.push_back({ "pau", 30 });
      names.push_back({ "helena", 20 });

    }
    else {

      names.push_back({ "lluc", 5 });
      names.erase(names.begin() + 1);

    }

    auto it = d.data(names);

    d.exit()
      .transition()
      .duration(0.5f)
      .set(0, [](auto d, auto idx) { return 0.f; })
      .remove()
      ;

    //d.enter().filter([](auto d, auto idx) { return (idx & 1) == 0; }).each(dump);
    d.enter().append([](auto d, auto idx) {
      TVisual v;
      v.x0 = idx;
      v.y0 = d.key;
      return v;
    }).set(0, [](auto d, auto idx) { return idx * 10.f; })
      .transition()
      .duration(0.5f)
      .ease(ease::linear)
    //.delay(0.5f)
      //.set(0, [](auto d, auto idx) { return idx * 20.f; })
      ;

    auto all = d.enter().merge(d.updated());
    all.sort().each(dump);
    
    printf("Sort with a custom sort fn\n");
    all.sort([](auto a, auto b) {
      return strcmp(a.name, b.name) > 0;
    }).each(dump);

    if( i == 0 ) 
      dumpAll("After Initial binding", d);
    else
      dumpAll("After pushing lluc and removing pau", d);

    for (int i = 0; i < 6; ++i) {
      d.update(0.1f);
      printf("%d After updating 0.1f -> %f\n", i, d.currentTime());
      dumpAll("", d);
    }
  }

  return 0;
}


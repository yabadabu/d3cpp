#include <cstdio>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <vector>

#include <string>

// ----------------------------------------
template< typename TUserData, typename TVisualData >
class CDataVisualizer {

  typedef std::vector< TUserData >   TUserDataContainer;
  typedef std::vector< TVisualData > TVisualDataContainer;
  typedef uint32_t TIndex;
  static const TIndex invalid_idx = ~0;

  typedef std::vector< TIndex >      TVisualizedDataContainer;

  // A prop of type binded to a user data.orop_id
  template< typename TPropValueType >
  struct TPropValue {
    TIndex          user_data_idx;     // index in TUserDataContainer & TVisualDataContainer
    uint32_t        prop_id;           // color, pos, direction, ... the id of the attribute
    TPropValueType  prop_value;
  };

  // Define a function prototype that we must implement later
  template< typename TPropType >      
  std::vector< TPropValue< TPropType > >& getProps();

  // This define the container and the specialized member function returning the container
#define DECL_PROP_TYPE(atype)   std::vector< TPropValue< atype > > vals_ ## atype; \
  template<>                                                                       \
  std::vector< TPropValue< atype > >& getProps() { return vals_ ## atype; }

  DECL_PROP_TYPE(float);
  DECL_PROP_TYPE(int);

  // To be able to use the macro hack with types with spaces... (ugly)
  typedef const char* const_char_ptr;
  DECL_PROP_TYPE(const_char_ptr);

public:

  // -----------------------------------------
  class CSelection {
    
    friend class CDataVisualizer;
    CDataVisualizer*           dv;
    TVisualizedDataContainer   data;

  public:

    TIndex size() const { return data.size(); }
    bool empty() const { return data.empty(); }

    template< typename TFn >
    void each( TFn fn ) const {
      assert(this->dv || data.empty() );
      for (auto d : data) {
        fn(dv->all_user_data[d], dv->all_visual_data[d]);
      }
    }

    // ----------------------------------------------------------------------
    // Generates a new selection which just those items where the user data
    // passes the filter 
    template< typename TFn >
    CSelection filter(TFn filter ) const {
      assert(this->dv || data.empty());
      CSelection new_sel;
      TIndex idx = 0;
      for (auto d : data) {
        if (filter(dv->all_user_data[d], idx))
          new_sel.data.push_back(d);
        ++idx;
      }
      new_sel.dv = this->dv;
      return new_sel;
    }

    // ----------------------------------------------------------------------
    CSelection merge(const CSelection& other) const {
      assert(this->dv == other.dv);
      CSelection new_sel;

      std::merge( data.begin(), data.end()
                , other.data.begin(), other.data.end()
                , std::back_inserter(new_sel.data)
                );

      new_sel.dv = this->dv;
      return new_sel;
    }

    // ----------------------------------------------------------------------
    // Sort selection view using the default less operator unless a custom function is given
    template< typename TFn = std::less<TUserData>>
    CSelection sort(TFn sorter = TFn()) const {
      assert(this->dv);
      CSelection new_sel(*this);

      // Use the sort algorithm
      std::sort(new_sel.data.begin(), new_sel.data.end(), [this, sorter](TIndex a, TIndex b) {
        return sorter(dv->all_user_data[a], dv->all_user_data[b]);
      });

      new_sel.dv = this->dv;
      return new_sel;
    }

    // ----------------------------------------------------------------------
    template< typename TFn >
    CSelection append(TFn generator) const {
      assert(this->dv || data.empty());
      TIndex idx = 0;
      for (auto d : data) {
        dv->all_visual_data[d] = generator(dv->all_user_data[d], idx);
        ++idx;
      }
      return *this;
    }

    // ----------------------------------------------------------------------
    CSelection remove() const {
      for (auto d : data) 
        dv->all_visual_data[ d ].destroy();
      return *this;
    }

    // -----------------------------------------------------------------
    // Register that 
    //     user_data  d
    //     will have  prop_id
    //     of type    TPropType
    //     has value  v
    // I need this in case we want to transition...
    template< typename TFn >
    const CSelection& set(uint32_t prop_id, TFn prop_value_provider ) const {

      typedef typename decltype(prop_value_provider(data[0], 0)) TPropType;

      auto& props_container = dv->getProps< TPropType >();

      TPropValue< TPropType > p;
      p.prop_id = prop_id;

      TIndex idx = 0;
      for (auto d : data) {
        p.user_data_idx = d;
        p.prop_value = prop_value_provider(d, idx);
        props_container.push_back(p);

        ++idx;
      }
      return *this;
    }

  };

  // https://medium.com/@mbostock/what-makes-software-good-943557f8a488#.dgmv8u19d
  CSelection& data( TUserDataContainer& new_data ) {

    // By default all exit    
    s_exit = s_all;
    s_enter.data.clear();
    s_updated.data.clear();
    s_all.data.clear();

    // For each input user_data 
    for( auto nd : new_data ) {

      // Find user_data_idx for nd;
      auto nd_iter = std::find(all_user_data.begin(), all_user_data.end(), nd);
      TIndex data_idx = invalid_idx;

      // If it does not exists in old_entries...
      if (nd_iter == all_user_data.end()) {
        
        // Register the new user data
        data_idx = (TIndex) all_user_data.size();
        all_user_data.push_back(nd);
        all_visual_data.resize(all_visual_data.size() + 1); 
        
        // The new entry is entering the data_set
        s_enter.data.push_back(data_idx);
      }
      else {
        // If exists in old_entries
        data_idx = (TIndex) std::distance(all_user_data.begin(), nd_iter);
      
        s_updated.data.push_back(data_idx);
        
        // Remove from the s_exit those entries already found. This will leave in s_exit
        // those which are no longer in the data_set
        auto it_exit = std::find(s_exit.data.begin(), s_exit.data.end(), data_idx);
        assert(it_exit != s_exit.data.end());
        s_exit.data.erase(it_exit);
      }
      
      s_all.data.push_back(data_idx);
    }

    // At this point
    //   exit contains the elems that were there but no longer are
    //   enter contains the elems that were not there but now are
    //   update contains the elems that were there and are

    return s_updated;
  }

  CSelection& exit() { return s_exit; }
  CSelection& enter() { return s_enter; }
  CSelection& updated() { return s_updated; }

  CDataVisualizer() {
    s_updated.dv = this;
    s_enter.dv = this;
    s_exit.dv = this;
    s_all.dv = this;
  }

  bool isValid() const {
    return s_updated.dv == this
      && s_enter.dv == this
      && s_exit.dv == this
      && s_all.dv == this
      ;
    return true;
  }

private:

  CSelection                s_updated;
  CSelection                s_enter;
  CSelection                s_exit;
  CSelection                s_all;

  TUserDataContainer        all_user_data;
  TVisualDataContainer      all_visual_data;

  friend class CSelection;

};

// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
struct TVisual {
  int x0;
  int y0;
  TVisual() : x0(100), y0(0) {}
  void destroy() {
    x0 = y0 = -1;
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
  printf("  dump   : %-16s %dx%d\n", s.name, v.x0, v.y0);
};

template< class CVisualizer >
void dumpAll(const char* title, CVisualizer& d) {
  assert(d.isValid());
  printf("%s\n", title);
  d.enter().each([](auto s, auto v) {
    printf("  enter  : %-16s %dx%d\n", s.name, v.x0, v.y0);
  });
  d.updated().each([](auto s, auto v) {
    printf("  updated: %-16s %dx%d\n", s.name, v.x0, v.y0);
  });
  d.exit().each([](auto s, auto v) {
    printf("  exit   : %-16s %dx%d\n", s.name, v.x0, v.y0);
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

    d.exit().remove();

    //d.enter().filter([](auto d, auto idx) { return (idx & 1) == 0; }).each(dump);
    d.enter().append([](auto d, auto idx) {
      TVisual v;
      v.x0 = idx;
      v.y0 = d.key;
      return v;
    }).set(0, [](auto d, auto idx) { return idx * 10.f; });

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
  }

  return 0;
}


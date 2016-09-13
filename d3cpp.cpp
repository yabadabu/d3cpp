#include <cstdio>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <vector>

#include <string>

namespace ease {

  typedef float(*TEaseFn)(float);

  float linear(float t) {
    return t;
  }
 
  float cubicIn(float t) {
    return t * t * t;
  }

  float cubicOut(float t) {
    return --t * t * t + 1;
  }

  // same as cubicInOut
  float cubic(float t) {
    return ((t *= 2) <= 1 ? t * t * t : (t -= 2) * t * t + 2) / 2;
  }

}

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
  struct TTweenValue {
    TIndex          user_data_idx;      // index in TUserDataContainer & TVisualDataContainer
    uint32_t        prop_id;            // color, pos, direction, ... the id of the attribute
    float           start_delay;        // When must start
    float           duration;           // How long will be
    ease::TEaseFn   ease_fn;            // Type of blending
    TPropValueType  value_t0;           // initial value
    TPropValueType  value_t1;           // final value
  };

  // Define a function template that we must implement later
  template< typename TPropType >
  std::vector< TTweenValue< TPropType > >& getTweens();

  // This define the container and the specialized member function returning the container
#define DECL_PROP_TYPE(atype)   \
  std::vector< TTweenValue< atype > > tweens_ ## atype; \
  template<> std::vector< TTweenValue< atype > >& getTweens() { return tweens_ ## atype; } \

  DECL_PROP_TYPE(float);    // This declared the member: vals_float
  DECL_PROP_TYPE(int);

  // To be able to use the macro hack with types with spaces... (ugly)
  typedef const char* const_char_ptr;
  DECL_PROP_TYPE(const_char_ptr);

  // Forward get/set property access to the visual data object
  template< typename TPropType >
  void setPropValue(TIndex user_data_idx, uint32_t prop_id, TPropType new_value) {
    all_visual_data[user_data_idx].set(prop_id, new_value);
  }
  template< typename TPropType >
  TPropType getPropValue(TIndex user_data_idx, uint32_t prop_id) {
    return all_visual_data[user_data_idx].get(prop_id);
  }

  template< typename TPropType >
  bool updateTweens(float dt) {
    int nactives = 0;
    auto& tweens = getTweens< TPropType >();
    for (auto& tw : tweens) {
      float unit_time = (current_time - tw.start_delay) / tw.duration;
      if (unit_time < 0.f)
        continue;
      if (unit_time < 1.f) {
        unit_time = tw.ease_fn(unit_time);
      }
      else {
        unit_time = 1.f;
        // disable this
        tw.start_delay = -1.f;
        // notify end of the transition
      }

      auto new_value = tw.value_t0 * (1.f - unit_time) + tw.value_t1 * (unit_time);

      setPropValue(tw.user_data_idx, tw.prop_id, new_value);
      
      ++nactives;
    }

    // Delete everything
    if (!nactives)
      tweens.clear();
    
    return nactives > 0;
  }

public:

  // -----------------------------------------
  class CSelection {
    
    friend class CDataVisualizer;
    friend class CTransition;
    CDataVisualizer*           dv;
    TVisualizedDataContainer   data;

  public:

    TIndex size() const { return (TIndex) data.size(); }
    bool empty() const { return data.empty(); }
    bool isValid() const { return dv != nullptr; }

    template< typename TFn >
    void each( TFn fn ) const {
      assert( isValid() );
      for (auto d : data) {
        fn(dv->all_user_data[d], dv->all_visual_data[d]);
      }
    }

    // ----------------------------------------------------------------------
    // Generates a new selection which just those items where the user data
    // passes the filter 
    template< typename TFn >
    CSelection filter(TFn filter ) const {
      assert(isValid());
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
      assert(isValid());
      assert(this->dv == other.dv);   // Both selection should be part of the same CDataVisualizer instance
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
    // Register the following:
    //     user_data   d
    //     at property prop_id
    //     has value   v
    // We need to store this in case we want to transition...
    template< typename TFn >
    const CSelection& set(uint32_t prop_id, TFn prop_value_provider ) const {

      // Get the type of the value returned by the provided function
      typedef typename decltype(prop_value_provider(data[0], 0)) TPropType;

      // All the registers entries will have the same prop_id
      TIndex idx = 0;
      for (auto d : data) {
        auto new_value = prop_value_provider(d, idx);
        dv->setPropValue<TPropType>(d, prop_id, new_value );
        ++idx;
      }
      return *this;
    }

    // -----------------------------------------------------------------
    // -----------------------------------------------------------------
    // -----------------------------------------------------------------
    class CTransition {
      const CSelection& selection;
      float             default_delay = 0.f;
      float             default_duration = 0.25f;
      ease::TEaseFn ease_fn = ease::cubic;

      friend class CSelection;

      // Applied in the selection order
      struct TTweenBaseParam {
        float     delay;
        float     duration;
      };
      std::vector< TTweenBaseParam > base_params;

      void alloc() {
        base_params.resize(selection.size());
        for (auto& e : base_params) {
          e.delay = default_delay;
          e.duration = default_duration;
        }
      }

      CTransition(const CSelection& new_selection) : selection( new_selection ) {
        alloc();
      }

    public:
      
      // Save delay for each element in the selection
      template< typename TFn >
      CTransition& delay(TFn fn) {
        const TUserDataContainer& udc = selection.dv->all_user_data;
        TIndex idx = 0;
        for (auto d : selection.data) {
          base_params[ idx ].delay = fn(udc[d], idx);
          ++idx;
        }
        return *this;
      }

      // Save duration for each element in the selection
      template< typename TFn >
      CTransition& duration(TFn fn) {
        const TUserDataContainer& udc = selection.dv->all_user_data;
        TIndex idx = 0;
        for (auto d : selection.data) {
          base_params[ idx ].duration = fn(udc[d], idx);
          assert(base_params[idx].duration > 0.f);
          ++idx;
        }
        return *this;
      }

      // This can't be configured per element
      CTransition& ease(ease::TEaseFn new_ease_fn) {
        ease_fn = new_ease_fn;
        return *this;
      }

      // 
      template< typename TFn >
      const CTransition& set(uint32_t prop_id, TFn prop_value_provider) const {
        // Get the type of the value returned by the provided function
        typedef typename decltype(prop_value_provider(data[0], 0)) TPropType;

        auto& tweens_container = selection.dv->getTweens< TPropType >();

        // Reserve N new tweens
        TIndex i0 = (TIndex)tweens_container.size();
        TIndex i1 = i0 + selection.size();
        tweens_container.resize( i1 );

        auto* tc = &tweens_container[i0];

        // Init all tweens in bulk
        TIndex idx = 0;
        for (auto d : selection.data) {
          tc->user_data_idx = d;
          tc->prop_id = prop_id;
          tc->start_delay = base_params[idx].delay;
          tc->duration = base_params[idx].duration;
          tc->ease_fn = ease_fn;
          tc->value_t0 = selection.dv->getPropValue<TPropType>(d, prop_id);
          tc->value_t1 = prop_value_provider(d, idx);
          ++tc;
          ++idx;
        }

        return *this;
      }

    };

    CTransition transition() const {
      return CTransition(*this);
    }

  };

  // -----------------------------------------------------------------------------
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

  CDataVisualizer() 
  : current_time( 0.f )
  {
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

  void update(float dt) {
    if (updateTweens<float>(dt))
      current_time += dt;
    else
      current_time = 0.f;
  }

private:

  CSelection                s_updated;
  CSelection                s_enter;
  CSelection                s_exit;
  CSelection                s_all;

  TUserDataContainer        all_user_data;
  TVisualDataContainer      all_visual_data;

  float                     current_time;

  friend class CSelection;

};


// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
struct TVisual {
  int x0;
  int y0;
  float k;
  TVisual() : x0(100), y0(0) {}
  void destroy() {
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
    }).set(0, [](auto d, auto idx) { return idx * 10.f; })
      .transition()
      .delay([](auto d, auto idx) { return 0.5f; })
      .set(0, [](auto d, auto idx) { return idx * 20.f; });

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

  d.update(0.f);

  return 0;
}


#ifndef INC_D3CPP_H_
#define INC_D3CPP_H_

#include <cstdio>
#include <cstdint>
#include <cassert>
#include <algorithm>
#include <iterator>
#include <vector>

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
  struct TTweenValue {
    TIndex          user_data_idx;      // index in TUserDataContainer & TVisualDataContainer
    uint32_t        prop_id;            // color, pos, direction, ... the id of the attribute
    bool            remove_on_end;      // Remove item when tween finishes?
    float           start_delay;        // When must start
    float           duration;           // How long will be
    ease::TEaseFn   ease_fn;            // Type of blending
    uint32_t        offset_to_data;
  };
  std::vector< TTweenValue > tweens;

  template< typename TPropValueType >
  struct TTweenData {
    TPropValueType  value_t0;           // initial value
    TPropValueType  value_t1;           // final value
  };
  std::vector< uint8_t >     tweens_data;

  // Forward get/set property access to the visual data object
  template< typename TPropType >
  void setPropValue(TIndex user_data_idx, uint32_t prop_id, TPropType new_value) {
    all_visual_data[user_data_idx].set(prop_id, new_value);
  }
  template< typename TPropType >
  TPropType getPropValue(TIndex user_data_idx, uint32_t prop_id) {
    return all_visual_data[user_data_idx].get(prop_id);
  }

  // -------------------------------------------------------
  bool updateTweens(float dt) {
    int nactives = 0;

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
        if (tw.remove_on_end) {
          // Should we at least render one time with the full blend?
          all_visual_data[tw.user_data_idx].destroy();
          continue;
        }
      }

      // Special case for floats
      TTweenData<float>* twd = reinterpret_cast< TTweenData<float> *>(&tweens_data[tw.offset_to_data]);
      auto new_value = twd->value_t0 * (1.f - unit_time) + twd->value_t1 * (unit_time);
      setPropValue<float>(tw.user_data_idx, tw.prop_id, new_value);

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

    TIndex size() const { return (TIndex)data.size(); }
    bool empty() const { return data.empty(); }
    bool isValid() const { return dv != nullptr; }

    template< typename TFn >
    void each(TFn fn) const {
      assert(isValid());
      for (auto d : data) {
        fn(dv->all_user_data[d], dv->all_visual_data[d]);
      }
    }

    // ----------------------------------------------------------------------
    // Generates a new selection which just those items where the user data
    // passes the filter 
    template< typename TFn >
    CSelection filter(TFn filter) const {
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

      if (data.empty())
        return other;
      else if (other.empty())
        return *this;
      else {
        std::merge(data.begin(), data.end()
          , other.data.begin(), other.data.end()
          , std::back_inserter(new_sel.data)
        );
      }

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
        dv->all_visual_data[d].destroy();
      return *this;
    }

    // -----------------------------------------------------------------
    // Register the following:
    //     user_data   d
    //     at property prop_id
    //     has value   v
    // We need to store this in case we want to transition...
    template< typename TFn >
    const CSelection& set(uint32_t prop_id, TFn prop_value_provider) const {

      // Get the type of the value returned by the provided function
      typedef typename decltype(prop_value_provider(data[0], 0)) TPropType;

      // All the registers entries will have the same prop_id
      TIndex idx = 0;
      for (auto d : data) {
        auto new_value = prop_value_provider(d, idx);
        dv->setPropValue<TPropType>(d, prop_id, new_value);
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
      bool              default_remove_on_end = false;
      TIndex            first_tween_idx = invalid_idx;
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
        duration(default_duration);
        delay(default_delay);
      }

      CTransition(const CSelection& new_selection) : selection(new_selection) {
        alloc();
      }

    public:

      // -----------------------------------------------------------
      // Save delay for each element in the selection
      template< typename TFn >
      CTransition& delay(TFn fn) {
        const TUserDataContainer& udc = selection.dv->all_user_data;
        TIndex idx = 0;
        for (auto d : selection.data) {
          base_params[idx].delay = fn(udc[d], idx);
          ++idx;
        }
        return *this;
      }

      // Cte delay for each element in the selection
      CTransition& delay(float new_constant_delay) {
        auto first = base_params.begin()
          , last = base_params.end();
        while (first != last) {
          first->delay = new_constant_delay;
          ++first;
        }
        return *this;
      }

      // -----------------------------------------------------------
      // Save duration for each element in the selection
      template< typename TFn >
      CTransition& duration(TFn fn) {
        const TUserDataContainer& udc = selection.dv->all_user_data;
        TIndex idx = 0;
        for (auto d : selection.data) {
          base_params[idx].duration = fn(udc[d], idx);
          assert(base_params[idx].duration > 0.f);
          ++idx;
        }
        return *this;
      }

      // Cte duration for each element in the selection
      CTransition& duration(float new_constant_duration) {
        auto first = base_params.begin()
          , last = base_params.end();
        while (first != last) {
          first->duration = new_constant_duration;
          ++first;
        }
        return *this;
      }

      // -----------------------------------------------------------
      // This can't be configured per element
      CTransition& ease(ease::TEaseFn new_ease_fn) {
        ease_fn = new_ease_fn;
        return *this;
      }

      // -----------------------------------------------------------
      CTransition& remove() {
        default_remove_on_end = true;

        // update the already registered tweens
        if (first_tween_idx != invalid_idx) {
          auto* tc = &selection.dv->tweens[first_tween_idx];
          for (TIndex n = selection.size(); n--; ++tc)
            tc->remove_on_end = true;
        }

        return *this;
      }

      // -----------------------------------------------------------
      template< typename TFn >
      CTransition& set(uint32_t prop_id, TFn prop_value_provider) {
        // Get the type of the value returned by the provided function
        typedef typename decltype(prop_value_provider(data[0], 0)) TPropType;

        auto& tweens_container = selection.dv->tweens;

        if (selection.empty())
          return *this;

        // Reserve N new tweens
        TIndex i0 = (TIndex)tweens_container.size();      // This is where we start
        TIndex i1 = i0 + selection.size();                // This is the final size
        tweens_container.resize(i1);                    // So, allocate them

        auto tc = tweens_container.begin() + i0;          // This is where we write our first tween

                                                          // Reserve space for N tween data
        auto data_bytes_per_tween = sizeof(TTweenData<TPropType>);
        auto& tweens_data = selection.dv->tweens_data;
        auto offset_to_data = tweens_data.size();
        tweens_data.resize(offset_to_data + data_bytes_per_tween * selection.data.size());
        TTweenData<TPropType>* addr = reinterpret_cast<TTweenData<TPropType> *>(&tweens_data[offset_to_data]);

        // Init all tweens in bulk
        TIndex idx = 0;
        for (auto d : selection.data) {
          tc->user_data_idx = d;
          tc->prop_id = prop_id;
          tc->start_delay = base_params[idx].delay;
          tc->duration = base_params[idx].duration;
          tc->ease_fn = ease_fn;
          tc->remove_on_end = default_remove_on_end;
          tc->offset_to_data = (TIndex)offset_to_data;
          ++tc;
          ++idx;

          assert((uint8_t*)addr < &tweens_data.back());
          addr->value_t0 = selection.dv->getPropValue<TPropType>(d, prop_id);
          addr->value_t1 = prop_value_provider(d, idx);
          addr++;
          offset_to_data += data_bytes_per_tween;
        }
        assert((uint8_t*)addr - &tweens_data[0] == tweens_data.size());

        if (first_tween_idx == invalid_idx)
          first_tween_idx = i0;

        return *this;
      }

    };

    CTransition transition() const {
      return CTransition(*this);
    }

  };

  // -----------------------------------------------------------------------------
  // https://medium.com/@mbostock/what-makes-software-good-943557f8a488#.dgmv8u19d
  CSelection& data(TUserDataContainer& new_data) {

    // Data:[         ] 
    // Data:[ 1 2 3   ] ->   Enter:[ 1 2 3   ]   Updated:[     ]  Exit:[         ]  All:[ 1 2 3   ]
    // Data:[   2 3 4 ] ->   Enter:[       4 ]   Updated:[ 2 3 ]  Exit:[ 1       ]  All:[ 1 2 3 4 ]
    // Data:[ 1 2     ] ->   Enter:[ 1       ]   Updated:[ 2   ]  Exit:[     3 4 ]  All:[ 1 2 3 4 ]

    // confirm new set is ordered
    std::sort(new_data.begin(), new_data.end());

    // By default all exit 
    s_exit = s_enter.merge( s_updated ).sort();
    s_enter.data.clear();
    s_updated.data.clear();

    for (auto& nd : new_data) {

      // Find user_data_idx for nd;
      auto nd_iter = std::find(all_user_data.begin(), all_user_data.end(), nd);
      TIndex data_idx = invalid_idx;

      // If it does not exists in old_entries... it means it's really new, never seen before
      if (nd_iter == all_user_data.end()) {

        // Register the new user data
        data_idx = (TIndex)all_user_data.size();
        all_user_data.push_back(nd);
        all_visual_data.resize(all_visual_data.size() + 1);

        // The new entry is entering the data_set
        s_enter.data.push_back(data_idx);
      }
      else {
        // We have seen this entry before. Find where
        data_idx = (TIndex)std::distance(all_user_data.begin(), nd_iter);

        // Update our copy with the updated data
        *nd_iter = nd;

        // Now in terms if is new or no
        auto it = std::find(s_exit.data.begin(), s_exit.data.end(), data_idx);
        if (it == s_exit.data.end()) {
          s_enter.data.push_back(data_idx);
        }
        else {
          s_updated.data.push_back(data_idx);
          s_exit.data.erase(it);
        }
      }

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
    : current_time(0.f)
  {
    s_updated.dv = this;
    s_enter.dv = this;
    s_exit.dv = this;
  }

  bool isValid() const {
    return s_updated.dv == this
      && s_enter.dv == this
      && s_exit.dv == this
      ;
    return true;
  }

  void update(float dt) {
    current_time += dt;
    if (!updateTweens(dt))
      current_time = 0.f;
  }

  float currentTime() const { return current_time; }

private:

  CSelection                s_updated;
  CSelection                s_enter;
  CSelection                s_exit;
  CSelection                s_current;
  
  TUserDataContainer        all_user_data;
  TVisualDataContainer      all_visual_data;

  float                     current_time;

  friend class CSelection;

};

#endif

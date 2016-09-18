#ifndef INC_TWEEN_FUNCTIONS_H_
#define INC_TWEEN_FUNCTIONS_H_

namespace tween {

  // Generic description on how to blend two elems of type TDataType
  template< typename TDataType >
  TDataType tweenData(float t, TDataType s, TDataType d);

  // Implementation for the floats
  template<>
  float tweenData<float>(float t, float s, float d) {
    return s * (1.f - t) + d * t;
  }

#ifdef INC_GEOMETRY_H_
  // Implementation for the VEC3
  template<>
  VEC3 tweenData<VEC3>(float t, VEC3 s, VEC3 d) {
    return s * (1.f - t) + d * t;
  }
#endif

}

#endif


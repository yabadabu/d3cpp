#ifndef INC_EASE_FUNCTIONS_H_
#define INC_EASE_FUNCTIONS_H_

#include <cassert>
#define _USE_MATH_DEFINES     // for win32
#include <math.h>

namespace ease {

  typedef float(*TEaseFn)(float);

  float linear(float t) {
    return t;
  }

  // ---------------------------------------------------------
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

  // ----------------------------------------------------------
  float bounceOut(float t) {
    const float b1 = 4.f / 11.f,
      b2 = 6.f / 11.f,
      b3 = 8.f / 11.f,
      b4 = 3.f / 4.f,
      b5 = 9.f / 11.f,
      b6 = 10.f / 11.f,
      b7 = 15.f / 16.f,
      b8 = 21.f / 22.f,
      b9 = 63.f / 64.f,
      b0 = 1.f / b1 / b1;
    return t < b1 ? b0 * t * t : t < b3 ? b0 * (t -= b2) * t + b4 : t < b6 ? b0 * (t -= b5) * t + b7 : b0 * (t -= b8) * t + b9;
  }

  float bounceIn(float t) {
    return 1 - bounceOut(1 - t);
  }

  float bounce(float t) {
    return ((t *= 2) <= 1 ? 1 - bounceOut(1 - t) : bounceOut(t - 1) + 1) / 2;
  }

  // -------------------------------------------------
  float elasticIn(float t) {
    const float b = 0.f;
    const float c = 1.f;
    const float d = 1.f;
    if (t == 0) return b;  if ((t /= d) == 1) return b + c;
    float p = d*.3f;
    float a = c;
    float s = p / 4;
    float postFix = a*powf(2, 10 * (t -= 1)); // this is a fix, again, with post-increment operators
    return -(postFix * sinf((t*d - s)*(2 * (float)M_PI) / p)) + b;
  }

  float elasticOut(float t) {
    const float b = 0.f;
    const float c = 1.f;
    const float d = 1.f;
    if (t == 0) return b;  if ((t /= d) == 1) return b + c;
    float p = d*.3f;
    float a = c;
    float s = p / 4;
    return (a*powf(2, -10 * t) * sinf((t*d - s)*(2 * (float)M_PI) / p) + c + b);
  }

  float elastic(float t) {
    const float b = 0.f;
    const float c = 1.f;
    const float d = 1.f;
    if (t == 0) return b;  if ((t /= d / 2) == 2) return b + c;
    float p = d*(.3f*1.5f);
    float a = c;
    float s = p / 4;

    if (t < 1) {
      float postFix = a*powf(2, 10 * (t -= 1)); // postIncrement is evil
      return -.5f*(postFix* sinf((t*d - s)*(2 * (float)M_PI) / p)) + b;
    }
    float postFix = a*powf(2, -10 * (t -= 1)); // postIncrement is evil
    return postFix * sinf((t*d - s)*(2 * (float)M_PI) / p)*.5f + c + b;
  }

  // ---------------------------------------------------------
  float backIn(float t) {
    float s = 1.70158f;
    return t*t*((s + 1)*t - s);
  }
  float backOut(float t) {
    float s = 1.70158f;
    return ((t = t - 1)*t*((s + 1)*t + s) + 1);
  }

  float back(float t) {
    float s = 1.70158f;
    if ((t /= 1.f / 2) < 1) return 1.f / 2 * (t*t*(((s *= (1.525f)) + 1)*t - s));
    float postFix = t -= 2;
    return 0.5f * ((postFix)*t*(((s *= (1.525f)) + 1)*t + s) + 2);
  }

  // https://github.com/d3/d3-ease
  // https://github.com/jesusgollonet/ofpennereasing

  enum eType {
    LINEAR = 0
    , CUBIC_IN
    , CUBIC_OUT
    , CUBIC
    , BOUNCE_IN
    , BOUNCE_OUT
    , BOUNCE
    , ELASIC_IN
    , ELASIC_OUT
    , ELASIC
    , BACK_IN
    , BACK_OUT
    , BACK
    , EASE_TYPES_COUNT
  };

  TEaseFn getFunc(uint32_t e_type) {
    assert(e_type < EASE_TYPES_COUNT);
    static TEaseFn funcs[EASE_TYPES_COUNT] = {
      &linear
      , &cubicIn, &cubicOut, &cubic
      , &bounceIn, &bounceOut, &bounce
      , &elasticIn, &elasticOut, &elastic
      , &backIn, &backOut, &back
    };
    return funcs[e_type];
  }
  const char* getName(uint32_t e_type) {
    assert(e_type < EASE_TYPES_COUNT);
    static const char* names[EASE_TYPES_COUNT] = {
      "Linear"
      , "Cubic In", "Cubic Out", "Cubic"
      , "Bounce In", "Bounce Out", "Bounce"
      , "Elastic In", "Elastic Out", "Elastic"
      , "Back In", "Back Out", "Back"
    };
    return names[e_type];
  }
}

#endif

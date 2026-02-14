#pragma once

namespace neurender {

/**
 * @brief Generates a number from the Halton sequence.
 * @param index The index of the sequence (e.g., frame count).
 * @param base The base of the sequence (e.g., 2 or 3).
 * @return A float between 0 and 1.
 */
inline float Halton(int index, int base) {
  float result = 0.0f;
  float f = 1.0f / (float)base;
  int i = index;
  while (i > 0) {
    result = result + f * (float)(i % base);
    i = (int)(i / base);
    f = f / (float)base;
  }
  return result;
}

} // namespace neurender

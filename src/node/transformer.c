#include "transformer.h"

void text_to_tensor(const char *text, float *tensor_data, int max_tensor_size) {
  int i = 0;

  // 1. Loop through the string and cast each character to a float
  while (text[i] != '\0' && i < max_tensor_size) {
    tensor_data[i] = (float)text[i];
    i++;
  }

  // 2. Pad the rest of the tensor with pure zeros (AI requires clean empty
  // space)
  while (i < max_tensor_size) {
    tensor_data[i] = 0.0f;
    i++;
  }
}

void tensor_to_text(const float *tensor_data, char *text, int max_text_size) {
  int i = 0;

  // 1. Loop through the tensor and cast floats back to characters
  // Stop if we hit a 0.0 (empty space) or the max buffer size
  while (i < (max_text_size - 1) && tensor_data[i] != 0.0f) {
    text[i] = (char)tensor_data[i];
    i++;
  }

  // 2. Cap the string securely!
  text[i] = '\0';
}

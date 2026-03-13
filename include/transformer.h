#ifndef TRANSFORMER_H
#define TRANSFORMER_H

// Transforms human English into a mathematical tensor array
void text_to_tensor(const char *text, float *tensor_data, int max_tensor_size);

// Transforms a mathematical tensor array back into human English
void tensor_to_text(const float *tensor_data, char *text, int max_text_size);

#endif

#include "font_metrics.h"

#include <iostream>

int main() {
  const std::string input = "Euro € — test";
  const std::string encoded = EncodeWinAnsi(input);
  if (encoded.empty()) {
    std::cerr << "Encoding returned empty output" << std::endl;
    return 1;
  }
  if (static_cast<unsigned char>(encoded[5]) != 0x80) {
    std::cerr << "Euro sign was not mapped to WinAnsi 0x80" << std::endl;
    return 1;
  }
  return 0;
}

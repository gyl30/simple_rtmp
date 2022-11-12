#include "buffer.h"

std::size_t buffer::size() const { return data_.size(); }
uint8_t* buffer::data() { return data_.data(); }
const uint8_t* buffer::data() const { return data_.data(); }
void buffer::erase(ssize_t pos, ssize_t size) { data_.erase(data_.begin() + pos, data_.begin() + pos + size); }
void buffer::clear() { data_.clear(); }

#include "reassembler.hh"
#include <map>
#include <string>
using namespace std;

void Reassembler::insert(uint64_t first_index, string data,
                         bool is_last_substring)
{
  // Process the situations where overlap, close happens.
  // byte stream: ouput_
  //
  const uint64_t first_unassembled_index{output_.writer().bytes_pushed()};
  const uint64_t first_unacceptable_index{first_unassembled_index + output_.writer().available_capacity()};

  if (is_last_substring)
  { // to record pos of eof
    eof_idx_ = first_index + data.size();
  }

  if (eof_idx_.has_value() && output_.writer().bytes_pushed() >= eof_idx_.value())
  {
    output_.writer().close();
    return;
  }

  // trim right
  if (first_index + data.size() > first_unacceptable_index)
  {
    if (first_index >= first_unacceptable_index)
      return;
    uint64_t keep_len = first_unacceptable_index - first_index; // A simple way to calculate how much size from first index byte to last acceptable byte
    data = data.substr(0, keep_len);
  }
  // trim left
  if (first_index < first_unassembled_index)
  {
    if (first_index + data.size() <= first_unassembled_index)
    {
      return;
    }
    uint64_t overlap = first_unassembled_index - first_index;
    data = data.substr(overlap);
    first_index = first_unassembled_index;
  }

  // check empty string
  if (data.empty())
  {
    return;
  }

  // at this point, first_unassembled <= the index of the incoming substring < first_unacceptable
  // if overlap with substring in buffer_, then ...
  // overlapping happens at left edge
  if (first_index > first_unassembled_index)
  {
    auto it = buffer_.lower_bound(first_index);

    // if (it != buffer_.end() && it->first == first_index) {
    //   if (it->second.size() >= data.size()) {
    //     return; // new data fully redundant; don't overwrite existing
    //   }
    //   buffer_.erase(it);              // new data is longer; replace existing
    //   it = buffer_.lower_bound(first_index);
    // }

    // trim left
    if (it != buffer_.begin())
    {
      auto prev_it = std::prev(it);
      uint64_t prev_end = prev_it->first + prev_it->second.size();

      if (prev_end > first_index)
      {
        if (prev_end >= first_index + data.size())
        {
          return;
        }
        uint64_t overlap_len = prev_end - first_index;
        data = data.substr(overlap_len);
        first_index = prev_end;
      }
    }
    it = buffer_.lower_bound(first_index); // first index may be modified from left trimming
    while (it != buffer_.end() && it->first < first_index + data.size())
    {
      uint64_t next_end = it->first + it->second.size();
      if (next_end <= first_index + data.size())
      {
        it = buffer_.erase(it);
      }
      else
      {
        uint64_t new_len = it->first - first_index;
        data = data.substr(0, new_len);
        break;
      }
    }
    if (data.empty())
    {
      return;
    }
    buffer_[first_index] = data;
  }
  else if (first_index == first_unassembled_index)
  {
    output_.writer().push(data);
    while (!buffer_.empty())
    {
      auto it = buffer_.begin();
      uint64_t current_unassembled = output_.writer().bytes_pushed();
      if (it->first <= current_unassembled)
      {
        uint64_t upper_bound = it->first + it->second.size();
        if (upper_bound > current_unassembled)
        {
          uint64_t offset = current_unassembled - it->first;
          output_.writer().push(it->second.substr(offset));
        }
        buffer_.erase(it);
      }
      else
      {
        break;
      }
    }
  }
  if (eof_idx_.has_value() && output_.writer().bytes_pushed() >= eof_idx_.value())
  {
    output_.writer().close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t cnt{0};
  for (const auto &it : buffer_)
  {
    cnt += it.second.size();
  }
  return cnt;
}
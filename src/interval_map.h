#pragma once

#include <algorithm>
#include <iostream>
#include <tuple>
#include <vector>

template <typename T>
struct IntervalMap {

  void set(int first, int last, T value) {
    // first put our key into the vector
    auto it = std::lower_bound(
        data.begin(), data.end(), first,
        [](const std::tuple<int, int, T> &a, const int &b) {
          return std::get<0>(a) < b;
        });
    it = data.insert(it, std::make_tuple(first, last, value));
    // then, determine if we need to shorten the previous interval
    auto prev = it;
    while (prev != data.begin()) {
      --prev;
      if (std::get<1>(*prev) > first) {
        if (std::get<1>(*prev) > last) {
          // if we're inserting in the middle, we need to create a new entry
          int tmp = std::get<1>(*prev);
          std::get<1>(*prev) = first;
          ++it;
          data.insert(it, std::make_tuple(last, tmp, std::get<2>(*prev)));
          return;
        } else {
          // otherwise we just shorten the prior
          std::get<1>(*prev) = first;
        }
      } else {
        break;
      }
    }
    // then, determine the number of following intervals that were overwritten
    auto next = it;
    ++next;
    while (next != data.end()) {
      if (std::get<1>(*next) <= last) {
        // we've entirely overwritten this one
        next = data.erase(next);
        --next;
      } else if (std::get<0>(*next) < last) {
        std::get<0>(*next) = last;
      }
      ++next;
    }
  }

  std::vector<T> get(int first, int last) {
    std::vector<T> result;
    for (auto &[f, l, v]: data) {
      if (!(last <= f) & !(first >= l)) {
        result.push_back(v);
      }
    }
    return result;
  }

  std::vector<std::tuple<int, int, T>> data;
};

template <typename T>
std::ostream &operator<<(std::ostream &out, IntervalMap<T> &im) {
  out << '[';
  for (auto &[first, last, value]: im.data) {
    out << '(' << first << ' ' << value << ' ' << last << ')';
  }
  out << ']';
  return out;
}

#pragma once

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <ctime>
#include <cassert>

#include <vector>
#include <utility>
#include <limits>

#include <array>
#include <tuple>
#include <type_traits>

#include <span>

#include "encode.hpp"
#include "data_structure.hpp"

namespace sfc_comp {

template <typename T, T Iden = std::numeric_limits<T>::min()>
struct range_max {
  using value_type = T;
  static T iden() { return Iden; }
  static T op(const value_type& l, const value_type& r) { return std::max<T>(l, r); }
};

template <typename T, T Iden = std::numeric_limits<T>::max()>
struct range_min {
  using value_type = T;
  static T iden() { return Iden; }
  static T op(const value_type& l, const value_type& r) { return std::min<T>(l, r); }
};

namespace encode {

struct lz_data {
  constexpr bool operator == (const lz_data& rhs) const = default;
  constexpr bool operator > (const lz_data& rhs) const {
    return len > rhs.len || (len == rhs.len && ofs > rhs.ofs);
  }
  size_t ofs;
  size_t len;
};

namespace lz {

constexpr encode::lz_data empty = encode::lz_data();

template <typename U, typename S = std::make_signed_t<U>>
requires std::integral<U>
encode::lz_data find_left(size_t adr, size_t i, size_t d, size_t min_len,
    std::span<const U> lcp_node, std::span<const S> ofs_node) {
  if (i == 0) return empty;
  const size_t width = lcp_node.size() / 2;
  const auto found = [&](size_t k) { return ofs_node[k] + ptrdiff_t(d) >= ptrdiff_t(adr); };
  U lcp = std::numeric_limits<U>::max();
  const auto quit = [&](size_t k) { return lcp_node[k] < lcp && (lcp = lcp_node[k]) < min_len; };

  size_t lo = i - 1, hi = i, k = lo + width;
  while (lo > 0 && !found(k)) {
    if (quit(k)) return empty;
    size_t diff = hi - lo;
    if (!(k & 1)) hi = lo, lo -= 2 * diff, k = (k >> 1) - 1;
    else lo -= diff, hi -= diff, --k;
  }
  if (lo == 0 && !found(k)) return empty;
  while (k < width) {
    size_t mi = (lo + hi) >> 1;
    if (found(2 * k + 1)) lo = mi, k = 2 * k + 1;
    else {
      if (quit(2 * k + 1)) return empty;
      hi = mi, k = k * 2;
    }
  }
  if (quit(k)) return empty;
  return {size_t(ofs_node[lo + width]), lcp};
}

template <typename U, typename S = std::make_signed_t<U>>
requires std::integral<U>
encode::lz_data find_right(size_t adr, size_t i, size_t d, size_t min_len,
    std::span<const U> lcp_node, std::span<const S> ofs_node) {
  const size_t width = lcp_node.size() / 2;
  const auto found = [&](size_t k) { return ofs_node[k] + ptrdiff_t(d) >= ptrdiff_t(adr); };
  U lcp = std::numeric_limits<U>::max();
  const auto quit = [&](size_t k) { return lcp_node[k] < lcp && (lcp = lcp_node[k]) < min_len; };

  size_t lo = i, hi = i + 1, k = lo + width;
  while (hi < width && !found(k)) {
    if (quit(k)) return empty;
    size_t diff = hi - lo;
    if (k & 1) lo = hi, hi += 2 * diff, k = (k + 1) >> 1;
    else hi += diff, lo += diff, ++k;
  }
  if (hi == width && !found(k)) return empty;
  while (k < width) {
    size_t mi = (lo + hi) >> 1;
    if (found(2 * k)) hi = mi, k = 2 * k;
    else {
      if (quit(2 * k)) return empty;
      lo = mi, k = 2 * k + 1;
    }
  }
  return {size_t(ofs_node[lo + width]), lcp};
}

template <typename U, typename S = std::make_signed_t<U>>
requires std::integral<U>
encode::lz_data find(size_t adr, size_t rank, size_t max_dist, size_t min_len,
    std::span<const U> lcp_node, std::span<const S> ofs_node) {
  const auto left = find_left(adr, rank, max_dist, min_len, lcp_node, ofs_node);
  const auto right = find_right(adr, rank, max_dist, min_len, lcp_node, ofs_node);
  return left.len >= right.len ? left : right; // [TODO] choose the close one.
}

template <typename U, typename S = std::make_signed_t<U>>
requires std::integral<U>
encode::lz_data find_closest(size_t adr, size_t rank, size_t max_dist, size_t min_len, size_t max_len,
    const segment_tree<range_min<U>>& lcp, const segment_tree<range_max<S>>& seg) {
  auto ret = find(adr, rank, max_dist, min_len, lcp.nodes(), seg.nodes());
  if (ret.len > 0) {
    if (ret.len > max_len) ret.len = max_len;
    const auto r = lcp.find_range(rank, [&](size_t len) { return len >= ret.len; });
    ret.ofs = seg.fold(r.first, r.second + 1);
  }
  return ret;
}

template <typename U, typename Elem>
requires std::integral<U>
encode::lz_data find(size_t i, size_t j, size_t rank, const wavelet_matrix<U>& wm,
    const segment_tree<range_min<U>>& lcp, const suffix_array<Elem, U>& sa) {
  const auto k = wm.count_lt(i, j, rank);
  auto ret = empty;
  if (k > 0) {
    const auto rank_l = wm.kth(i, j, k - 1);
    const auto len_l = lcp.fold(rank_l, rank);
    if (len_l > ret.len) ret = {sa[rank_l], len_l};
  }
  if (k < (j - i)) {
    const auto rank_r = wm.kth(i, j, k);
    const auto len_r = lcp.fold(rank, rank_r);
    // [TODO] choose the close one.
    if (len_r > ret.len) ret = {sa[rank_r], len_r};
  }
  return ret;
}

template <typename Func>
requires std::convertible_to<std::invoke_result_t<Func, size_t>, encode::lz_data>
encode::lz_data find_non_overlapping(size_t adr_l, size_t adr, Func&& find_lz, encode::lz_data prev) {
  constexpr auto overlapped = [](size_t i, const encode::lz_data& res) {
    return res.len > 0 && res.ofs + res.len > i;
  };
  if (prev.len >= 1) prev.len -= 1, prev.ofs += 1;
  auto ret = find_lz(adr - (std::max<size_t>(prev.len, 1) - 1));
  if (!overlapped(adr, ret)) return ret;
  size_t len_hi = std::min(adr - adr_l, ret.len);
  ret.len = adr - ret.ofs;
  while (ret.len < len_hi) {
    const size_t len = (ret.len + len_hi + 1) / 2;
    auto lz = find_lz(adr - (len - 1));
    if (overlapped(adr, lz)) lz.len = adr - lz.ofs;
    if (lz > ret) ret = lz;
    if (lz.len < len) len_hi = len - 1;
  }
  return ret;
}

} // namespace lz

} // namespace encode

template <typename U = uint32_t>
requires std::unsigned_integral<U>
class lz_helper {
public:
  using index_type = U;
  using signed_index_type = std::make_signed_t<index_type>;

  lz_helper(std::span<const uint8_t> input) : n(input.size()), seg(n) {
    const auto [lcp, rank]= suffix_array<uint8_t>(input).lcp_rank();
    this->rank = std::move(rank);
    this->lcp = decltype(this->lcp)(lcp);
  }

  encode::lz_data find(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank[pos], max_dist, min_len, lcp.nodes(), seg.nodes());
  }

  encode::lz_data find_closest(size_t pos, size_t max_dist, size_t min_len, size_t max_len) const {
    return encode::lz::find_closest(pos, rank[pos], max_dist, min_len, max_len, lcp, seg);
  }

  void add_element(size_t i) {
    seg.update(rank[i], i);
  }

private:
  const size_t n;
  std::vector<index_type> rank;
  segment_tree<range_max<signed_index_type>> seg;
  segment_tree<range_min<index_type>> lcp;
};

template <typename U = uint32_t>
requires std::unsigned_integral<U>
class lz_helper_c {
public:
  using index_type = U;
  using signed_index_type = std::make_signed_t<index_type>;

private:
  std::vector<int16_t> complement_appended(std::span<const uint8_t> input) const {
    std::vector<int16_t> input_xor(2 * n + 1);
    for (size_t i = 0; i < n; ++i) input_xor[i] = input[i];
    input_xor[n] = -1;
    for (size_t i = 0; i < n; ++i) input_xor[i + n + 1] = input[i] ^ 0xff;
    return input_xor;
  }

public:
  lz_helper_c(std::span<const uint8_t> input) : n(input.size()) {
    const auto [lcp, rank] = suffix_array<int16_t>(complement_appended(input)).lcp_rank();
    this->rank = std::move(rank);
    this->lcp = decltype(this->lcp)(lcp);
    seg = decltype(seg)(rank.size());
    seg_c = decltype(seg_c)(rank.size());
  }

public:
  encode::lz_data find(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank[pos], max_dist, min_len, lcp.nodes(), seg.nodes());
  }

  encode::lz_data find_c(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank[pos], max_dist, min_len, lcp.nodes(), seg_c.nodes());
  }

  void add_element(size_t i) {
    seg.update(rank[i], signed_index_type(i));
    seg_c.update(rank[i + n + 1], signed_index_type(i));
  }

private:
  const size_t n;
  std::vector<index_type> rank;
  segment_tree<range_max<signed_index_type>> seg, seg_c;
  segment_tree<range_min<index_type>> lcp;
};

constexpr auto bit_reversed = [] {
  std::array<uint8_t, 256> rev;
  for (size_t i = 0; i < rev.size(); ++i) {
    size_t b = i;
    b = (b & 0xf0) >> 4 | (b & 0x0f) << 4;
    b = (b & 0xcc) >> 2 | (b & 0x33) << 2;
    b = (b & 0xaa) >> 1 | (b & 0x55) << 1;
    rev[i] = b;
  }
  return rev;
}();

template <typename U = uint32_t>
requires std::unsigned_integral<U>
class lz_helper_kirby {
public:
  using index_type = U;
  using signed_index_type = std::make_signed_t<index_type>;

private:
  std::vector<int16_t> hflip_appended(std::span<const uint8_t> input) const {
    std::vector<int16_t> ret(2 * n + 1);
    for (size_t i = 0; i < n; ++i) ret[i] = input[i];
    ret[n] = -1;
    for (size_t i = 0; i < n; ++i) ret[i + n + 1] = bit_reversed[input[i]];
    return ret;
  }

  std::vector<int16_t> vflip_appended(std::span<const uint8_t> input) const {
    std::vector<int16_t> ret(2 * n + 1);
    for (size_t i = 0; i < n; ++i) ret[i] = input[i];
    ret[n] = -1;
    for (size_t i = 0; i < n; ++i) ret[i + n + 1] = input[n - 1 - i];
    return ret;
  }

public:
  lz_helper_kirby(std::span<const uint8_t> input) : n(input.size()) {
    const auto [lcp_h, rank_h] = suffix_array<int16_t>(hflip_appended(input)).lcp_rank();
    this->rank_h = std::move(rank_h);
    this->lcp_h = decltype(this->lcp_h)(lcp_h);
    seg = decltype(seg)(rank_h.size());
    seg_h = decltype(seg_h)(rank_h.size());

    const auto [lcp_v, rank_v] = suffix_array<int16_t>(vflip_appended(input)).lcp_rank();
    this->rank_v = std::move(rank_v);
    this->lcp_v = decltype(this->lcp_v)(lcp_v);
    seg_v = decltype(seg_v)(rank_v.size());
  }

public:
  encode::lz_data find(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank_h[pos], max_dist, min_len, lcp_h.nodes(), seg.nodes());
  }

  encode::lz_data find_h(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank_h[pos], max_dist, min_len, lcp_h.nodes(), seg_h.nodes());
  }

  encode::lz_data find_v(size_t pos, size_t max_dist, size_t min_len) const {
    return encode::lz::find(pos, rank_v[pos], max_dist, min_len, lcp_v.nodes(), seg_v.nodes());
  }

  void add_element(size_t i) {
    seg.update(rank_h[i], i);
    seg_h.update(rank_h[i + n + 1], i);
    seg_v.update(rank_v[2 * n - i], i);
  }

private:
  const size_t n;
  std::vector<index_type> rank_h, rank_v;
  segment_tree<range_min<index_type>> lcp_h, lcp_v;
  segment_tree<range_max<signed_index_type>> seg, seg_h, seg_v;
};

template <typename U = uint32_t>
requires std::unsigned_integral<U>
class non_overlapping_lz_helper {
 public:
  using index_type = U;
  using signed_index_type = std::make_signed_t<index_type>;

  non_overlapping_lz_helper(std::span<const uint8_t> input) : n(input.size()), sa(input) {
    const auto [lcp, rank]= sa.lcp_rank();
    this->rank = std::move(rank);
    this->wm = decltype(wm)(this->rank);
    this->lcp = decltype(this->lcp)(lcp);
  }

  encode::lz_data find_non_overlapping(const size_t adr, const size_t max_dist,
      const encode::lz_data prev = {}) const {
    const size_t adr_l = (adr < max_dist) ? 0 : adr - max_dist;
    const size_t rank = this->rank[adr];
    return encode::lz::find_non_overlapping(adr_l, adr, [&](size_t adr_r) {
      return encode::lz::find(adr_l, adr_r, rank, wm, lcp, sa);
    }, prev);
  }

  encode::lz_data find(const size_t adr, const size_t max_dist) const {
    const size_t adr_l = (adr < max_dist) ? 0 : adr - max_dist;
    return encode::lz::find(adr_l, adr, rank[adr], wm, lcp, sa);
  }

 private:
  const size_t n;
  suffix_array<uint8_t> sa;
  std::vector<index_type> rank;
  wavelet_matrix<index_type> wm;
  segment_tree<range_min<index_type>> lcp;
};

template <size_t A, size_t B, size_t C>
struct LinearQ {
  constexpr size_t operator() (size_t i) const {
    return (A * i + B) / C;
  }
};

template <size_t A, size_t B>
struct Linear : LinearQ<A, B, 1> {};

template <size_t C>
struct Constant : Linear<0, C> {};

template <typename>
struct is_linear : std::false_type {};

template <size_t A, size_t B, size_t C>
struct is_linear<LinearQ<A, B, C>>
  : std::conditional_t<C == 1, std::true_type, std::false_type> {};

template <size_t A, size_t B>
struct is_linear<Linear<A, B>> : is_linear<LinearQ<A, B, 1>> {};

template <size_t N>
struct is_linear<Constant<N>> : is_linear<Linear<0, N>> {};

template <typename, size_t>
struct is_linear_k : std::false_type {};

template <size_t A, size_t B, size_t C, size_t K>
struct is_linear_k<LinearQ<A, B, C>, K>
  : std::conditional_t<K % C == 0, std::true_type, std::false_type> {};

template <size_t A, size_t B, size_t K>
struct is_linear_k<Linear<A, B>, K> : is_linear_k<LinearQ<A, B, 1>, K> {};

template <size_t N, size_t K>
struct is_linear_k<Constant<N>, K> : is_linear_k<Linear<0, N>, K> {};

template <typename CostType>
struct cost_traits;

template <>
struct cost_traits<size_t> {
  static constexpr size_t infinity() { return std::numeric_limits<size_t>::max() / 2; }
  static constexpr size_t unspecified() { return std::numeric_limits<size_t>::max(); }
};

template <typename CostType = size_t>
class uncomp_helper {
 public:
  using cost_type = CostType;
  static constexpr cost_type infinite_cost = cost_traits<cost_type>::infinity();
  static constexpr size_t nlen = std::numeric_limits<size_t>::max();

  struct len_cost {
    size_t len;
    cost_type cost;
  };

 private:
  struct indexed_cost {
    constexpr bool operator < (const indexed_cost& rhs) const {
      return cost < rhs.cost || (cost == rhs.cost && index < rhs.index);
    }
    cost_type cost;
    size_t index;
  };
  static constexpr indexed_cost iden = indexed_cost(infinite_cost, nlen);

 public:
  uncomp_helper(size_t size, size_t slope)
      : n(size), slope_(slope), tree_(size) {}

  void update(size_t i, cost_type cost) {
    tree_.update(i, {cost  + (n - i) * slope_, i});
  }

  void reset(size_t i) {
    tree_.update(i, iden);
  }

  void reset(size_t begin, size_t end) {
    for (size_t i = begin; i < end; ++i) reset(i);
  }

  len_cost find(size_t i, size_t fr, size_t to) const {
    if (i < fr) return {nlen, infinite_cost};
    to = std::min(i, to);
    const auto res = tree_.fold(i - to, i - fr + 1);
    if (res.cost >= infinite_cost) return {nlen, infinite_cost};
    return {i - res.index, res.cost - (n - i) * slope_};
  }

 private:
  size_t n;
  size_t slope_;
  segment_tree<range_min<indexed_cost, iden>> tree_;
};

struct vrange {
  size_t min;
  size_t max;
  size_t bitlen;
  uint64_t val;
  uint64_t mask = -1;
};

struct vrange_min {
  size_t min;
  size_t bitlen;
  uint64_t val;
  uint64_t mask = -1;
};

template <size_t N>
constexpr std::array<vrange, N> to_vranges(vrange_min (&&a)[N], size_t max_len) {
  return create_array<vrange, N>([&](size_t i) {
    return vrange(a[i].min, (i + 1 == N) ? max_len : a[i + 1].min - 1, a[i].bitlen, a[i].val, a[i].mask);
  });
}

namespace encode::lz {

template <typename MaxOffset, typename Func>
requires std::convertible_to<std::invoke_result_t<MaxOffset, size_t>, size_t> &&
         std::convertible_to<std::invoke_result_t<Func, size_t>, encode::lz_data>
void find_all(size_t i, size_t o_size, const size_t lz_min_len,
    std::span<encode::lz_data> dest, MaxOffset&& max_ofs, Func&& find_lz) {
  for (ptrdiff_t oi = o_size - 1; oi >= 0; ) {
    auto res_lz = find_lz(max_ofs(oi));
    if (res_lz.len < lz_min_len) res_lz = {0, 0};
    do {
      dest[oi--] = res_lz;
    } while (oi >= 0 && (res_lz.len < lz_min_len || (i - res_lz.ofs) <= max_ofs(oi)));
  }
}

template <typename Func>
void find_all(size_t i, std::span<const size_t> max_offsets, const size_t lz_min_len,
    std::span<encode::lz_data> dest, Func&& find_lz) {
  return find_all(i, max_offsets.size(), lz_min_len, dest,
                  [&](size_t oi) { return max_offsets[oi]; }, std::forward<Func>(find_lz));
}

template <typename Func>
void find_all(size_t i, std::span<const vrange> offsets, const size_t lz_min_len,
    std::span<encode::lz_data> dest, Func&& find_lz) {
  return find_all(i, offsets.size(), lz_min_len, dest,
                  [&](size_t oi) { return offsets[oi].max; }, std::forward<Func>(find_lz));
}

} // namespace encode::lz

template <typename Tag>
requires std::equality_comparable<Tag> && std::convertible_to<Tag, uint32_t>
struct tag_ol {
  tag_ol() = default;
  tag_ol(Tag tag, uint64_t oi, uint64_t li) : tag(tag), oi(oi), li(li) {}
  constexpr bool operator == (const tag_ol& rhs) const {
    return tag == rhs.tag && li == rhs.li;
  }
  Tag tag : 32;
  uint64_t oi : 16;
  uint64_t li : 16;
};

template <typename Tag>
requires std::equality_comparable<Tag> && std::convertible_to<Tag, uint32_t>
struct tag_l {
  tag_l() = default;
  tag_l(Tag tag, uint64_t li) : tag(tag), li(li) {}
  constexpr bool operator == (const tag_l& rhs) const {
    return tag == rhs.tag && li == rhs.li;
  }
  Tag tag : 32;
  uint64_t li : 16;
};

template <typename Tag>
requires std::equality_comparable<Tag> && std::convertible_to<Tag, uint32_t>
struct tag_o {
  tag_o() = default;
  tag_o(Tag tag, uint64_t oi) : tag(tag), oi(oi) {}
  constexpr bool operator == (const tag_o& rhs) const {
    return tag == rhs.tag;
  }
  Tag tag : 32;
  uint64_t oi : 16;
};

template <typename U, typename V>
concept add_able = requires (U u, V v) {
  {u + v} -> std::convertible_to<U>;
};

template <typename TagType, typename CostType = size_t>
requires std::equality_comparable<TagType>
class sssp_solver {
 public:
  using cost_type = CostType;
  using tag_type = TagType;
  static constexpr cost_type infinite_cost = cost_traits<cost_type>::infinity();

 private:
  static constexpr cost_type unspecified = cost_traits<cost_type>::unspecified();

 public:
  struct Vertex {
    cost_type cost;
    size_t len;
    size_t lz_ofs;
    tag_type type;

    size_t val() const { return lz_ofs; }
    void set_val(size_t v) { lz_ofs = v; }
  };

  using vertex_type = Vertex;

 public:
  sssp_solver() = default;
  sssp_solver(const size_t n, size_t begin = 0) : vertex(n + 1) {
    reset(0, n + 1);
    if (begin <= n) (*this)[begin].cost = cost_type(0);
  }

  void reset(size_t i) {
    (*this)[i].cost = infinite_cost;
  }

  void reset(size_t begin, size_t end) {
    for (size_t i = begin; i < end; ++i) reset(i);
  }

 private:
  template <typename LzFunc, typename Func>
  requires std::convertible_to<std::invoke_result_t<LzFunc, size_t>, encode::lz_data> &&
           std::convertible_to<std::invoke_result_t<Func, ptrdiff_t, size_t, size_t, size_t, encode::lz_data>, ptrdiff_t>
  void update_lz_matrix_(size_t adr, size_t li, encode::lz_data res_lz, const size_t lz_min_len,
      std::span<const vrange> offsets, LzFunc&& find_lz, Func&& update) {
    using encode::lz_data;
    ptrdiff_t best_oi = -1; lz_data best_lz = {0, 0};
    size_t best_bitlen = std::numeric_limits<size_t>::max();
    ptrdiff_t oi = offsets.size() - 1;
    while (true) {
      const size_t d = adr - res_lz.ofs;
      if (res_lz.len < lz_min_len) break;
      while (oi >= 0 && d < offsets[oi].min) --oi;
      if (oi < 0) break;
      if (offsets[oi].bitlen <= best_bitlen) {
        best_oi = oi; best_bitlen = offsets[best_oi].bitlen; best_lz = res_lz;
      }
      lz_data nres_lz = (oi == 0) ? lz_data(0, 0) : find_lz(oi - 1);
      li = update(li, best_oi, nres_lz.len + 1, res_lz.len, best_lz);
      if (--oi < 0) break;
      res_lz = std::move(nres_lz);
    }
  }

 public:
  template <typename LzFunc, typename TagFunc>
  requires std::convertible_to<std::invoke_result_t<TagFunc, size_t, size_t>, tag_type>
  void update_lz_matrix(size_t adr, std::span<const vrange> offsets, std::span<const vrange> lens,
      LzFunc&& find_lz, TagFunc&& tag, size_t c, cost_type base_cost = unspecified) {
    if (lens.empty() || offsets.empty()) return;
    if (base_cost == unspecified) base_cost = (*this)[adr].cost;
    const auto f = [&](ptrdiff_t li, size_t oi, size_t min_len, size_t max_len, encode::lz_data best_lz) {
      for (; li >= 0 && max_len < lens[li].min; --li);
      for (; li >= 0 && min_len <= lens[li].max; --li) {
        const auto& l = lens[li];
        const auto cost = base_cost + (offsets[oi].bitlen + l.bitlen + c);
        update_lz(adr, std::max(min_len, l.min), std::min(max_len, l.max), best_lz, Constant<0>(), tag(oi, li), cost);
        if (min_len > l.min) break;
      }
      return li;
    };
    return update_lz_matrix_(adr, lens.size() - 1, find_lz(offsets.size() - 1), lens[0].min, offsets,
                             std::forward<LzFunc>(find_lz), std::forward<decltype(f)>(f));
  }

  template <typename LzFunc, typename TagFunc, typename LenCostFunc>
  requires add_able<cost_type, std::invoke_result_t<LenCostFunc, size_t>> &&
           std::convertible_to<std::invoke_result_t<TagFunc, size_t, size_t>, tag_type>
  void update_lz_matrix(size_t adr, std::span<const vrange> offsets, std::span<const size_t> lens,
      LzFunc&& find_lz, LenCostFunc&& len_cost, TagFunc&& tag, cost_type base_cost = unspecified) {
    if (lens.empty() || offsets.empty()) return;
    if (base_cost == unspecified) base_cost = (*this)[adr].cost;
    const auto f = [&](ptrdiff_t li, size_t oi, size_t min_len, size_t max_len, encode::lz_data best_lz) {
      for (; li >= 0 && max_len < lens[li]; --li);
      for (; li >= 0 && min_len <= lens[li]; --li) {
        const auto cost = base_cost + (offsets[oi].bitlen + len_cost(li));
        auto& target = (*this)[adr + lens[li]];
        if (cost >= target.cost) continue;
        target = {cost, lens[li], size_t(best_lz.ofs), tag(oi, li)};
      }
      return li;
    };
    encode::lz_data res_lz = find_lz(offsets.size() - 1);
    res_lz.len = std::min(res_lz.len, (size() - 1) - adr); // needed when compressing chunks.
    ptrdiff_t li = (std::ranges::upper_bound(lens, res_lz.len) - lens.begin()) - 1;
    return update_lz_matrix_(adr, li, res_lz, lens[0], offsets,
                             std::forward<LzFunc>(find_lz), std::forward<decltype(f)>(f));
  }

  template <typename Func>
  requires add_able<cost_type, std::invoke_result_t<Func, size_t>>
  void update_lz_table(size_t adr, std::span<const size_t> table, encode::lz_data lz, Func&& func, tag_type tag) {
    const cost_type base_cost = (*this)[adr].cost;
    for (size_t i = 0; i < table.size(); ++i) {
      const size_t l = table[i];
      if (l > lz.len || adr + l >= size()) break;
      auto& target = (*this)[adr + l];
      const cost_type curr_cost = base_cost + func(i);
      if (curr_cost >= target.cost) continue;
      target.cost = curr_cost;
      target.len = l;
      target.lz_ofs = lz.ofs;
      target.type = tag;
    }
  }

  template <typename Skip = std::greater_equal<cost_type>>
  requires std::convertible_to<std::invoke_result_t<Skip, cost_type, cost_type>, bool>
  void update(size_t adr, size_t len, tag_type tag, cost_type cost, size_t arg = 0) {
    if (adr < len) return;
    constexpr auto skip = Skip();
    auto& target = (*this)[adr];
    if (skip(cost, target.cost)) return;
    target.cost = cost;
    target.len = len;
    target.lz_ofs = arg;
    target.type = tag;
  }

  void update_u(size_t adr, size_t len, tag_type tag, cost_type cost, size_t arg = 0) {
    return update<std::greater<cost_type>>(adr, len, tag, cost, arg);
  }

  template <typename Skip = std::greater_equal<cost_type>, typename Func>
  requires std::convertible_to<std::invoke_result_t<Skip, cost_type, cost_type>, bool> &&
           add_able<cost_type, std::invoke_result_t<Func, size_t>>
  void update(size_t adr, size_t fr, size_t to, Func&& func,
      tag_type tag, cost_type base_cost = unspecified, size_t arg = 0) {
    constexpr auto skip = Skip();
    to = std::min(to, size() > adr ? (size() - 1) - adr : 0);
    if (base_cost == unspecified) base_cost = (*this)[adr].cost;
    for (size_t i = to; ptrdiff_t(i) >= ptrdiff_t(fr); --i) {
      const cost_type curr_cost = base_cost + func(i);
      auto& target = (*this)[adr + i];
      if (skip(curr_cost, target.cost)) {
        if constexpr (is_linear<Func>::value) {
          if (target.type == tag) break;
        }
        continue;
      }
      target.cost = curr_cost;
      target.lz_ofs = arg;
      target.len = i;
      target.type = tag;
    }
  }

  template <typename Skip = std::greater_equal<cost_type>, typename Func>
  void update(size_t adr, size_t fr, size_t to, size_t len, Func&& func,
      tag_type tag, cost_type base_cost = unspecified, size_t arg = 0) {
    return update<Skip, Func>(adr, fr, std::min(to, len), std::forward<Func>(func), tag, base_cost, arg);
  }

  template <typename Skip = std::greater_equal<cost_type>, typename Func>
  void update_lz(size_t adr, size_t fr, size_t to, encode::lz_data lz, Func&& func,
      tag_type tag, cost_type base_cost = unspecified) {
    return update<Skip, Func>(adr, fr, std::min(to, lz.len), std::forward<Func>(func), tag, base_cost, lz.ofs);
  }

  template <size_t K, typename Func>
  requires (K > 0) &&
           add_able<cost_type, std::invoke_result_t<Func, size_t>>
  void update_k(size_t adr, size_t fr, size_t to, Func&& func, tag_type tag, size_t arg = 0) {
    to = std::min(to, size() > adr ? (size() - 1) - adr : 0);
    if (to < fr) return;
    to = fr + (to - fr) / K * K;
    const cost_type base_cost = (*this)[adr].cost;
    for (size_t i = to; ptrdiff_t(i) >= ptrdiff_t(fr); i -= K) {
      const cost_type curr_cost = base_cost + func(i);
      auto& target = (*this)[adr + i];
      if (curr_cost >= target.cost) {
        if constexpr (is_linear_k<Func, K>::value) {
          if (target.type == tag) break;
        }
        continue;
      }
      target.cost = curr_cost;
      target.len = i;
      target.lz_ofs = arg;
      target.type = tag;
    }
  }

  template <size_t K, typename Func>
  void update_k(size_t adr, size_t fr, size_t to,
      size_t max_len, Func&& func, tag_type tag, size_t arg = 0) {
    return update_k<K>(adr, fr, std::min(max_len, to), std::forward<Func>(func), tag, arg);
  }

  cost_type optimal_cost() const {
    return vertex.back().cost;
  }

  std::vector<vertex_type> commands(ptrdiff_t start=0) const {
    std::vector<vertex_type> ret;
    ptrdiff_t adr = size() - 1;
    while (adr > start) {
      auto cmd = (*this)[adr];
      if (cmd.len == 0) throw std::logic_error("cmd.len == 0");
      adr -= cmd.len;
      ret.emplace_back(cmd);
    }
    assert(adr == start);
    std::reverse(ret.begin(), ret.end());
    return ret;
  }

  const vertex_type& operator [] (size_t i) const {
    return vertex[i];
  }

  vertex_type& operator [] (size_t i) {
    return vertex[i];
  }

  size_t size() const {
    return vertex.size();
  }

 private:
  std::vector<vertex_type> vertex;
};

} // namespace sfc_comp

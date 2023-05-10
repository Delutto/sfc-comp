#include "algorithm.hpp"
#include "encode.hpp"
#include "utility.hpp"
#include "writer.hpp"

namespace sfc_comp {

namespace {

std::vector<uint8_t> super_robot_wars_comp_core(
    std::span<const uint8_t> input, const size_t lz_max_len, const size_t header_size, const size_t skipped_size) {
  enum tag { uncomp, lzs, lzl, lzll };

  lz_helper lz_helper(input);
  sssp_solver<tag> dp(input.size(), skipped_size);

  if (skipped_size > input.size()) {
    throw std::logic_error("skipped_size exceeds the input size.");
  }
  for (size_t i = 0; i < skipped_size; ++i) lz_helper.add_element(i);

  for (size_t i = skipped_size; i < input.size(); ++i) {
    dp.update(i, 1, 1, Constant<9>(), uncomp);
    auto res_lzs = lz_helper.find(i, 0x100, 2);
    dp.update_lz(i, 2, 5, res_lzs, Constant<12>(), lzs);
    auto res_lzl = lz_helper.find(i, 0x2000, 3);
    dp.update_lz(i, 3, 9, res_lzl, Constant<18>(), lzl);
    dp.update_lz(i, 10, lz_max_len, res_lzl, Constant<26>(), lzll);
    lz_helper.add_element(i);
  }

  using namespace data_type;
  writer_b8_h ret(header_size); ret.write<d8n>({skipped_size, &input[0]});

  size_t adr = skipped_size;
  for (const auto cmd : dp.commands(adr)) {
    size_t d = adr - cmd.lz_ofs;
    switch (cmd.type) {
    case uncomp: ret.write<b1, d8>(true, input[adr]); break;
    case lzs: ret.write<bnh, d8>({4, cmd.len - 2}, 0x100 - d); break;
    case lzl: ret.write<b1, b1, d16b>(false, true, (0x2000 - d) << 3 | (cmd.len - 2)); break;
    case lzll: ret.write<b1, b1, d24b>(false, true, (0x2000 - d) << 11 | (cmd.len - (lz_max_len - 0xFF))); break;
    default: assert(0);
    }
    adr += cmd.len;
  }
  ret.write<b1, b1, d24b>(false, true, 0);
  assert(adr == input.size());
  assert(dp.optimal_cost() + 2 + 3 * 8 + (header_size + skipped_size) * 8 == ret.bit_length());
  return ret.out;
}

} // namespace

std::vector<uint8_t> super_robot_wars_comp(std::span<const uint8_t> input) {
  return super_robot_wars_comp_core(input, 256, 0, 0);
}

std::vector<uint8_t> tactics_ogre_comp_2(std::span<const uint8_t> input) {
  return super_robot_wars_comp_core(input, 264, 0, 0);
}

std::vector<uint8_t> tenchi_souzou_comp(std::span<const uint8_t> input) {
  check_size(input.size(), 1, 0x10000);
  auto ret = super_robot_wars_comp_core(input, 256, 3, 1);
  ret[0] = 0; // [TODO] unknown
  write16(ret, 1, input.size());
  return ret;
}

} // namespace sfc_comp

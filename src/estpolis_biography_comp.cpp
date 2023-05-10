#include "algorithm.hpp"
#include "encode.hpp"
#include "utility.hpp"
#include "writer.hpp"

namespace sfc_comp {

std::vector<uint8_t> estpolis_biography_comp(std::span<const uint8_t> input) {
  check_size(input.size(), 1, 0x10000);

  enum tag { uncomp0, uncomp1, lzs, lzl };

  lz_helper lz_helper(input);
  sssp_solver<tag> dp(input.size());

  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] & 0x80) {
      dp.update(i, 1, 1, Constant<9>(), uncomp1);
    } else {
      dp.update(i, 1, 1, Constant<8>(), uncomp0);
    }
    auto res_lzs = lz_helper.find(i, 0x800, 3);
    dp.update_lz(i, 3, 0x11, res_lzs, Constant<17>(), lzs);
    auto res_lzl = lz_helper.find(i, 0x2000, 3);
    dp.update_lz(i, 3, 0x42, res_lzl, Constant<25>(), lzl);
    lz_helper.add_element(i);
  }
  using namespace data_type;
  writer_b8_h ret(2);
  size_t adr = 0;
  for (const auto cmd : dp.commands()) {
    size_t d = adr - cmd.lz_ofs;
    switch (cmd.type) {
    case uncomp0: ret.write<none, d8>(none(), input[adr]); break;
    case uncomp1: ret.write<b1, d8>(false, input[adr]); break;
    case lzs: ret.write<b1, d16b>(true, (0x1000 - d) << 4 | (cmd.len - 2)); break;
    case lzl: ret.write<b1, d24b>(true,
      ((0x4000 - d) & ~3) << 10 | ((0x4000 - d) & 3) << 6 | (cmd.len - 3)); break;
    default: assert(0);
    }
    adr += cmd.len;
  }
  write16(ret.out, 0, input.size());
  assert(adr == input.size());
  assert(dp.optimal_cost() + 2 * 8 == ret.bit_length());
  return ret.out;
}

} // namespace sfc_comp

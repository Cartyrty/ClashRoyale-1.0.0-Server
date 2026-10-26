#include "titan/RC4Encrypter.h"

#include <utility>
#include <vector>

namespace server {
const char *const RC4Encrypter::kKey = "fhsd6f86f67rt8fw78fw789we78r9789wer6re";

RC4Encrypter::RC4Encrypter(const std::string &key) {
    for (int k = 0; k < 256; ++k) state_[k] = std::uint8_t(k);
    std::uint8_t j = 0;
    for (int k = 0; k < 256; ++k) {
        j = std::uint8_t(j + state_[k] + std::uint8_t(key[std::size_t(k) % key.size()]));
        std::swap(state_[k], state_[j]);
    }
    std::vector<std::uint8_t> discarded(key.size());
    encrypt(discarded.data(), discarded.size());
}

void RC4Encrypter::encrypt(std::uint8_t *data, std::size_t length) {
    for (std::size_t n = 0; n < length; ++n) {
        i_ = std::uint8_t(i_ + 1);
        j_ = std::uint8_t(j_ + state_[i_]);
        std::swap(state_[i_], state_[j_]);
        data[n] ^= state_[std::uint8_t(state_[i_] + state_[j_])];
    }
}
} // namespace server

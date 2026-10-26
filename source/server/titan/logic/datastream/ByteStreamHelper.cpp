#include "titan/logic/datastream/ByteStreamHelper.h"

namespace server {
namespace ByteStreamHelper {
using library::call;

void encodeLogicLong(ByteStream &stream, std::int64_t value) {
    call("__ZN16ByteStreamHelper15encodeLogicLongEP15ChecksumEncoderPK9LogicLong",
         {stream.getLibraryStream(), library::newLogicLong(value)});
}

std::int64_t decodeLogicLong(ByteStream &stream) {
    const auto logicLong = library::newLogicLong(0);
    call("__ZN16ByteStreamHelper15decodeLogicLongEP10ByteStreamP9LogicLong", {stream.getLibraryStream(), logicLong});
    return library::readLogicLong(logicLong);
}

void writeDataReference(ByteStream &stream, int globalId) {
    call("__ZN16ByteStreamHelper13writeGlobalIDEP15ChecksumEncoderi",
         {stream.getLibraryStream(), std::uint64_t(std::uint32_t(globalId))});
}

int readDataReference(ByteStream &stream) {
    return int(call("__ZN16ByteStreamHelper12readGlobalIDEP10ByteStream", {stream.getLibraryStream()}));
}
} // namespace ByteStreamHelper
} // namespace server

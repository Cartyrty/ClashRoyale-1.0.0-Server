#pragma once
// ByteStreamHelper as on the client: LogicLong (two VInts) and data references (a global id as class and instance).
#include "titan/logic/datastream/ByteStream.h"

#include <cstdint>

namespace server {
namespace ByteStreamHelper {
void encodeLogicLong(ByteStream &stream, std::int64_t value);
std::int64_t decodeLogicLong(ByteStream &stream);
void writeDataReference(ByteStream &stream, int globalId); // 0 writes the "no data" reference
int readDataReference(ByteStream &stream);                 // the global id, 0 when none
} // namespace ByteStreamHelper
} // namespace server

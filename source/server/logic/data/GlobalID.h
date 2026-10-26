#pragma once
// GlobalID as on the client: a data row's id is its table's class id * 1000000 + its instance id; 0 is "no data".
// Saved in JSON as [class id, instance id] (the Node server's format), or null.
#include "game/utils/Json.h"

namespace server {
namespace GlobalID {
constexpr int kClassMultiplier = 1000000;

constexpr int createGlobalID(int classId, int instanceId) {
    return classId ? classId * kClassMultiplier + instanceId : 0;
}
constexpr int getClassID(int globalId) { return globalId / kClassMultiplier; }
constexpr int getInstanceID(int globalId) { return globalId % kClassMultiplier; }

inline json::Value saveToJSON(int globalId) {
    if (!globalId) return json::Value();
    auto reference = json::Value::array();
    reference.push(getClassID(globalId));
    reference.push(getInstanceID(globalId));
    return reference;
}

inline int loadFromJSON(const json::Value &reference) {
    if (!reference.isArray() || reference.items().size() != 2) return 0;
    return createGlobalID(int(reference.items()[0].integer()), int(reference.items()[1].integer()));
}
} // namespace GlobalID
} // namespace server

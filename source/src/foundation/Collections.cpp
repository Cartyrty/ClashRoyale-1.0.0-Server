// NSArray / NSDictionary / NSSet (and mutable variants), NSEnumerator, fast enumeration, and the CFArray /
// CFDictionary functions.
#include "foundation/Foundation.h"
#include "Log.h"
#include "Memory.h"
#include <algorithm>

namespace ns {
using objc::marg;
using objc::self;
using hle::ret;

bool objectsEqual(id a, id b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (isString(a) && isString(b)) return text16(a) == text16(b);
    if (isNumber(a) && isNumber(b)) return doubleValue(a) == doubleValue(b) && intValue(a) == intValue(b);
    return objc::send(a, "isEqual:", {b}) & 0xFF;
}
std::uint64_t objectHash(id object) { return object ? objc::send(object, "hash") : 0; }

namespace {
ArrayData *arr(id a) { return objc::host<ArrayData>(a); }
DictData *dict(id d) { return objc::host<DictData>(d); }
SetData *setd(id s) { return objc::host<SetData>(s); }
std::uint64_t mutationsWord(std::uint64_t &slot) {
    if (!slot) slot = mem::hleAlloc(8);
    return slot;
}
id attachArray(id object, std::vector<id> items, bool m) {
    for (auto i : items) objc::retain(i);
    auto d = std::make_unique<ArrayData>();
    d->items = std::move(items);
    d->mutableArray = m;
    objc::setHostObject(object, std::move(d));
    return object;
}
id attachDict(id object, std::vector<std::pair<id, id>> items, bool m) {
    auto d = std::make_unique<DictData>();
    for (auto &[k, v] : items) {
        objc::retain(k);
        objc::retain(v);
        bool replaced = false;
        for (auto &e : d->items) if (objectsEqual(e.first, k)) { e.second = v; replaced = true; break; }
        if (!replaced) d->items.push_back({k, v});
    }
    d->mutableDict = m;
    objc::setHostObject(object, std::move(d));
    return object;
}
id attachSet(id object, const std::vector<id> &items, bool m) {
    auto d = std::make_unique<SetData>();
    for (auto i : items) {
        if (std::any_of(d->items.begin(), d->items.end(), [&](id e) { return objectsEqual(e, i); })) continue;
        objc::retain(i);
        d->items.push_back(i);
    }
    d->mutableSet = m;
    objc::setHostObject(object, std::move(d));
    return object;
}
bool isMutableClass(objc::Class cls, const char *mutableName) { return objc::isSubclass(cls, objc::getClass(mutableName)); }
// Reads a nil-terminated variadic object list whose first element is in register x(2 + firstArg) and the rest on the
// stack (Apple ARM64 variadics).
std::vector<id> nilTerminated(int firstArg) {
    std::vector<id> out;
    id first = marg(firstArg);
    if (!first) return out;
    out.push_back(first);
    for (std::uint64_t slot = cpu::sp();; slot += 8) {
        const id next = mem::r64(slot);
        if (!next) break;
        out.push_back(next);
    }
    return out;
}
std::vector<id> guestList(std::uint64_t pointer, std::uint64_t count) {
    std::vector<id> out;
    for (std::uint64_t i = 0; i < count; ++i) out.push_back(mem::r64(pointer + 8 * i));
    return out;
}
// Fast enumeration: countByEnumeratingWithState:objects:count: over a host vector.
std::uint64_t enumerate(const std::vector<id> &items, std::uint64_t &mutations, std::uint64_t state, std::uint64_t buffer, std::uint64_t len) {
    const auto index = mem::r64(state);
    if (index >= items.size()) return 0;
    const auto n = std::min<std::uint64_t>(len, items.size() - index);
    for (std::uint64_t i = 0; i < n; ++i) mem::w64(buffer + 8 * i, items[std::size_t(index + i)]);
    mem::w64(state, index + n);
    mem::w64(state + 8, buffer);
    mem::w64(state + 16, mutationsWord(mutations));
    return n;
}
std::uint64_t blockInvoke(id block) { return mem::r64(block + 16); }
} // namespace

id array(const std::vector<id> &items, bool m) {
    return attachArray(objc::allocInstance(objc::getClass(m ? "NSMutableArray" : "NSArray")), items, m);
}
id dictionary(const std::vector<std::pair<id, id>> &items, bool m) {
    return attachDict(objc::allocInstance(objc::getClass(m ? "NSMutableDictionary" : "NSDictionary")), items, m);
}
id set(const std::vector<id> &items, bool m) {
    return attachSet(objc::allocInstance(objc::getClass(m ? "NSMutableSet" : "NSSet")), items, m);
}
std::vector<id> arrayItems(id a) {
    if (auto *d = arr(a)) return d->items;
    if (auto *s = setd(a)) return s->items;
    std::vector<id> out;
    if (a) {
        const auto n = objc::send(a, "count");
        for (std::uint64_t i = 0; i < n; ++i) out.push_back(objc::send(a, "objectAtIndex:", {i}));
    }
    return out;
}
id dictGet(id d, id key) {
    if (auto *dd = dict(d))
        for (auto &[k, v] : dd->items) if (objectsEqual(k, key)) return v;
    return 0;
}
void dictSet(id d, id key, id value) {
    auto *dd = dict(d);
    if (!dd || !key) return;
    objc::retain(value);
    for (auto &e : dd->items)
        if (objectsEqual(e.first, key)) { objc::release(e.second); e.second = value; return; }
    objc::retain(key);
    dd->items.push_back({key, value});
}
} // namespace ns

using namespace ns;
using objc::marg;
using objc::self;
using hle::ret;

// ======================================== NSArray ========================================
OBJC_CLASS(NSArray, NSObject);
OBJC_CLASS(NSMutableArray, NSArray);
OBJC_CLASS_METHOD(NSArray, "alloc", alloc) {
    ret(attachArray(objc::allocInstance(self()), {}, isMutableClass(self(), "NSMutableArray")));
}
OBJC_CLASS_METHOD(NSArray, "allocWithZone:", allocWithZone) {
    ret(attachArray(objc::allocInstance(self()), {}, isMutableClass(self(), "NSMutableArray")));
}
OBJC_CLASS_METHOD(NSArray, "array", array_) { ret(array({}, isMutableClass(self(), "NSMutableArray"))); }
OBJC_CLASS_METHOD(NSArray, "arrayWithCapacity:", arrayWithCapacity) { ret(array({}, isMutableClass(self(), "NSMutableArray"))); }
OBJC_CLASS_METHOD(NSArray, "arrayWithObject:", arrayWithObject) { ret(array({marg(0)}, isMutableClass(self(), "NSMutableArray"))); }
OBJC_CLASS_METHOD(NSArray, "arrayWithObjects:", arrayWithObjects) { ret(array(nilTerminated(0), isMutableClass(self(), "NSMutableArray"))); }
OBJC_CLASS_METHOD(NSArray, "arrayWithObjects:count:", arrayWithObjectsCount) {
    ret(array(guestList(marg(0), marg(1)), isMutableClass(self(), "NSMutableArray")));
}
OBJC_CLASS_METHOD(NSArray, "arrayWithArray:", arrayWithArray) { ret(array(arrayItems(marg(0)), isMutableClass(self(), "NSMutableArray"))); }
OBJC_METHOD(NSArray, "init", init) { ret(self()); }
OBJC_METHOD(NSArray, "initWithCapacity:", initWithCapacity) { ret(self()); }
OBJC_METHOD(NSArray, "initWithObjects:count:", initWithObjectsCount) {
    auto *d = arr(self());
    for (auto i : guestList(marg(0), marg(1))) { objc::retain(i); d->items.push_back(i); }
    ret(self());
}
OBJC_METHOD(NSArray, "initWithObjects:", initWithObjects) {
    auto *d = arr(self());
    for (auto i : nilTerminated(0)) { objc::retain(i); d->items.push_back(i); }
    ret(self());
}
OBJC_METHOD(NSArray, "initWithArray:", initWithArray) {
    auto *d = arr(self());
    for (auto i : arrayItems(marg(0))) { objc::retain(i); d->items.push_back(i); }
    ret(self());
}
OBJC_METHOD(NSArray, "initWithArray:copyItems:", initWithArrayCopy) {
    auto *d = arr(self());
    for (auto i : arrayItems(marg(0))) { objc::retain(i); d->items.push_back(i); }
    ret(self());
}
OBJC_METHOD(NSArray, "dealloc", dealloc) {
    if (auto *d = arr(self())) for (auto i : d->items) objc::release(i);
    objc::setHostObject(self(), nullptr);
}
OBJC_METHOD(NSArray, "count", count) { auto *d = arr(self()); ret(d ? d->items.size() : 0); }
OBJC_METHOD(NSArray, "objectAtIndex:", objectAtIndex) {
    auto *d = arr(self());
    if (!d || marg(0) >= d->items.size()) {
        warn("-[NSArray objectAtIndex:] index %llu beyond %zu", (unsigned long long)marg(0), d ? d->items.size() : 0);
        return ret(0);
    }
    ret(d->items[std::size_t(marg(0))]);
}
OBJC_METHOD(NSArray, "objectAtIndexedSubscript:", objectAtIndexedSubscript) {
    auto *d = arr(self());
    ret(d && marg(0) < d->items.size() ? d->items[std::size_t(marg(0))] : 0);
}
OBJC_METHOD(NSArray, "firstObject", firstObject) { auto *d = arr(self()); ret(d && !d->items.empty() ? d->items.front() : 0); }
OBJC_METHOD(NSArray, "lastObject", lastObject) { auto *d = arr(self()); ret(d && !d->items.empty() ? d->items.back() : 0); }
OBJC_METHOD(NSArray, "containsObject:", containsObject) {
    auto *d = arr(self());
    ret(d && std::any_of(d->items.begin(), d->items.end(), [](id e) { return objectsEqual(e, marg(0)); }));
}
OBJC_METHOD(NSArray, "indexOfObject:", indexOfObject) {
    auto *d = arr(self());
    if (d) for (std::size_t i = 0; i < d->items.size(); ++i) if (objectsEqual(d->items[i], marg(0))) return ret(i);
    ret(0x7FFFFFFFFFFFFFFFull);
}
OBJC_METHOD(NSArray, "indexOfObjectIdenticalTo:", indexOfObjectIdenticalTo) {
    auto *d = arr(self());
    if (d) for (std::size_t i = 0; i < d->items.size(); ++i) if (d->items[i] == marg(0)) return ret(i);
    ret(0x7FFFFFFFFFFFFFFFull);
}
OBJC_METHOD(NSArray, "arrayByAddingObject:", arrayByAddingObject) {
    auto items = arrayItems(self());
    items.push_back(marg(0));
    ret(array(items));
}
OBJC_METHOD(NSArray, "arrayByAddingObjectsFromArray:", arrayByAddingObjectsFromArray) {
    auto items = arrayItems(self());
    for (auto i : arrayItems(marg(0))) items.push_back(i);
    ret(array(items));
}
OBJC_METHOD(NSArray, "subarrayWithRange:", subarrayWithRange) {
    const auto items = arrayItems(self());
    std::vector<id> out;
    for (std::uint64_t i = marg(0); i < marg(0) + marg(1) && i < items.size(); ++i) out.push_back(items[std::size_t(i)]);
    ret(array(out));
}
OBJC_METHOD(NSArray, "componentsJoinedByString:", componentsJoinedByString) {
    std::u16string out;
    const auto sep = text16(marg(0));
    bool first = true;
    for (auto i : arrayItems(self())) {
        if (!first) out += sep;
        first = false;
        out += fromUtf8(describe(i));
    }
    ret(stringU16(out));
}
OBJC_METHOD(NSArray, "isEqualToArray:", isEqualToArray) {
    const auto a = arrayItems(self()), b = arrayItems(marg(0));
    if (a.size() != b.size()) return ret(0);
    for (std::size_t i = 0; i < a.size(); ++i) if (!objectsEqual(a[i], b[i])) return ret(0);
    ret(1);
}
OBJC_METHOD(NSArray, "copyWithZone:", copyWithZone) { ret(array(arrayItems(self()))); }
OBJC_METHOD(NSArray, "mutableCopyWithZone:", mutableCopyWithZone) { ret(array(arrayItems(self()), true)); }
OBJC_METHOD(NSArray, "countByEnumeratingWithState:objects:count:", countByEnumerating) {
    auto *d = arr(self());
    static std::uint64_t none = 0;
    ret(d ? enumerate(d->items, d->mutations, marg(0), marg(1), marg(2)) : enumerate({}, none, marg(0), marg(1), marg(2)));
}
OBJC_METHOD(NSArray, "makeObjectsPerformSelector:", makeObjectsPerformSelector) {
    for (auto i : arrayItems(self())) objc::send(i, objc::selName(marg(0)));
}
OBJC_METHOD(NSArray, "makeObjectsPerformSelector:withObject:", makeObjectsPerformSelectorWith) {
    for (auto i : arrayItems(self())) objc::send(i, objc::selName(marg(0)), {marg(1)});
}
// enumerateObjectsUsingBlock: ^(id obj, NSUInteger idx, BOOL *stop)
OBJC_METHOD(NSArray, "enumerateObjectsUsingBlock:", enumerateObjectsUsingBlock) {
    const auto block = marg(0);
    const auto stop = mem::calloc(8);
    const auto items = arrayItems(self());
    for (std::size_t i = 0; i < items.size(); ++i) {
        cpu::call(blockInvoke(block), {block, items[i], i, stop});
        if (mem::read<std::uint8_t>(stop)) break;
    }
    mem::free(stop);
}
static std::vector<id> sortedBy(std::vector<id> items, const std::function<std::int64_t(id, id)> &compare) {
    std::stable_sort(items.begin(), items.end(), [&](id a, id b) { return compare(a, b) < 0; });
    return items;
}
OBJC_METHOD(NSArray, "sortedArrayUsingSelector:", sortedArrayUsingSelector) {
    const auto name = objc::selName(marg(0));
    ret(array(sortedBy(arrayItems(self()), [&](id a, id b) { return std::int64_t(objc::send(a, name, {b})); })));
}
OBJC_METHOD(NSArray, "sortedArrayUsingComparator:", sortedArrayUsingComparator) {
    const auto block = marg(0);
    ret(array(sortedBy(arrayItems(self()), [&](id a, id b) { return std::int64_t(cpu::call(blockInvoke(block), {block, a, b})); })));
}
OBJC_METHOD(NSArray, "sortedArrayUsingFunction:context:", sortedArrayUsingFunction) {
    const auto fn = marg(0), ctx = marg(1);
    ret(array(sortedBy(arrayItems(self()), [&](id a, id b) { return std::int64_t(cpu::call(fn, {a, b, ctx})); })));
}
OBJC_METHOD(NSArray, "objectEnumerator", objectEnumerator) {
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {self()}));
}
OBJC_METHOD(NSArray, "reverseObjectEnumerator", reverseObjectEnumerator) {
    auto items = arrayItems(self());
    std::reverse(items.begin(), items.end());
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {array(items)}));
}
OBJC_METHOD(NSArray, "description", description) {
    std::string out = "(\n";
    for (auto i : arrayItems(self())) out += "    " + describe(i) + ",\n";
    out += ")";
    ret(string(out));
}
OBJC_METHOD(NSArray, "valueForKey:", valueForKey) {
    std::vector<id> out;
    for (auto i : arrayItems(self())) out.push_back(objc::send(i, "valueForKey:", {marg(0)}));
    ret(array(out));
}
// NSMutableArray
OBJC_METHOD(NSMutableArray, "addObject:", addObject) {
    if (!marg(0)) { warn("-[NSMutableArray addObject:] nil"); return; }
    objc::retain(marg(0));
    if (auto *d = arr(self())) { d->items.push_back(marg(0)); ++mem::ptr<std::uint64_t>(mutationsWord(d->mutations))[0]; }
}
OBJC_METHOD(NSMutableArray, "addObjectsFromArray:", addObjectsFromArray) {
    auto *d = arr(self());
    for (auto i : arrayItems(marg(0))) { objc::retain(i); d->items.push_back(i); }
}
OBJC_METHOD(NSMutableArray, "insertObject:atIndex:", insertObject) {
    auto *d = arr(self());
    if (!d || marg(1) > d->items.size()) return;
    objc::retain(marg(0));
    d->items.insert(d->items.begin() + std::ptrdiff_t(marg(1)), marg(0));
}
OBJC_METHOD(NSMutableArray, "removeObjectAtIndex:", removeObjectAtIndex) {
    auto *d = arr(self());
    if (!d || marg(0) >= d->items.size()) return;
    const auto o = d->items[std::size_t(marg(0))];
    d->items.erase(d->items.begin() + std::ptrdiff_t(marg(0)));
    objc::release(o);
}
OBJC_METHOD(NSMutableArray, "removeLastObject", removeLastObject) {
    auto *d = arr(self());
    if (!d || d->items.empty()) return;
    const auto o = d->items.back();
    d->items.pop_back();
    objc::release(o);
}
OBJC_METHOD(NSMutableArray, "removeObject:", removeObject) {
    auto *d = arr(self());
    if (!d) return;
    std::vector<id> removed;
    d->items.erase(std::remove_if(d->items.begin(), d->items.end(), [&](id e) {
        if (objectsEqual(e, marg(0))) { removed.push_back(e); return true; }
        return false;
    }), d->items.end());
    for (auto o : removed) objc::release(o);
}
OBJC_METHOD(NSMutableArray, "removeObjectIdenticalTo:", removeObjectIdenticalTo) {
    auto *d = arr(self());
    if (!d) return;
    const auto before = d->items.size();
    d->items.erase(std::remove(d->items.begin(), d->items.end(), marg(0)), d->items.end());
    for (auto n = before - d->items.size(); n; --n) objc::release(marg(0));
}
OBJC_METHOD(NSMutableArray, "removeObjectsInArray:", removeObjectsInArray) {
    auto *d = arr(self());
    if (!d) return;
    const auto remove = arrayItems(marg(0));
    d->items.erase(std::remove_if(d->items.begin(), d->items.end(), [&](id e) {
        return std::any_of(remove.begin(), remove.end(), [&](id r) { return objectsEqual(e, r); });
    }), d->items.end());
}
OBJC_METHOD(NSMutableArray, "removeAllObjects", removeAllObjects) {
    auto *d = arr(self());
    if (!d) return;
    const auto items = std::move(d->items);
    d->items.clear();
    for (auto i : items) objc::release(i);
}
OBJC_METHOD(NSMutableArray, "replaceObjectAtIndex:withObject:", replaceObjectAtIndex) {
    auto *d = arr(self());
    if (!d || marg(0) >= d->items.size()) return;
    objc::retain(marg(1));
    objc::release(d->items[std::size_t(marg(0))]);
    d->items[std::size_t(marg(0))] = marg(1);
}
OBJC_METHOD(NSMutableArray, "setObject:atIndexedSubscript:", setObjectAtIndexedSubscript) {
    auto *d = arr(self());
    if (!d) return;
    objc::retain(marg(0));
    if (marg(1) == d->items.size()) d->items.push_back(marg(0));
    else if (marg(1) < d->items.size()) { objc::release(d->items[std::size_t(marg(1))]); d->items[std::size_t(marg(1))] = marg(0); }
}
OBJC_METHOD(NSMutableArray, "exchangeObjectAtIndex:withObjectAtIndex:", exchangeObjectAtIndex) {
    auto *d = arr(self());
    if (d && marg(0) < d->items.size() && marg(1) < d->items.size()) std::swap(d->items[std::size_t(marg(0))], d->items[std::size_t(marg(1))]);
}
OBJC_METHOD(NSMutableArray, "sortUsingSelector:", sortUsingSelector) {
    auto *d = arr(self());
    const auto name = objc::selName(marg(0));
    d->items = sortedBy(d->items, [&](id a, id b) { return std::int64_t(objc::send(a, name, {b})); });
}
OBJC_METHOD(NSMutableArray, "sortUsingComparator:", sortUsingComparator) {
    auto *d = arr(self());
    const auto block = marg(0);
    d->items = sortedBy(d->items, [&](id a, id b) { return std::int64_t(cpu::call(blockInvoke(block), {block, a, b})); });
}
OBJC_METHOD(NSMutableArray, "sortUsingFunction:context:", sortUsingFunction) {
    auto *d = arr(self());
    const auto fn = marg(0), ctx = marg(1);
    d->items = sortedBy(d->items, [&](id a, id b) { return std::int64_t(cpu::call(fn, {a, b, ctx})); });
}

// NSEnumerator over a snapshot array.
struct EnumeratorData : objc::HostObject {
    std::vector<id> items;
    std::size_t next = 0;
};
OBJC_CLASS(NSEnumerator, NSObject);
OBJC_METHOD(NSEnumerator, "initWithArray:", initWithArray) {
    auto d = std::make_unique<EnumeratorData>();
    d->items = arrayItems(marg(0));
    objc::setHostObject(self(), std::move(d));
    ret(self());
}
OBJC_METHOD(NSEnumerator, "nextObject", nextObject) {
    auto *d = objc::host<EnumeratorData>(self());
    ret(d && d->next < d->items.size() ? d->items[d->next++] : 0);
}
OBJC_METHOD(NSEnumerator, "allObjects", allObjects) {
    auto *d = objc::host<EnumeratorData>(self());
    std::vector<id> rest;
    if (d) rest.assign(d->items.begin() + std::ptrdiff_t(d->next), d->items.end());
    ret(array(rest));
}
OBJC_METHOD(NSEnumerator, "countByEnumeratingWithState:objects:count:", countByEnumerating) {
    auto *d = objc::host<EnumeratorData>(self());
    static std::uint64_t mutations = 0;
    ret(d ? enumerate(d->items, mutations, marg(0), marg(1), marg(2)) : 0);
}

// ======================================== NSDictionary ========================================
OBJC_CLASS(NSDictionary, NSObject);
OBJC_CLASS(NSMutableDictionary, NSDictionary);
static bool mutableDictClass() { return isMutableClass(self(), "NSMutableDictionary"); }
OBJC_CLASS_METHOD(NSDictionary, "alloc", alloc) { ret(attachDict(objc::allocInstance(self()), {}, mutableDictClass())); }
OBJC_CLASS_METHOD(NSDictionary, "allocWithZone:", allocWithZone) { ret(attachDict(objc::allocInstance(self()), {}, mutableDictClass())); }
OBJC_CLASS_METHOD(NSDictionary, "dictionary", dictionary_) { ret(dictionary({}, mutableDictClass())); }
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithCapacity:", dictionaryWithCapacity) { ret(dictionary({}, mutableDictClass())); }
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithObject:forKey:", dictionaryWithObjectForKey) {
    ret(dictionary({{marg(1), marg(0)}}, mutableDictClass()));
}
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithObjects:forKeys:count:", dictionaryWithObjectsForKeysCount) {
    const auto objects = guestList(marg(0), marg(2)), keys = guestList(marg(1), marg(2));
    std::vector<std::pair<id, id>> items;
    for (std::size_t i = 0; i < keys.size(); ++i) items.push_back({keys[i], objects[i]});
    ret(dictionary(items, mutableDictClass()));
}
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithObjects:forKeys:", dictionaryWithObjectsForKeys) {
    const auto objects = arrayItems(marg(0)), keys = arrayItems(marg(1));
    std::vector<std::pair<id, id>> items;
    for (std::size_t i = 0; i < keys.size() && i < objects.size(); ++i) items.push_back({keys[i], objects[i]});
    ret(dictionary(items, mutableDictClass()));
}
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithObjectsAndKeys:", dictionaryWithObjectsAndKeys) {
    const auto list = nilTerminated(0);
    std::vector<std::pair<id, id>> items;
    for (std::size_t i = 0; i + 1 < list.size(); i += 2) items.push_back({list[i + 1], list[i]});
    ret(dictionary(items, mutableDictClass()));
}
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithDictionary:", dictionaryWithDictionary) {
    auto *src = dict(marg(0));
    ret(dictionary(src ? src->items : std::vector<std::pair<id, id>>{}, mutableDictClass()));
}
OBJC_METHOD(NSDictionary, "init", init) { ret(self()); }
OBJC_METHOD(NSDictionary, "initWithCapacity:", initWithCapacity) { ret(self()); }
OBJC_METHOD(NSDictionary, "initWithObjects:forKeys:count:", initWithObjectsForKeysCount) {
    const auto objects = guestList(marg(0), marg(2)), keys = guestList(marg(1), marg(2));
    for (std::size_t i = 0; i < keys.size(); ++i) dictSet(self(), keys[i], objects[i]);
    ret(self());
}
OBJC_METHOD(NSDictionary, "initWithObjects:forKeys:", initWithObjectsForKeys) {
    const auto objects = arrayItems(marg(0)), keys = arrayItems(marg(1));
    for (std::size_t i = 0; i < keys.size() && i < objects.size(); ++i) dictSet(self(), keys[i], objects[i]);
    ret(self());
}
OBJC_METHOD(NSDictionary, "initWithObjectsAndKeys:", initWithObjectsAndKeys) {
    const auto list = nilTerminated(0);
    for (std::size_t i = 0; i + 1 < list.size(); i += 2) dictSet(self(), list[i + 1], list[i]);
    ret(self());
}
OBJC_METHOD(NSDictionary, "initWithDictionary:", initWithDictionary) {
    if (auto *src = dict(marg(0))) for (auto &[k, v] : src->items) dictSet(self(), k, v);
    ret(self());
}
OBJC_METHOD(NSDictionary, "dealloc", dealloc) {
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) { objc::release(k); objc::release(v); }
    objc::setHostObject(self(), nullptr);
}
OBJC_METHOD(NSDictionary, "count", count) { auto *d = dict(self()); ret(d ? d->items.size() : 0); }
OBJC_METHOD(NSDictionary, "objectForKey:", objectForKey) { ret(dictGet(self(), marg(0))); }
OBJC_METHOD(NSDictionary, "objectForKeyedSubscript:", objectForKeyedSubscript) { ret(dictGet(self(), marg(0))); }
OBJC_METHOD(NSDictionary, "valueForKey:", valueForKey) { ret(dictGet(self(), marg(0))); }
OBJC_METHOD(NSDictionary, "allKeys", allKeys) {
    std::vector<id> keys;
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) keys.push_back(k);
    ret(array(keys));
}
OBJC_METHOD(NSDictionary, "allValues", allValues) {
    std::vector<id> values;
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) values.push_back(v);
    ret(array(values));
}
OBJC_METHOD(NSDictionary, "allKeysForObject:", allKeysForObject) {
    std::vector<id> keys;
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) if (objectsEqual(v, marg(0))) keys.push_back(k);
    ret(array(keys));
}
OBJC_METHOD(NSDictionary, "keyEnumerator", keyEnumerator) {
    std::vector<id> keys;
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) keys.push_back(k);
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {array(keys)}));
}
OBJC_METHOD(NSDictionary, "objectEnumerator", objectEnumerator) {
    std::vector<id> values;
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) values.push_back(v);
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {array(values)}));
}
OBJC_METHOD(NSDictionary, "countByEnumeratingWithState:objects:count:", countByEnumerating) {
    auto *d = dict(self());
    if (!d) return ret(0);
    std::vector<id> keys;
    for (auto &[k, v] : d->items) keys.push_back(k);
    ret(enumerate(keys, d->mutations, marg(0), marg(1), marg(2)));
}
// enumerateKeysAndObjectsUsingBlock: ^(id key, id obj, BOOL *stop)
OBJC_METHOD(NSDictionary, "enumerateKeysAndObjectsUsingBlock:", enumerateKeysAndObjects) {
    auto *d = dict(self());
    if (!d) return;
    const auto block = marg(0);
    const auto stop = mem::calloc(8);
    const auto items = d->items;
    for (auto &[k, v] : items) {
        cpu::call(blockInvoke(block), {block, k, v, stop});
        if (mem::read<std::uint8_t>(stop)) break;
    }
}
OBJC_METHOD(NSDictionary, "copyWithZone:", copyWithZone) { auto *d = dict(self()); ret(dictionary(d ? d->items : std::vector<std::pair<id, id>>{})); }
OBJC_METHOD(NSDictionary, "mutableCopyWithZone:", mutableCopyWithZone) {
    auto *d = dict(self());
    ret(dictionary(d ? d->items : std::vector<std::pair<id, id>>{}, true));
}
OBJC_METHOD(NSDictionary, "isEqualToDictionary:", isEqualToDictionary) {
    auto *a = dict(self()), *b = dict(marg(0));
    if (!a || !b || a->items.size() != b->items.size()) return ret(0);
    for (auto &[k, v] : a->items) if (!objectsEqual(dictGet(marg(0), k), v)) return ret(0);
    ret(1);
}
OBJC_METHOD(NSDictionary, "description", description) {
    std::string out = "{\n";
    if (auto *d = dict(self())) for (auto &[k, v] : d->items) out += "    " + describe(k) + " = " + describe(v) + ";\n";
    out += "}";
    ret(string(out));
}
OBJC_METHOD(NSMutableDictionary, "setObject:forKey:", setObjectForKey) {
    if (!marg(1)) { warn("-[NSMutableDictionary setObject:forKey:] nil key"); return; }
    if (!marg(0)) { warn("-[NSMutableDictionary setObject:forKey:] nil object for %s", describe(marg(1)).c_str()); return; }
    const auto key = isString(marg(1)) ? stringU16(text16(marg(1))) : marg(1);
    dictSet(self(), key, marg(0));
}
OBJC_METHOD(NSMutableDictionary, "setObject:forKeyedSubscript:", setObjectForKeyedSubscript) {
    if (!marg(1)) return;
    if (!marg(0)) { objc::send(self(), "removeObjectForKey:", {marg(1)}); return; }
    dictSet(self(), isString(marg(1)) ? stringU16(text16(marg(1))) : marg(1), marg(0));
}
OBJC_METHOD(NSMutableDictionary, "setValue:forKey:", setValueForKey) {
    if (!marg(0)) { objc::send(self(), "removeObjectForKey:", {marg(1)}); return; }
    dictSet(self(), marg(1), marg(0));
}
OBJC_METHOD(NSMutableDictionary, "removeObjectForKey:", removeObjectForKey) {
    auto *d = dict(self());
    if (!d) return;
    for (auto it = d->items.begin(); it != d->items.end(); ++it)
        if (objectsEqual(it->first, marg(0))) {
            const auto k = it->first, v = it->second;
            d->items.erase(it);
            objc::release(k);
            objc::release(v);
            return;
        }
}
OBJC_METHOD(NSMutableDictionary, "removeObjectsForKeys:", removeObjectsForKeys) {
    for (auto k : arrayItems(marg(0))) objc::send(self(), "removeObjectForKey:", {k});
}
OBJC_METHOD(NSMutableDictionary, "removeAllObjects", removeAllObjects) {
    auto *d = dict(self());
    if (!d) return;
    const auto items = std::move(d->items);
    d->items.clear();
    for (auto &[k, v] : items) { objc::release(k); objc::release(v); }
}
OBJC_METHOD(NSMutableDictionary, "addEntriesFromDictionary:", addEntriesFromDictionary) {
    if (auto *src = dict(marg(0))) for (auto &[k, v] : src->items) dictSet(self(), k, v);
}

// ======================================== NSSet ========================================
OBJC_CLASS(NSSet, NSObject);
OBJC_CLASS(NSMutableSet, NSSet);
static bool mutableSetClass() { return isMutableClass(self(), "NSMutableSet"); }
OBJC_CLASS_METHOD(NSSet, "alloc", alloc) { ret(attachSet(objc::allocInstance(self()), {}, mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "set", set_) { ret(set({}, mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "setWithCapacity:", setWithCapacity) { ret(set({}, mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "setWithObject:", setWithObject) { ret(set({marg(0)}, mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "setWithObjects:", setWithObjects) { ret(set(nilTerminated(0), mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "setWithArray:", setWithArray) { ret(set(arrayItems(marg(0)), mutableSetClass())); }
OBJC_CLASS_METHOD(NSSet, "setWithObjects:count:", setWithObjectsCount) { ret(set(guestList(marg(0), marg(1)), mutableSetClass())); }
OBJC_METHOD(NSSet, "init", init) { ret(self()); }
OBJC_METHOD(NSSet, "initWithCapacity:", initWithCapacity) { ret(self()); }
OBJC_METHOD(NSSet, "initWithArray:", initWithArray) { ret(attachSet(self(), arrayItems(marg(0)), setd(self()) && setd(self())->mutableSet)); }
OBJC_METHOD(NSSet, "initWithObjects:count:", initWithObjectsCount) {
    ret(attachSet(self(), guestList(marg(0), marg(1)), setd(self()) && setd(self())->mutableSet));
}
OBJC_METHOD(NSSet, "count", count) { auto *d = setd(self()); ret(d ? d->items.size() : 0); }
OBJC_METHOD(NSSet, "anyObject", anyObject) { auto *d = setd(self()); ret(d && !d->items.empty() ? d->items.front() : 0); }
OBJC_METHOD(NSSet, "allObjects", allObjects) { ret(array(arrayItems(self()))); }
OBJC_METHOD(NSSet, "containsObject:", containsObject) {
    auto *d = setd(self());
    ret(d && std::any_of(d->items.begin(), d->items.end(), [](id e) { return objectsEqual(e, marg(0)); }));
}
OBJC_METHOD(NSSet, "member:", member) {
    if (auto *d = setd(self())) for (auto e : d->items) if (objectsEqual(e, marg(0))) return ret(e);
    ret(0);
}
OBJC_METHOD(NSSet, "objectEnumerator", objectEnumerator) {
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {array(arrayItems(self()))}));
}
OBJC_METHOD(NSSet, "countByEnumeratingWithState:objects:count:", countByEnumerating) {
    auto *d = setd(self());
    ret(d ? enumerate(d->items, d->mutations, marg(0), marg(1), marg(2)) : 0);
}
OBJC_METHOD(NSSet, "copyWithZone:", copyWithZone) { ret(set(arrayItems(self()))); }
OBJC_METHOD(NSSet, "mutableCopyWithZone:", mutableCopyWithZone) { ret(set(arrayItems(self()), true)); }
OBJC_METHOD(NSSet, "enumerateObjectsUsingBlock:", enumerateObjectsUsingBlock) {
    const auto block = marg(0);
    const auto stop = mem::calloc(8);
    for (auto i : arrayItems(self())) {
        cpu::call(blockInvoke(block), {block, i, stop});
        if (mem::read<std::uint8_t>(stop)) break;
    }
}
OBJC_METHOD(NSMutableSet, "addObject:", addObject) {
    auto *d = setd(self());
    if (!d || !marg(0) || std::any_of(d->items.begin(), d->items.end(), [](id e) { return objectsEqual(e, marg(0)); })) return;
    objc::retain(marg(0));
    d->items.push_back(marg(0));
}
OBJC_METHOD(NSMutableSet, "addObjectsFromArray:", addObjectsFromArray) {
    for (auto i : arrayItems(marg(0))) objc::send(self(), "addObject:", {i});
}
OBJC_METHOD(NSMutableSet, "removeObject:", removeObject) {
    auto *d = setd(self());
    if (!d) return;
    for (auto it = d->items.begin(); it != d->items.end(); ++it)
        if (objectsEqual(*it, marg(0))) { const auto o = *it; d->items.erase(it); objc::release(o); return; }
}
OBJC_METHOD(NSMutableSet, "removeAllObjects", removeAllObjects) {
    auto *d = setd(self());
    if (!d) return;
    for (auto o : d->items) objc::release(o);
    d->items.clear();
}

// ======================================== CFArray / CFDictionary / CFSet ========================================
using hle::arg;
HLE(CFArrayCreate) { ret(array(guestList(arg(1), arg(2)))); }
HLE(CFArrayCreateMutable) { ret(array({}, true)); }
HLE(CFArrayCreateMutableCopy) { ret(array(arrayItems(arg(2)), true)); }
HLE(CFArrayGetCount) { ret(arrayItems(arg(0)).size()); }
HLE(CFArrayGetValueAtIndex) {
    auto *d = objc::host<ArrayData>(arg(0));
    ret(d && arg(1) < d->items.size() ? d->items[std::size_t(arg(1))] : 0);
}
HLE(CFArrayAppendValue) {
    if (auto *d = objc::host<ArrayData>(arg(0))) { objc::retain(arg(1)); d->items.push_back(arg(1)); }
}
HLE(CFArrayRemoveValueAtIndex) {
    if (auto *d = objc::host<ArrayData>(arg(0))) if (arg(1) < d->items.size()) d->items.erase(d->items.begin() + std::ptrdiff_t(arg(1)));
}
HLE(CFArrayRemoveAllValues) { if (auto *d = objc::host<ArrayData>(arg(0))) d->items.clear(); }
HLE(CFDictionaryCreate) {
    std::vector<std::pair<id, id>> items;
    for (std::uint64_t i = 0; i < arg(3); ++i) items.push_back({mem::r64(arg(1) + 8 * i), mem::r64(arg(2) + 8 * i)});
    ret(dictionary(items));
}
HLE(CFDictionaryCreateMutable) { ret(dictionary({}, true)); }
HLE(CFDictionaryCreateMutableCopy) {
    auto *d = objc::host<DictData>(arg(2));
    ret(dictionary(d ? d->items : std::vector<std::pair<id, id>>{}, true));
}
HLE(CFDictionaryGetValue) { ret(dictGet(arg(0), arg(1))); }
HLE(CFDictionaryGetValueIfPresent) {
    const auto v = dictGet(arg(0), arg(1));
    if (arg(2)) mem::w64(arg(2), v);
    ret(v != 0);
}
HLE(CFDictionarySetValue) { dictSet(arg(0), arg(1), arg(2)); }
HLE(CFDictionaryAddValue) { if (!dictGet(arg(0), arg(1))) dictSet(arg(0), arg(1), arg(2)); }
HLE(CFDictionaryRemoveValue) { objc::send(arg(0), "removeObjectForKey:", {arg(1)}); }
HLE(CFDictionaryGetCount) { auto *d = objc::host<DictData>(arg(0)); ret(d ? d->items.size() : 0); }
HLE(CFDictionaryContainsKey) { ret(dictGet(arg(0), arg(1)) != 0); }
// Callback structs: their contents are never read here.
static hle::DataRegistrar d_arrayCallbacks("_kCFTypeArrayCallBacks", [] { return mem::hleAlloc(48); });
static hle::DataRegistrar d_dictKeyCallbacks("_kCFTypeDictionaryKeyCallBacks", [] { return mem::hleAlloc(56); });
static hle::DataRegistrar d_dictValueCallbacks("_kCFTypeDictionaryValueCallBacks", [] { return mem::hleAlloc(48); });
static hle::DataRegistrar d_copyKeyCallbacks("_kCFCopyStringDictionaryKeyCallBacks", [] { return mem::hleAlloc(56); });
static hle::DataRegistrar d_setCallbacks("_kCFTypeSetCallBacks", [] { return mem::hleAlloc(56); });

#pragma once
// Objective-C runtime over the guest's Apple ABI structures (objc4-680, 64-bit): classes are objc_class structs in
// guest memory (isa, superclass, cache, bits -> class_ro_t), selectors are unique C strings, objects start with a raw
// isa pointer. Classes come from the app image (__objc_classlist, __objc_catlist) or from host definitions
// (Foundation/UIKit/... implemented in C++), which get real class structs in guest memory so app classes can subclass
// them. Host methods are HLE stubs reading self/_cmd/arguments from x0, x1, x2...
#include "Hle.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace objc {
using id = std::uint64_t;
using SEL = std::uint64_t;
using Class = std::uint64_t;
constexpr std::uint64_t kIsaMask = 0x0000000ffffffff8ull;

void loadImage(); // registers the app's classes/categories/selectors and runs +load
SEL sel(std::string_view name);
std::string selName(SEL sel);
Class getClass(std::string_view name); // 0 if unknown
Class classOf(id object);
Class superclassOf(Class cls);
std::string className(Class cls);
bool isMetaClass(Class cls);
bool isSubclass(Class cls, Class ancestor);
bool isKindOf(id object, std::string_view className);
std::uint64_t lookup(Class cls, SEL sel); // IMP or 0, triggers +initialize
bool respondsTo(id object, SEL sel);
std::uint32_t instanceSize(Class cls);
void addMethod(Class cls, SEL sel, std::uint64_t imp, const char *types = nullptr);
std::uint64_t methodImplementation(Class cls, SEL sel);
bool conformsTo(Class cls, std::string_view protocol);

// Message sends from host code (integer/pointer arguments; float arguments go in d0..d7).
id send(id receiver, std::string_view selector, std::initializer_list<std::uint64_t> args = {},
        std::initializer_list<double> floatArgs = {});
id sendSuper(id receiver, Class startClass, std::string_view selector, std::initializer_list<std::uint64_t> args = {});

// Objects.
id allocInstance(Class cls, std::size_t extraBytes = 0);
void retain(id object);
void release(id object);
id autorelease(id object);
std::uint64_t retainCount(id object);

// Host-side state attached to objects of host classes (NSString contents, NSArray elements...).
struct HostObject {
    virtual ~HostObject() = default;
};
void setHostObject(id object, std::unique_ptr<HostObject> data);
HostObject *hostObject(id object);
template <class T> T *host(id object) { return dynamic_cast<T *>(hostObject(object)); }

// Host class definitions, registered at static-init time and materialized on first use.
struct ClassDef {
    std::string name, superName;
    std::uint32_t instanceSize = 8;
    std::vector<std::pair<std::string, hle::Handler>> instanceMethods, classMethods;
};
ClassDef &defineClass(const std::string &name, const std::string &superName, std::uint32_t instanceSize = 8);
struct ClassRegistrar {
    ClassRegistrar(const char *name, const char *superName, std::uint32_t size = 8) { defineClass(name, superName ? superName : "", size); }
};
struct MethodRegistrar {
    MethodRegistrar(const char *cls, const char *selector, bool classMethod, hle::Handler h) {
        auto &def = defineClass(cls, "\x01");
        (classMethod ? def.classMethods : def.instanceMethods).push_back({selector, h});
    }
};

// Helpers for handlers: the receiver, the selector, and the i-th method argument (x2 + i).
inline id self() { return cpu::x(0); }
inline SEL cmd() { return cpu::x(1); }
inline std::uint64_t marg(int i) { return cpu::x(2 + i); }
void logUnrecognized(id receiver, SEL sel);
} // namespace objc

// OBJC_CLASS(NSString, NSObject) declares a host class; OBJC_METHOD(NSString, "length", length) { ... } an instance
// method; OBJC_CLASS_METHOD(NSString, "stringWithUTF8String:", stringWithUTF8) { ... } a class method.
#define OBJC_CLASS(name, super) static ::objc::ClassRegistrar HLE_CAT(objc_cls_, name)(#name, #super)
#define OBJC_ROOT_CLASS(name) static ::objc::ClassRegistrar HLE_CAT(objc_cls_, name)(#name, nullptr)
#define OBJC_CLASS_SIZED(name, super, size) static ::objc::ClassRegistrar HLE_CAT(objc_cls_, name)(#name, #super, size)
#define OBJC_METHOD(cls, selector, id)                                                                                 \
    static void HLE_CAT(objc_m_##cls##_, id)();                                                                        \
    static ::objc::MethodRegistrar HLE_CAT(objc_mr_##cls##_, id)(#cls, selector, false, HLE_CAT(objc_m_##cls##_, id)); \
    static void HLE_CAT(objc_m_##cls##_, id)()
#define OBJC_CLASS_METHOD(cls, selector, id)                                                                           \
    static void HLE_CAT(objc_cm_##cls##_, id)();                                                                       \
    static ::objc::MethodRegistrar HLE_CAT(objc_cmr_##cls##_, id)(#cls, selector, true, HLE_CAT(objc_cm_##cls##_, id)); \
    static void HLE_CAT(objc_cm_##cls##_, id)()

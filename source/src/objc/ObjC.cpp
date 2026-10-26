#include "objc/ObjC.h"
#include "Loader.h"
#include "Log.h"
#include "Memory.h"
#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace objc {
namespace {
// class_t: isa +0, superclass +8, cache buckets +16, mask/occupied +24, bits +32 (-> class_ro_t, low 3 bits flags).
// class_ro_t: flags +0, instanceStart +4, instanceSize +8, ivarLayout +16, name +24, baseMethodList +32,
//             baseProtocols +40, ivars +48, weakIvarLayout +56, baseProperties +64.
constexpr std::uint64_t kDataMask = 0x00007ffffffffff8ull;
constexpr std::uint32_t RO_META = 1, RO_ROOT = 2;

struct ClassInfo {
    Class cls = 0;
    std::string name;
    bool meta = false, host = false, initialized = false, initializing = false;
    Class instanceClass = 0; // for a metaclass: its class
    std::uint32_t instanceSize = 8;
    std::unordered_map<SEL, std::uint64_t> methods; // selector -> method_t (name, types, imp) in guest memory
    std::vector<std::string> protocols;
};

std::recursive_mutex g_lock;
std::unordered_map<std::string, SEL> g_selectors;
std::unordered_map<SEL, std::string> g_selectorNames;
std::unordered_map<Class, ClassInfo> g_classes;
std::unordered_map<std::string, Class> g_classByName;
std::map<std::pair<Class, SEL>, std::uint64_t> g_cache;
std::unordered_map<std::string, ClassDef> &defs() { static std::unordered_map<std::string, ClassDef> m; return m; }
std::uint64_t g_emptyCache = 0;
Class g_rootMeta = 0, g_root = 0;

std::mutex g_hostLock;
std::unordered_map<id, std::unique_ptr<HostObject>> g_hostObjects;
std::mutex g_refLock;
std::unordered_map<id, std::int64_t> g_refCounts; // extra references beyond the initial one
std::unordered_set<id> g_deallocated;

std::uint64_t emptyCache() {
    if (!g_emptyCache) g_emptyCache = mem::hleAlloc(16, 16);
    return g_emptyCache;
}
std::uint64_t methodStruct(SEL sel, std::uint64_t imp, const char *types) {
    const auto m = mem::hleAlloc(24, 8);
    mem::w64(m, sel);
    mem::w64(m + 8, types ? mem::hleString(types) : 0);
    mem::w64(m + 16, imp);
    return m;
}
ClassInfo *info(Class cls) {
    auto found = g_classes.find(cls);
    return found == g_classes.end() ? nullptr : &found->second;
}
// Reads a method_list_t (entsize/flags, count, then entries) into the class's method table.
void addMethodList(ClassInfo &ci, std::uint64_t list) {
    if (!list) return;
    const auto entsize = mem::r32(list) & ~3u, count = mem::r32(list + 4);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto m = list + 8 + std::uint64_t(i) * entsize;
        const SEL canonical = sel(mem::cstring(mem::r64(m)));
        mem::w64(m, canonical);
        ci.methods[canonical] = m;
    }
}
void addProtocols(ClassInfo &ci, std::uint64_t list) {
    if (!list) return;
    const auto count = mem::r64(list);
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto proto = mem::r64(list + 8 + 8 * i);
        if (!proto) continue;
        ci.protocols.push_back(mem::cstring(mem::r64(proto + 8)));
        addProtocols(ci, mem::r64(proto + 16)); // inherited protocols
    }
}
// Registers a class struct already present in guest memory (from the app image).
void registerGuestClass(Class cls, bool meta) {
    if (info(cls)) return;
    ClassInfo ci;
    ci.cls = cls;
    ci.meta = meta;
    const auto ro = mem::r64(cls + 32) & kDataMask;
    ci.name = mem::cstring(mem::r64(ro + 24));
    ci.instanceSize = mem::r32(ro + 8);
    addMethodList(ci, mem::r64(ro + 32));
    addProtocols(ci, mem::r64(ro + 40));
    // Empty method caches: nothing in the app reads them, but keep them valid.
    mem::w64(cls + 16, emptyCache());
    g_classes.emplace(cls, std::move(ci));
}

Class materialize(const std::string &name);
// Creates the guest structs for a host class (and its superclasses first).
Class createHostClass(const ClassDef &def) {
    Class super = 0, superMeta = 0;
    if (!def.superName.empty()) {
        super = materialize(def.superName);
        superMeta = mem::r64(super) & kIsaMask;
    }
    const auto cls = mem::hleAlloc(40, 16), meta = mem::hleAlloc(40, 16);
    const auto ro = mem::hleAlloc(72, 8), metaRo = mem::hleAlloc(72, 8);
    const auto nameStr = mem::hleString(def.name);
    mem::w32(ro, def.superName.empty() ? RO_ROOT : 0);
    mem::w32(ro + 4, def.instanceSize);
    mem::w32(ro + 8, def.instanceSize);
    mem::w64(ro + 24, nameStr);
    mem::w32(metaRo, RO_META | (def.superName.empty() ? RO_ROOT : 0));
    mem::w32(metaRo + 4, 40);
    mem::w32(metaRo + 8, 40);
    mem::w64(metaRo + 24, nameStr);
    const bool root = def.superName.empty();
    if (root) { g_root = cls; g_rootMeta = meta; }
    // isa: class -> meta -> root meta; root meta's superclass is the root class.
    mem::w64(cls, meta);
    mem::w64(cls + 8, super);
    mem::w64(cls + 16, emptyCache());
    mem::w64(cls + 32, ro);
    mem::w64(meta, root ? meta : g_rootMeta);
    mem::w64(meta + 8, root ? cls : superMeta);
    mem::w64(meta + 16, emptyCache());
    mem::w64(meta + 32, metaRo);
    ClassInfo ci, mi;
    ci.cls = cls; ci.name = def.name; ci.host = true; ci.instanceSize = def.instanceSize;
    mi.cls = meta; mi.name = def.name; mi.host = true; mi.meta = true; mi.instanceClass = cls; mi.instanceSize = 40;
    for (const auto &[selector, handler] : def.instanceMethods)
        ci.methods[sel(selector)] = methodStruct(sel(selector), hle::makeStub("-[" + def.name + " " + selector + "]", handler), nullptr);
    for (const auto &[selector, handler] : def.classMethods)
        mi.methods[sel(selector)] = methodStruct(sel(selector), hle::makeStub("+[" + def.name + " " + selector + "]", handler), nullptr);
    g_classes.emplace(cls, std::move(ci));
    g_classes.emplace(meta, std::move(mi));
    g_classByName[def.name] = cls;
    return cls;
}
Class materialize(const std::string &name) {
    if (auto found = g_classByName.find(name); found != g_classByName.end()) return found->second;
    auto def = defs().find(name);
    if (def == defs().end()) {
        // A framework class we have no implementation for: an empty NSObject subclass, so messages to it are logged
        // as unrecognized and return nil instead of crashing.
        hlog("objc", "no implementation for class %s; using an empty NSObject subclass", name.c_str());
        auto &d = defineClass(name, "NSObject");
        return createHostClass(d);
    }
    if (def->second.superName == "\x01") def->second.superName = name == "NSObject" ? "" : "NSObject";
    return createHostClass(def->second);
}

// The receiver's class for a lookup; class objects use their metaclass.
SEL g_selInitialize = 0, g_selForwardingTarget = 0, g_selResolveInstance = 0, g_selResolveClass = 0;

void runInitialize(Class cls) {
    // cls is a non-meta class.
    ClassInfo *ci;
    {
        std::lock_guard lock(g_lock);
        ci = info(cls);
        if (!ci || ci->initialized || ci->initializing) return;
        ci->initializing = true;
    }
    if (const auto super = superclassOf(cls)) runInitialize(super);
    const auto meta = classOf(cls);
    std::uint64_t imp = 0;
    for (Class c = meta; c && !imp; c = superclassOf(c)) {
        std::lock_guard lock(g_lock);
        if (auto *i = info(c)) if (auto m = i->methods.find(g_selInitialize); m != i->methods.end()) imp = mem::r64(m->second + 16);
    }
    if (imp) cpu::call(imp, {cls, g_selInitialize});
    std::lock_guard lock(g_lock);
    ci->initializing = false;
    ci->initialized = true;
}
} // namespace

SEL sel(std::string_view name) {
    std::lock_guard lock(g_lock);
    const std::string key(name);
    if (auto found = g_selectors.find(key); found != g_selectors.end()) return found->second;
    const SEL s = mem::hleString(key);
    g_selectors.emplace(key, s);
    g_selectorNames.emplace(s, key);
    return s;
}
std::string selName(SEL s) {
    std::lock_guard lock(g_lock);
    if (auto found = g_selectorNames.find(s); found != g_selectorNames.end()) return found->second;
    return s ? mem::cstring(s) : "(null)";
}
ClassDef &defineClass(const std::string &name, const std::string &superName, std::uint32_t size) {
    auto &d = defs()[name];
    d.name = name;
    if (superName != "\x01") { d.superName = superName; d.instanceSize = size; }
    else if (d.superName.empty() && name != "NSObject") d.superName = "\x01";
    return d;
}
Class getClass(std::string_view name) {
    std::lock_guard lock(g_lock);
    const std::string key(name);
    if (auto found = g_classByName.find(key); found != g_classByName.end()) return found->second;
    if (defs().count(key)) return materialize(key);
    return 0;
}
Class classOf(id object) {
    if (!object) return 0;
    if (object >> 63) return 0; // tagged pointers are never created here
    return mem::r64(object) & kIsaMask;
}
Class superclassOf(Class cls) { return cls ? mem::r64(cls + 8) : 0; }
std::string className(Class cls) {
    std::lock_guard lock(g_lock);
    if (auto *ci = info(cls)) return ci->name;
    return "?";
}
bool isMetaClass(Class cls) {
    std::lock_guard lock(g_lock);
    auto *ci = info(cls);
    return ci && ci->meta;
}
bool isSubclass(Class cls, Class ancestor) {
    for (Class c = cls; c; c = superclassOf(c)) if (c == ancestor) return true;
    return false;
}
bool isKindOf(id object, std::string_view name) {
    const auto target = getClass(name);
    return target && isSubclass(classOf(object), target);
}
std::uint32_t instanceSize(Class cls) {
    std::lock_guard lock(g_lock);
    auto *ci = info(cls);
    return ci ? ci->instanceSize : 8;
}

std::uint64_t lookup(Class cls, SEL s) {
    if (!cls) return 0;
    {
        std::lock_guard lock(g_lock);
        if (auto found = g_cache.find({cls, s}); found != g_cache.end()) return found->second;
    }
    // +initialize on first use of a class (instance or class side).
    {
        ClassInfo *ci;
        Class target;
        {
            std::lock_guard lock(g_lock);
            ci = info(cls);
            target = ci && ci->meta ? ci->instanceClass : cls;
            if (ci && ci->meta && !target) {
                // Metaclass of an app class: its instance class is the class whose isa is this metaclass.
                for (auto &[c, i] : g_classes) if (!i.meta && (mem::r64(c) & kIsaMask) == cls) { ci->instanceClass = c; break; }
                target = ci->instanceClass;
            }
        }
        if (target) {
            std::lock_guard lock(g_lock);
            auto *ti = info(target);
            if (ti && !ti->initialized && !ti->initializing) {
                // Leave the lock before running guest code.
                g_lock.unlock();
                runInitialize(target);
                g_lock.lock();
            }
        }
    }
    std::lock_guard lock(g_lock);
    for (Class c = cls; c; c = superclassOf(c)) {
        auto *ci = info(c);
        if (!ci) {
            warn("lookup through unregistered class %llx", (unsigned long long)c);
            break;
        }
        if (auto m = ci->methods.find(s); m != ci->methods.end()) {
            const auto imp = mem::r64(m->second + 16);
            g_cache[{cls, s}] = imp;
            return imp;
        }
    }
    return 0;
}
bool respondsTo(id object, SEL s) { return lookup(classOf(object), s) != 0; }
void addMethod(Class cls, SEL s, std::uint64_t imp, const char *types) {
    std::lock_guard lock(g_lock);
    if (auto *ci = info(cls)) {
        ci->methods[s] = methodStruct(s, imp, types);
        g_cache.clear();
    }
}
std::uint64_t methodImplementation(Class cls, SEL s) { return lookup(cls, s); }
bool conformsTo(Class cls, std::string_view protocol) {
    std::lock_guard lock(g_lock);
    for (Class c = cls; c; c = superclassOf(c))
        if (auto *ci = info(c))
            for (const auto &p : ci->protocols) if (p == protocol) return true;
    return false;
}

void logUnrecognized(id receiver, SEL s) {
    static std::mutex lock;
    static std::set<std::pair<std::string, std::string>> seen;
    const auto cls = classOf(receiver);
    const bool meta = isMetaClass(cls);
    const auto name = className(cls), selector = selName(s);
    std::lock_guard guard(lock);
    if (seen.insert({name, selector}).second)
        hlog("objc", "unrecognized selector %c[%s %s] (returning nil) from %s", meta ? '+' : '-', name.c_str(), selector.c_str(),
             loader::describe(cpu::lr()).c_str());
}

id send(id receiver, std::string_view selector, std::initializer_list<std::uint64_t> args, std::initializer_list<double> floatArgs) {
    if (!receiver) return 0;
    const SEL s = sel(selector);
    const auto imp = lookup(classOf(receiver), s);
    if (!imp) {
        logUnrecognized(receiver, s);
        return 0;
    }
    std::vector<std::uint64_t> all{receiver, s};
    all.insert(all.end(), args.begin(), args.end());
    switch (all.size()) {
    case 2: return cpu::call(imp, {all[0], all[1]}, floatArgs);
    case 3: return cpu::call(imp, {all[0], all[1], all[2]}, floatArgs);
    case 4: return cpu::call(imp, {all[0], all[1], all[2], all[3]}, floatArgs);
    case 5: return cpu::call(imp, {all[0], all[1], all[2], all[3], all[4]}, floatArgs);
    case 6: return cpu::call(imp, {all[0], all[1], all[2], all[3], all[4], all[5]}, floatArgs);
    case 7: return cpu::call(imp, {all[0], all[1], all[2], all[3], all[4], all[5], all[6]}, floatArgs);
    default: return cpu::call(imp, {all[0], all[1], all[2], all[3], all[4], all[5], all[6], all[7]}, floatArgs);
    }
}
id sendSuper(id receiver, Class startClass, std::string_view selector, std::initializer_list<std::uint64_t> args) {
    if (!receiver) return 0;
    const SEL s = sel(selector);
    const auto imp = lookup(startClass, s);
    if (!imp) { logUnrecognized(receiver, s); return 0; }
    std::vector<std::uint64_t> all{receiver, s};
    all.insert(all.end(), args.begin(), args.end());
    switch (all.size()) {
    case 2: return cpu::call(imp, {all[0], all[1]});
    case 3: return cpu::call(imp, {all[0], all[1], all[2]});
    case 4: return cpu::call(imp, {all[0], all[1], all[2], all[3]});
    default: return cpu::call(imp, {all[0], all[1], all[2], all[3], all[4]});
    }
}

// --- Objects -----------------------------------------------------------------------------------------------------------
id allocInstance(Class cls, std::size_t extra) {
    const auto size = std::max<std::size_t>(instanceSize(cls), 8) + extra;
    const auto object = mem::calloc(size);
    mem::w64(object, cls);
    return object;
}
void setHostObject(id object, std::unique_ptr<HostObject> data) {
    std::lock_guard lock(g_hostLock);
    if (data) g_hostObjects[object] = std::move(data);
    else g_hostObjects.erase(object);
}
HostObject *hostObject(id object) {
    std::lock_guard lock(g_hostLock);
    auto found = g_hostObjects.find(object);
    return found == g_hostObjects.end() ? nullptr : found->second.get();
}
static bool isHleConstant(id object) { return object >= mem::kHleBase && object < mem::kHleBase + mem::kHleSize; }
static bool isImageConstant(id object) { return object >= loader::imageBase() && object < loader::imageEnd(); }
void retain(id object) {
    if (!object || isHleConstant(object) || isImageConstant(object)) return;
    std::lock_guard lock(g_refLock);
    ++g_refCounts[object];
}
std::uint64_t retainCount(id object) {
    if (!object) return 0;
    if (isHleConstant(object) || isImageConstant(object)) return ~0ull >> 1;
    std::lock_guard lock(g_refLock);
    auto found = g_refCounts.find(object);
    return 1 + (found == g_refCounts.end() ? 0 : std::uint64_t(found->second));
}
void release(id object) {
    if (!object || isHleConstant(object) || isImageConstant(object)) return;
    bool dealloc = false;
    {
        std::lock_guard lock(g_refLock);
        auto &count = g_refCounts[object];
        if (count > 0) --count;
        else {
            g_refCounts.erase(object);
            dealloc = g_deallocated.insert(object).second;
        }
    }
    if (dealloc) send(object, "dealloc");
}

// --- Autorelease pools ------------------------------------------------------------------------------------------------
namespace {
thread_local std::vector<id> t_pool;
}
id autorelease(id object) {
    if (object) t_pool.push_back(object);
    return object;
}
static std::uint64_t poolPush() { return t_pool.size() + 1; }
static void poolPop(std::uint64_t token) {
    const std::size_t mark = token ? std::size_t(token - 1) : 0;
    while (t_pool.size() > mark) {
        const auto object = t_pool.back();
        t_pool.pop_back();
        release(object);
    }
}

// --- Image loading ----------------------------------------------------------------------------------------------------
void loadImage() {
    g_selInitialize = sel("initialize");
    g_selForwardingTarget = sel("forwardingTargetForSelector:");
    g_selResolveInstance = sel("resolveInstanceMethod:");
    g_selResolveClass = sel("resolveClassMethod:");
    std::lock_guard lock(g_lock);
    materialize("NSObject");
    // Selector references: point each at the unique selector.
    if (auto *s = loader::section("__DATA", "__objc_selrefs"))
        for (std::uint64_t a = s->address; a < s->address + s->size; a += 8) mem::w64(a, sel(mem::cstring(mem::r64(a))));
    std::size_t classes = 0;
    if (auto *s = loader::section("__DATA", "__objc_classlist"))
        for (std::uint64_t a = s->address; a < s->address + s->size; a += 8) {
            const Class cls = mem::r64(a), meta = mem::r64(cls) & kIsaMask;
            registerGuestClass(cls, false);
            registerGuestClass(meta, true);
            info(meta)->instanceClass = cls;
            g_classByName[info(cls)->name] = cls;
            ++classes;
        }
    std::size_t categories = 0;
    if (auto *s = loader::section("__DATA", "__objc_catlist"))
        for (std::uint64_t a = s->address; a < s->address + s->size; a += 8) {
            const auto cat = mem::r64(a);
            const Class cls = mem::r64(cat + 8);
            auto *ci = info(cls);
            if (!ci) { warn("category %s on unknown class", mem::cstring(mem::r64(cat)).c_str()); continue; }
            addMethodList(*ci, mem::r64(cat + 16));
            if (auto *mi = info(mem::r64(cls) & kIsaMask)) addMethodList(*mi, mem::r64(cat + 24));
            addProtocols(*ci, mem::r64(cat + 32));
            hlog("objc", "category %s on %s", mem::cstring(mem::r64(cat)).c_str(), ci->name.c_str());
            ++categories;
        }
    g_cache.clear();
    hlog("objc", "%zu app classes, %zu categories, %zu selectors", classes, categories, g_selectors.size());
    // +load: classes in __objc_nlclslist, then categories in __objc_nlcatlist.
    std::vector<Class> loads;
    if (auto *s = loader::section("__DATA", "__objc_nlclslist"))
        for (std::uint64_t a = s->address; a < s->address + s->size; a += 8) loads.push_back(mem::r64(a));
    for (const Class cls : loads) {
        auto *mi = info(mem::r64(cls) & kIsaMask);
        if (auto m = mi->methods.find(sel("load")); m != mi->methods.end()) {
            hlog("objc", "+[%s load]", info(cls)->name.c_str());
            g_lock.unlock();
            cpu::call(mem::r64(m->second + 16), {cls, sel("load")});
            g_lock.lock();
        }
    }
}

// --- libobjc exports --------------------------------------------------------------------------------------------------
namespace {
void returnNil() {
    cpu::setX(0, 0);
    cpu::setX(1, 0);
    for (int i = 0; i < 4; ++i) cpu::setD(i, 0);
}
// Unimplemented framework classes behave like simple models: -setFoo: stores x2, -foo returns it.
std::mutex g_bagLock;
std::map<std::pair<id, std::string>, std::uint64_t> g_bag;
bool propertyFallback(id receiver, SEL s) {
    const auto name = selName(s);
    const auto colons = std::count(name.begin(), name.end(), ':');
    if (colons == 1 && name.size() > 4 && name.rfind("set", 0) == 0 && std::isupper(static_cast<unsigned char>(name[3])) && name.back() == ':') {
        std::string key = name.substr(3, name.size() - 4);
        key[0] = char(std::tolower(static_cast<unsigned char>(key[0])));
        std::lock_guard lock(g_bagLock);
        g_bag[{receiver, key}] = cpu::x(2);
        return true;
    }
    if (colons == 0) {
        std::lock_guard lock(g_bagLock);
        auto found = g_bag.find({receiver, name});
        if (found != g_bag.end()) { cpu::setX(0, found->second); return true; }
    }
    return false;
}
// objc_msgSend: looks up the method and tail-calls it with the registers untouched.
void msgSend() {
    const id receiver = cpu::x(0);
    const SEL s = cpu::x(1);
    if (!receiver) return returnNil();
    if (receiver >> 63) { warn("message %s to tagged pointer %llx", selName(s).c_str(), (unsigned long long)receiver); return returnNil(); }
    const Class cls = classOf(receiver);
    auto imp = lookup(cls, s);
    if (!imp) {
        // forwardingTargetForSelector:
        if (const auto fwd = lookup(cls, g_selForwardingTarget)) {
            const auto target = cpu::call(fwd, {receiver, g_selForwardingTarget, s});
            if (target && target != receiver) {
                cpu::setX(0, target);
                if ((imp = lookup(classOf(target), s))) return cpu::setPc(imp);
            }
        }
        logUnrecognized(receiver, s);
        returnNil();
        if (propertyFallback(receiver, s)) return;
        return;
    }
    if (logEnabled("msg")) {
        static const bool on = std::getenv("HLE_TRACE_MSG") != nullptr;
        if (on) hlog("msg", "%c[%s %s]", isMetaClass(cls) ? '+' : '-', className(cls).c_str(), selName(s).c_str());
    }
    cpu::setPc(imp);
}
// objc_msgSendSuper2(struct objc_super *{receiver, current class}, SEL, ...): starts at the current class's superclass.
void msgSendSuper2() {
    const auto super = cpu::x(0);
    const id receiver = mem::r64(super);
    const Class current = mem::r64(super + 8);
    const SEL s = cpu::x(1);
    cpu::setX(0, receiver);
    if (!receiver) return returnNil();
    const auto imp = lookup(superclassOf(current), s);
    if (!imp) { logUnrecognized(receiver, s); return returnNil(); }
    cpu::setPc(imp);
}
hle::Registrar r_msgSend("_objc_msgSend", msgSend);
hle::Registrar r_msgSendSuper2("_objc_msgSendSuper2", msgSendSuper2);
hle::Registrar r_msgSendSuper("_objc_msgSendSuper", [] {
    const auto super = cpu::x(0);
    const id receiver = mem::r64(super);
    const Class start = mem::r64(super + 8);
    cpu::setX(0, receiver);
    if (!receiver) return returnNil();
    const auto imp = lookup(start, cpu::x(1));
    if (!imp) { logUnrecognized(receiver, cpu::x(1)); return returnNil(); }
    cpu::setPc(imp);
});
hle::DataRegistrar d_emptyCache("__objc_empty_cache", [] { return emptyCache(); });
const bool g_classImports = [] {
    hle::registerDataPrefix("_OBJC_CLASS_$_", [](const std::string &name) {
        std::lock_guard lock(g_lock);
        return materialize(name.substr(14));
    });
    hle::registerDataPrefix("_OBJC_METACLASS_$_", [](const std::string &name) {
        std::lock_guard lock(g_lock);
        return mem::r64(materialize(name.substr(18))) & kIsaMask;
    });
    hle::registerDataPrefix("_OBJC_EHTYPE_$_", [](const std::string &name) {
        // objc_ehtype_t {vtable, name, cls}
        const auto e = mem::hleAlloc(24, 8);
        mem::w64(e + 8, mem::hleString(name.substr(15)));
        std::lock_guard lock(g_lock);
        mem::w64(e + 16, materialize(name.substr(15)));
        return e;
    });
    return true;
}();
} // namespace

HLE(objc_retain) { retain(hle::arg(0)); }
HLE(objc_release) { release(hle::arg(0)); }
HLE(objc_autorelease) { autorelease(hle::arg(0)); }
HLE(objc_autoreleaseReturnValue) { autorelease(hle::arg(0)); }
HLE(objc_retainAutorelease) { retain(hle::arg(0)); autorelease(hle::arg(0)); }
HLE(objc_retainAutoreleaseReturnValue) { retain(hle::arg(0)); autorelease(hle::arg(0)); }
HLE(objc_retainAutoreleasedReturnValue) { retain(hle::arg(0)); }
HLE(objc_autoreleasePoolPush) { hle::ret(poolPush()); }
HLE(objc_autoreleasePoolPop) { poolPop(hle::arg(0)); }
HLE(objc_storeStrong) {
    const auto location = hle::arg(0), object = hle::arg(1);
    const auto old = mem::r64(location);
    if (old == object) return;
    retain(object);
    mem::w64(location, object);
    release(old);
}
// Weak references (objects are never freed, so they never need zeroing).
HLE(objc_storeWeak) { mem::w64(hle::arg(0), hle::arg(1)); }
HLE(objc_initWeak) { mem::w64(hle::arg(0), hle::arg(1)); }
HLE(objc_destroyWeak) { mem::w64(hle::arg(0), 0); }
HLE(objc_loadWeakRetained) { const auto o = mem::r64(hle::arg(0)); retain(o); hle::ret(o); }
HLE(objc_loadWeak) { hle::ret(mem::r64(hle::arg(0))); }
HLE(objc_copyWeak) { mem::w64(hle::arg(0), mem::r64(hle::arg(1))); }
HLE(objc_moveWeak) { mem::w64(hle::arg(0), mem::r64(hle::arg(1))); mem::w64(hle::arg(1), 0); }
// Properties: objc_getProperty(self, _cmd, offset, atomic), objc_setProperty(self, _cmd, offset, value, atomic, copy).
HLE(objc_getProperty) { hle::ret(mem::r64(hle::arg(0) + hle::arg(2))); }
static void setProperty(id self, std::uint64_t offset, id value, bool copy, bool mutableCopy) {
    if (copy) value = send(value, "copyWithZone:", {0});
    else if (mutableCopy) value = send(value, "mutableCopyWithZone:", {0});
    else retain(value);
    const auto old = mem::r64(self + offset);
    mem::w64(self + offset, value);
    release(old);
}
HLE(objc_setProperty) { setProperty(hle::arg(0), hle::arg(2), hle::arg(3), hle::arg(5) != 0 && (hle::arg(5) & 0xFF) != 2, (hle::arg(5) & 0xFF) == 2); }
HLE(objc_setProperty_atomic) { setProperty(hle::arg(0), hle::arg(3), hle::arg(2), false, false); }
HLE(objc_setProperty_nonatomic) { setProperty(hle::arg(0), hle::arg(3), hle::arg(2), false, false); }
HLE(objc_setProperty_atomic_copy) { setProperty(hle::arg(0), hle::arg(3), hle::arg(2), true, false); }
HLE(objc_setProperty_nonatomic_copy) { setProperty(hle::arg(0), hle::arg(3), hle::arg(2), true, false); }
HLE(objc_copyStruct) { std::memmove(mem::base() + hle::arg(0), mem::base() + hle::arg(1), std::size_t(hle::arg(2))); }
// @synchronized
namespace {
std::mutex g_syncLock;
std::unordered_map<id, std::unique_ptr<std::recursive_mutex>> g_syncs;
std::recursive_mutex &syncFor(id object) {
    std::lock_guard lock(g_syncLock);
    auto &m = g_syncs[object];
    if (!m) m = std::make_unique<std::recursive_mutex>();
    return *m;
}
} // namespace
HLE(objc_sync_enter) { if (hle::arg(0)) syncFor(hle::arg(0)).lock(); hle::ret(0); }
HLE(objc_sync_exit) { if (hle::arg(0)) syncFor(hle::arg(0)).unlock(); hle::ret(0); }
// Associated objects.
namespace {
std::mutex g_assocLock;
std::map<std::pair<id, std::uint64_t>, id> g_assoc;
} // namespace
HLE(objc_setAssociatedObject) {
    std::lock_guard lock(g_assocLock);
    retain(hle::arg(2));
    g_assoc[{hle::arg(0), hle::arg(1)}] = hle::arg(2);
}
HLE(objc_getAssociatedObject) {
    std::lock_guard lock(g_assocLock);
    auto found = g_assoc.find({hle::arg(0), hle::arg(1)});
    hle::ret(found == g_assoc.end() ? 0 : found->second);
}
HLE(objc_enumerationMutation) { warn("collection mutated while being enumerated (%llx)", (unsigned long long)hle::arg(0)); }
HLE(objc_terminate) { cpu::backtrace("objc_terminate"); fatal("objc_terminate"); }
HLE(objc_exception_throw) {
    const id e = hle::arg(0);
    const auto name = send(e, "name"), reason = send(e, "reason");
    const auto text = [](id s) { return s ? mem::cstring(send(s, "UTF8String")) : std::string("(nil)"); };
    cpu::backtrace("Objective-C exception");
    fatal("uncaught Objective-C exception %s: %s", text(name).c_str(), text(reason).c_str());
}
HLE(objc_exception_rethrow) { cpu::backtrace("objc_exception_rethrow"); fatal("objc_exception_rethrow"); }
HLE(objc_begin_catch) { hle::ret(hle::arg(0)); }
HLE(objc_end_catch) {}
HLE(objc_getClass) { hle::ret(getClass(mem::cstring(hle::arg(0)))); }
HLE(objc_lookUpClass) { hle::ret(getClass(mem::cstring(hle::arg(0)))); }
HLE(object_getClass) { hle::ret(classOf(hle::arg(0))); }
HLE(class_getName) { hle::ret(hle::arg(0) ? mem::r64((mem::r64(hle::arg(0) + 32) & kDataMask) + 24) : 0); }
HLE(class_getSuperclass) { hle::ret(superclassOf(hle::arg(0))); }
HLE(sel_registerName) { hle::ret(sel(mem::cstring(hle::arg(0)))); }
HLE(sel_getName) { hle::ret(hle::arg(0)); }
HLE(class_addMethod) {
    const auto cls = hle::arg(0), s = hle::arg(1);
    {
        std::lock_guard lock(g_lock);
        auto *ci = info(cls);
        if (!ci || ci->methods.count(s)) return hle::ret(0);
    }
    addMethod(cls, s, hle::arg(2), mem::cstring(hle::arg(3)).c_str());
    hle::ret(1);
}
// Method handles are the method_t structs.
HLE(class_getInstanceMethod) {
    std::lock_guard lock(g_lock);
    for (Class c = hle::arg(0); c; c = superclassOf(c))
        if (auto *ci = info(c))
            if (auto m = ci->methods.find(hle::arg(1)); m != ci->methods.end()) return hle::ret(m->second);
    hle::ret(0);
}
HLE(class_getClassMethod) {
    std::lock_guard lock(g_lock);
    for (Class c = classOf(hle::arg(0)); c; c = superclassOf(c))
        if (auto *ci = info(c))
            if (auto m = ci->methods.find(hle::arg(1)); m != ci->methods.end()) return hle::ret(m->second);
    hle::ret(0);
}
HLE(class_getMethodImplementation) {
    auto imp = lookup(hle::arg(0), hle::arg(1));
    if (!imp) imp = hle::resolveImport("__objc_msgForward");
    hle::ret(imp);
}
HLE(class_respondsToSelector) { hle::ret(lookup(hle::arg(0), hle::arg(1)) != 0); }
HLE(method_getImplementation) { hle::ret(hle::arg(0) ? mem::r64(hle::arg(0) + 16) : 0); }
HLE(method_setImplementation) {
    const auto old = mem::r64(hle::arg(0) + 16);
    mem::w64(hle::arg(0) + 16, hle::arg(1));
    std::lock_guard lock(g_lock);
    g_cache.clear();
    hle::ret(old);
}
HLE(method_exchangeImplementations) {
    const auto a = hle::arg(0), b = hle::arg(1);
    const auto ia = mem::r64(a + 16), ib = mem::r64(b + 16);
    mem::w64(a + 16, ib);
    mem::w64(b + 16, ia);
    std::lock_guard lock(g_lock);
    g_cache.clear();
}
HLE(method_getName) { hle::ret(hle::arg(0) ? mem::r64(hle::arg(0)) : 0); }
HLE(method_getTypeEncoding) { hle::ret(hle::arg(0) ? mem::r64(hle::arg(0) + 8) : 0); }
HLE(protocol_conformsToProtocol) {
    const auto a = hle::arg(0), b = hle::arg(1);
    if (!a || !b) return hle::ret(0);
    hle::ret(mem::cstring(mem::r64(a + 8)) == mem::cstring(mem::r64(b + 8)));
}
HLE(protocol_copyMethodDescriptionList) { if (hle::arg(3)) mem::w32(hle::arg(3), 0); hle::ret(0); }
HLE(protocol_copyProtocolList) { if (hle::arg(1)) mem::w32(hle::arg(1), 0); hle::ret(0); }
HLE(_objc_msgForward) {
    logUnrecognized(cpu::x(0), cpu::x(1));
    returnNil();
}

// --- NSObject ---------------------------------------------------------------------------------------------------------
OBJC_ROOT_CLASS(NSObject);
OBJC_CLASS_METHOD(NSObject, "alloc", alloc) { hle::ret(allocInstance(self())); }
OBJC_CLASS_METHOD(NSObject, "allocWithZone:", allocWithZone) { hle::ret(allocInstance(self())); }
OBJC_CLASS_METHOD(NSObject, "new", new_) { hle::ret(send(allocInstance(self()), "init")); }
OBJC_CLASS_METHOD(NSObject, "class", cls) { hle::ret(self()); }
OBJC_CLASS_METHOD(NSObject, "superclass", superclass) { hle::ret(superclassOf(self())); }
OBJC_CLASS_METHOD(NSObject, "initialize", initialize) {}
OBJC_CLASS_METHOD(NSObject, "load", load) {}
OBJC_CLASS_METHOD(NSObject, "isSubclassOfClass:", isSubclassOfClass) { hle::ret(isSubclass(self(), marg(0))); }
OBJC_CLASS_METHOD(NSObject, "instancesRespondToSelector:", instancesRespond) { hle::ret(lookup(self(), marg(0)) != 0); }
OBJC_CLASS_METHOD(NSObject, "respondsToSelector:", classResponds) { hle::ret(lookup(classOf(self()), marg(0)) != 0); }
OBJC_CLASS_METHOD(NSObject, "conformsToProtocol:", classConforms) {
    hle::ret(marg(0) && conformsTo(self(), mem::cstring(mem::r64(marg(0) + 8))));
}
OBJC_CLASS_METHOD(NSObject, "instanceMethodForSelector:", instanceMethodFor) { hle::ret(lookup(self(), marg(0))); }
OBJC_CLASS_METHOD(NSObject, "resolveInstanceMethod:", resolveInstance) { hle::ret(0); }
OBJC_CLASS_METHOD(NSObject, "resolveClassMethod:", resolveClass) { hle::ret(0); }
OBJC_CLASS_METHOD(NSObject, "retain", classRetain) { hle::ret(self()); }
OBJC_CLASS_METHOD(NSObject, "release", classRelease) {}
OBJC_CLASS_METHOD(NSObject, "autorelease", classAutorelease) { hle::ret(self()); }
OBJC_CLASS_METHOD(NSObject, "self", classSelf) { hle::ret(self()); }
OBJC_CLASS_METHOD(NSObject, "isKindOfClass:", classIsKind) { hle::ret(isSubclass(classOf(self()), marg(0))); }
OBJC_CLASS_METHOD(NSObject, "description", classDescription) { hle::ret(send(getClass("NSString"), "stringWithUTF8String:", {mem::heapString(className(self()))})); }
OBJC_CLASS_METHOD(NSObject, "hash", classHash) { hle::ret(self()); }
OBJC_CLASS_METHOD(NSObject, "isEqual:", classIsEqual) { hle::ret(self() == marg(0)); }
OBJC_METHOD(NSObject, "init", init) { hle::ret(self()); }
OBJC_METHOD(NSObject, "dealloc", dealloc) {
    // Memory is not reused (stale references in emulated code then stay harmless); host state is dropped.
    setHostObject(self(), nullptr);
}
OBJC_METHOD(NSObject, "finalize", finalize) {}
OBJC_METHOD(NSObject, "retain", retain_) { retain(self()); hle::ret(self()); }
OBJC_METHOD(NSObject, "release", release_) { release(self()); }
OBJC_METHOD(NSObject, "autorelease", autorelease_) { autorelease(self()); hle::ret(self()); }
OBJC_METHOD(NSObject, "retainCount", retainCount_) { hle::ret(retainCount(self())); }
OBJC_METHOD(NSObject, "class", cls) { hle::ret(classOf(self())); }
OBJC_METHOD(NSObject, "superclass", superclass) { hle::ret(superclassOf(classOf(self()))); }
OBJC_METHOD(NSObject, "self", self_) { hle::ret(self()); }
OBJC_METHOD(NSObject, "zone", zone) { hle::ret(0); }
OBJC_METHOD(NSObject, "hash", hash) { hle::ret(self()); }
OBJC_METHOD(NSObject, "isEqual:", isEqual) { hle::ret(self() == marg(0)); }
OBJC_METHOD(NSObject, "isKindOfClass:", isKindOfClass) { hle::ret(isSubclass(classOf(self()), marg(0))); }
OBJC_METHOD(NSObject, "isMemberOfClass:", isMemberOfClass) { hle::ret(classOf(self()) == marg(0)); }
OBJC_METHOD(NSObject, "respondsToSelector:", respondsToSelector) { hle::ret(lookup(classOf(self()), marg(0)) != 0); }
OBJC_METHOD(NSObject, "conformsToProtocol:", conformsToProtocol) {
    hle::ret(marg(0) && conformsTo(classOf(self()), mem::cstring(mem::r64(marg(0) + 8))));
}
OBJC_METHOD(NSObject, "methodForSelector:", methodForSelector) { hle::ret(lookup(classOf(self()), marg(0))); }
OBJC_METHOD(NSObject, "isProxy", isProxy) { hle::ret(0); }
OBJC_METHOD(NSObject, "copy", copy) { hle::ret(send(self(), "copyWithZone:", {0})); }
OBJC_METHOD(NSObject, "mutableCopy", mutableCopy) { hle::ret(send(self(), "mutableCopyWithZone:", {0})); }
OBJC_METHOD(NSObject, "performSelector:", performSelector) { hle::ret(send(self(), selName(marg(0)))); }
OBJC_METHOD(NSObject, "performSelector:withObject:", performSelector1) { hle::ret(send(self(), selName(marg(0)), {marg(1)})); }
OBJC_METHOD(NSObject, "performSelector:withObject:withObject:", performSelector2) {
    hle::ret(send(self(), selName(marg(0)), {marg(1), marg(2)}));
}
OBJC_METHOD(NSObject, "description", description) {
    char text[128];
    std::snprintf(text, sizeof text, "<%s: 0x%llx>", className(classOf(self())).c_str(), (unsigned long long)self());
    hle::ret(send(getClass("NSString"), "stringWithUTF8String:", {mem::heapString(text)}));
}
OBJC_METHOD(NSObject, "debugDescription", debugDescription) { hle::ret(send(self(), "description")); }
OBJC_METHOD(NSObject, "forwardingTargetForSelector:", forwardingTarget) { hle::ret(0); }
OBJC_METHOD(NSObject, "awakeFromNib", awakeFromNib) {}
OBJC_METHOD(NSObject, "willChangeValueForKey:", willChange) {}
OBJC_METHOD(NSObject, "didChangeValueForKey:", didChange) {}
OBJC_METHOD(NSObject, "addObserver:forKeyPath:options:context:", addObserver) {}
OBJC_METHOD(NSObject, "removeObserver:forKeyPath:", removeObserver) {}

// NSAutoreleasePool
OBJC_METHOD(NSAutoreleasePool, "init", init) {
    // The token is kept in the object's second word.
    mem::w64(self() + 8, poolPush());
    hle::ret(self());
}
OBJC_METHOD(NSAutoreleasePool, "drain", drain) { poolPop(mem::r64(self() + 8)); }
OBJC_METHOD(NSAutoreleasePool, "release", release_) { poolPop(mem::r64(self() + 8)); }
OBJC_CLASS_SIZED(NSAutoreleasePool, NSObject, 16);
} // namespace objc

// Foundation services: NSBundle, NSFileManager, NSUserDefaults, keychain (SecItem), NSNotificationCenter, NSDate,
// NSLocale, NSTimeZone, NSProcessInfo, NSThread, NSRunLoop, NSTimer, NSURL, NSError, NSUUID, locks, CF helpers.
#include "foundation/Foundation.h"
#include "foundation/PList.h"
#include "foundation/RunLoop.h"
#include "Log.h"
#include "Memory.h"
#include "Runtime.h"
#include "libc/Files.h"
#include "libc/System.h"
#include "platform/Platform.h"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>

namespace fs = std::filesystem;
using namespace ns;
using objc::marg;
using objc::self;
using hle::arg;
using hle::ret;

namespace ns {
double now() {
    const auto unixSeconds = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    return unixSeconds - 978307200.0;
}
} // namespace ns

namespace {
std::string bundlePath() { return files::kBundlePath; }
std::string homePath() { return files::kHomePath; }
id singleton(const char *cls, id &slot) {
    if (!slot) {
        slot = objc::allocInstance(objc::getClass(cls));
        objc::retain(slot);
    }
    return slot;
}
} // namespace

// ======================================== NSDate ========================================
struct DateData : objc::HostObject { double t = 0; }; // seconds since 2001-01-01
static id makeDate(double t) {
    const auto o = objc::allocInstance(objc::getClass("NSDate"));
    auto d = std::make_unique<DateData>();
    d->t = t;
    objc::setHostObject(o, std::move(d));
    return o;
}
static double dateValue(id o) { auto *d = objc::host<DateData>(o); return d ? d->t : 0; }
id ns::dateWithTimeIntervalSinceReferenceDate(double s) { return makeDate(s); }
OBJC_CLASS(NSDate, NSObject);
OBJC_CLASS_METHOD(NSDate, "date", date) { ret(makeDate(now())); }
OBJC_CLASS_METHOD(NSDate, "alloc", alloc) { const auto o = objc::allocInstance(self()); objc::setHostObject(o, std::make_unique<DateData>()); ret(o); }
OBJC_METHOD(NSDate, "init", init) { objc::host<DateData>(self())->t = now(); ret(self()); }
OBJC_METHOD(NSDate, "initWithTimeIntervalSinceNow:", initSinceNow) { objc::host<DateData>(self())->t = now() + cpu::d(0); ret(self()); }
OBJC_METHOD(NSDate, "initWithTimeIntervalSince1970:", initSince1970) { objc::host<DateData>(self())->t = cpu::d(0) - 978307200.0; ret(self()); }
OBJC_METHOD(NSDate, "initWithTimeIntervalSinceReferenceDate:", initSinceRef) { objc::host<DateData>(self())->t = cpu::d(0); ret(self()); }
OBJC_CLASS_METHOD(NSDate, "dateWithTimeIntervalSinceNow:", sinceNow) { ret(makeDate(now() + cpu::d(0))); }
OBJC_CLASS_METHOD(NSDate, "dateWithTimeIntervalSince1970:", since1970) { ret(makeDate(cpu::d(0) - 978307200.0)); }
OBJC_CLASS_METHOD(NSDate, "dateWithTimeIntervalSinceReferenceDate:", sinceRef) { ret(makeDate(cpu::d(0))); }
OBJC_CLASS_METHOD(NSDate, "distantFuture", distantFuture) { ret(makeDate(63113904000.0)); }
OBJC_CLASS_METHOD(NSDate, "distantPast", distantPast) { ret(makeDate(-63114076800.0)); }
OBJC_CLASS_METHOD(NSDate, "timeIntervalSinceReferenceDate", classSinceRef) { hle::retDouble(now()); }
OBJC_METHOD(NSDate, "timeIntervalSinceReferenceDate", sinceRefValue) { hle::retDouble(dateValue(self())); }
OBJC_METHOD(NSDate, "timeIntervalSince1970", since1970Value) { hle::retDouble(dateValue(self()) + 978307200.0); }
OBJC_METHOD(NSDate, "timeIntervalSinceNow", sinceNowValue) { hle::retDouble(dateValue(self()) - now()); }
OBJC_METHOD(NSDate, "timeIntervalSinceDate:", sinceDate) { hle::retDouble(dateValue(self()) - dateValue(marg(0))); }
OBJC_METHOD(NSDate, "dateByAddingTimeInterval:", adding) { ret(makeDate(dateValue(self()) + cpu::d(0))); }
OBJC_METHOD(NSDate, "compare:", compare) {
    const double a = dateValue(self()), b = dateValue(marg(0));
    ret(std::uint64_t(std::int64_t(a < b ? -1 : a > b ? 1 : 0)));
}
OBJC_METHOD(NSDate, "isEqualToDate:", isEqualToDate) { ret(dateValue(self()) == dateValue(marg(0))); }
OBJC_METHOD(NSDate, "earlierDate:", earlierDate) { ret(dateValue(self()) <= dateValue(marg(0)) ? self() : marg(0)); }
OBJC_METHOD(NSDate, "laterDate:", laterDate) { ret(dateValue(self()) >= dateValue(marg(0)) ? self() : marg(0)); }
OBJC_METHOD(NSDate, "description", description) {
    const std::time_t t = std::time_t(dateValue(self()) + 978307200.0);
    const std::tm tm = platform::utcTime(t);
    char b[64];
    std::strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S +0000", &tm);
    ret(string(b));
}
OBJC_METHOD(NSDate, "copyWithZone:", copyWithZone) { ret(self()); }
HLE(CFAbsoluteTimeGetCurrent) { hle::retDouble(now()); }
HLE(CFDateCreate) { ret(makeDate(cpu::d(0))); }
HLE(CFDateGetAbsoluteTime) { hle::retDouble(dateValue(arg(0))); }

// ======================================== NSBundle ========================================
static id g_mainBundle = 0;
static id infoDictionary() {
    static id info = 0;
    if (!info) {
        info = dictionary({
            {string("CFBundleIdentifier"), string("com.supercell.scroll")},
            {string("CFBundleName"), string("Clash Royale")},
            {string("CFBundleDisplayName"), string("Clash Royale")},
            {string("CFBundleExecutable"), string("Clash Royale")},
            {string("CFBundleShortVersionString"), string("1.0")},
            {string("CFBundleVersion"), string("2.1306")},
            {string("MinimumOSVersion"), string("7.0")},
            {string("UIPrerenderedIcon"), numberBool(true)},
            {string("FacebookAppID"), string("000000000000000")},
            {string("FacebookDisplayName"), string("Clash Royale")},
        });
        objc::retain(info);
    }
    return info;
}
OBJC_CLASS(NSBundle, NSObject);
OBJC_CLASS_METHOD(NSBundle, "mainBundle", mainBundle) { ret(singleton("NSBundle", g_mainBundle)); }
OBJC_CLASS_METHOD(NSBundle, "bundleForClass:", bundleForClass) { ret(singleton("NSBundle", g_mainBundle)); }
OBJC_CLASS_METHOD(NSBundle, "bundleWithPath:", bundleWithPath) { ret(0); }
OBJC_CLASS_METHOD(NSBundle, "bundleWithIdentifier:", bundleWithIdentifier) { ret(0); }
OBJC_METHOD(NSBundle, "bundlePath", bundlePath_) { ret(string(bundlePath())); }
OBJC_METHOD(NSBundle, "resourcePath", resourcePath) { ret(string(bundlePath())); }
OBJC_METHOD(NSBundle, "executablePath", executablePath) { ret(string(bundlePath() + "/Clash Royale")); }
OBJC_METHOD(NSBundle, "bundleIdentifier", bundleIdentifier) { ret(string("com.supercell.scroll")); }
OBJC_METHOD(NSBundle, "bundleURL", bundleURL) { ret(objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(bundlePath())})); }
OBJC_METHOD(NSBundle, "resourceURL", resourceURL) { ret(objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(bundlePath())})); }
OBJC_METHOD(NSBundle, "infoDictionary", infoDictionary_) { ret(infoDictionary()); }
OBJC_METHOD(NSBundle, "localizedInfoDictionary", localizedInfoDictionary) { ret(infoDictionary()); }
OBJC_METHOD(NSBundle, "objectForInfoDictionaryKey:", objectForInfoDictionaryKey) { ret(dictGet(infoDictionary(), marg(0))); }
OBJC_METHOD(NSBundle, "preferredLocalizations", preferredLocalizations) { ret(array({string("en")})); }
OBJC_METHOD(NSBundle, "localizations", localizations) { ret(array({string("en")})); }
OBJC_METHOD(NSBundle, "localizedStringForKey:value:table:", localizedString) { ret(marg(1) ? marg(1) : marg(0)); }
OBJC_METHOD(NSBundle, "loadNibNamed:owner:options:", loadNibNamed) { ret(array({})); }
static std::string resourcePathFor(id name, id type, id directory) {
    std::string file = text(name);
    if (type && !text(type).empty()) file += "." + text(type);
    std::string rel = directory ? text(directory) + "/" + file : file;
    const auto guest = bundlePath() + "/" + rel;
    return fs::exists(fs::u8path(files::hostPath(guest))) ? guest : std::string();
}
OBJC_METHOD(NSBundle, "pathForResource:ofType:", pathForResource) {
    const auto p = resourcePathFor(marg(0), marg(1), 0);
    if (p.empty()) hlog("bundle", "missing resource %s.%s", text(marg(0)).c_str(), text(marg(1)).c_str());
    ret(p.empty() ? 0 : string(p));
}
OBJC_METHOD(NSBundle, "pathForResource:ofType:inDirectory:", pathForResourceInDirectory) {
    const auto p = resourcePathFor(marg(0), marg(1), marg(2));
    ret(p.empty() ? 0 : string(p));
}
OBJC_METHOD(NSBundle, "URLForResource:withExtension:", URLForResource) {
    const auto p = resourcePathFor(marg(0), marg(1), 0);
    ret(p.empty() ? 0 : objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(p)}));
}
HLE(CFBundleGetMainBundle) { ret(singleton("NSBundle", g_mainBundle)); }
HLE(CFBundleGetValueForInfoDictionaryKey) { ret(dictGet(infoDictionary(), arg(1))); }
HLE(CFBundleGetInfoDictionary) { ret(infoDictionary()); }
HLE(CFBundleGetIdentifier) { ret(string("com.supercell.scroll")); }
HLE(CFBundleCopyResourcesDirectoryURL) { ret(objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(bundlePath())})); }
HLE(CFBundleCopyBundleURL) { ret(objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(bundlePath())})); }

// ======================================== Paths & NSFileManager ========================================
// NSSearchPathDirectory: Library 5, Document 9, Caches 13, ApplicationSupport 14.
HLE(NSSearchPathForDirectoriesInDomains) {
    std::string path;
    switch (arg(0)) {
    case 9: path = homePath() + "/Documents"; break;
    case 13: path = homePath() + "/Library/Caches"; break;
    case 5: path = homePath() + "/Library"; break;
    case 14: path = homePath() + "/Library/Application Support"; break;
    default: path = homePath() + "/Documents";
    }
    ret(array({string(path)}));
}
HLE(NSTemporaryDirectory) { ret(string(homePath() + "/tmp/")); }
HLE(NSHomeDirectory) { ret(string(homePath())); }
static id g_fileManager = 0;
static fs::path hostOf(id path) { return fs::u8path(files::hostPath(text(path))); }
static void setError(std::uint64_t errorOut, const char *what) {
    if (errorOut) mem::w64(errorOut, objc::send(objc::getClass("NSError"), "errorWithDomain:code:userInfo:", {string("NSCocoaErrorDomain"), 4, 0}));
    (void)what;
}
OBJC_CLASS(NSFileManager, NSObject);
OBJC_CLASS_METHOD(NSFileManager, "defaultManager", defaultManager) { ret(singleton("NSFileManager", g_fileManager)); }
OBJC_METHOD(NSFileManager, "fileExistsAtPath:", fileExists) { std::error_code ec; ret(fs::exists(hostOf(marg(0)), ec)); }
OBJC_METHOD(NSFileManager, "fileExistsAtPath:isDirectory:", fileExistsIsDirectory) {
    std::error_code ec;
    const auto p = hostOf(marg(0));
    const bool exists = fs::exists(p, ec);
    if (marg(1)) mem::write<std::uint8_t>(marg(1), exists && fs::is_directory(p, ec));
    ret(exists);
}
OBJC_METHOD(NSFileManager, "isReadableFileAtPath:", isReadable) { std::error_code ec; ret(fs::exists(hostOf(marg(0)), ec)); }
OBJC_METHOD(NSFileManager, "isWritableFileAtPath:", isWritable) { ret(1); }
OBJC_METHOD(NSFileManager, "isDeletableFileAtPath:", isDeletable) { ret(1); }
OBJC_METHOD(NSFileManager, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", createDirectory) {
    std::error_code ec;
    fs::create_directories(hostOf(marg(0)), ec);
    ret(1);
}
OBJC_METHOD(NSFileManager, "createDirectoryAtURL:withIntermediateDirectories:attributes:error:", createDirectoryURL) {
    std::error_code ec;
    fs::create_directories(hostOf(objc::send(marg(0), "path")), ec);
    ret(1);
}
OBJC_METHOD(NSFileManager, "createFileAtPath:contents:attributes:", createFile) {
    std::error_code ec;
    const auto p = hostOf(marg(0));
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary);
    if (marg(1)) out.write(reinterpret_cast<char *>(mem::base() + dataBytes(marg(1))), std::streamsize(dataLength(marg(1))));
    ret(bool(out));
}
OBJC_METHOD(NSFileManager, "contentsAtPath:", contentsAtPath) {
    std::string bytes;
    if (!files::readWholeFile(text(marg(0)), bytes)) return ret(0);
    ret(data(bytes.data(), bytes.size()));
}
OBJC_METHOD(NSFileManager, "removeItemAtPath:error:", removeItem) {
    std::error_code ec;
    const bool ok = fs::remove_all(hostOf(marg(0)), ec) > 0;
    if (!ok) setError(marg(1), "remove");
    ret(ok);
}
OBJC_METHOD(NSFileManager, "removeItemAtURL:error:", removeItemURL) {
    std::error_code ec;
    ret(fs::remove_all(hostOf(objc::send(marg(0), "path")), ec) > 0);
}
OBJC_METHOD(NSFileManager, "moveItemAtPath:toPath:error:", moveItem) {
    std::error_code ec;
    fs::rename(hostOf(marg(0)), hostOf(marg(1)), ec);
    if (ec) setError(marg(2), "move");
    ret(!ec);
}
OBJC_METHOD(NSFileManager, "copyItemAtPath:toPath:error:", copyItem) {
    std::error_code ec;
    fs::copy(hostOf(marg(0)), hostOf(marg(1)), fs::copy_options::recursive, ec);
    if (ec) setError(marg(2), "copy");
    ret(!ec);
}
OBJC_METHOD(NSFileManager, "contentsOfDirectoryAtPath:error:", contentsOfDirectory) {
    std::error_code ec;
    std::vector<id> names;
    for (const auto &e : fs::directory_iterator(hostOf(marg(0)), ec)) {
        const auto u8 = e.path().filename().u8string();
        names.push_back(string(std::string(reinterpret_cast<const char *>(u8.data()), u8.size())));
    }
    if (ec) { setError(marg(1), "list"); return ret(0); }
    ret(array(names));
}
OBJC_METHOD(NSFileManager, "subpathsAtPath:", subpaths) {
    std::error_code ec;
    std::vector<id> names;
    const auto root = hostOf(marg(0));
    for (const auto &e : fs::recursive_directory_iterator(root, ec)) {
        const auto u8 = fs::relative(e.path(), root).generic_u8string();
        names.push_back(string(std::string(reinterpret_cast<const char *>(u8.data()), u8.size())));
    }
    ret(array(names));
}
OBJC_METHOD(NSFileManager, "enumeratorAtPath:", enumeratorAtPath) {
    const auto list = objc::send(self(), "subpathsAtPath:", {marg(0)});
    ret(objc::send(objc::send(objc::getClass("NSEnumerator"), "alloc"), "initWithArray:", {list ? list : array({})}));
}
OBJC_METHOD(NSFileManager, "attributesOfItemAtPath:error:", attributesOfItem) {
    std::error_code ec;
    const auto p = hostOf(marg(0));
    if (!fs::exists(p, ec)) { setError(marg(1), "attributes"); return ret(0); }
    const bool dir = fs::is_directory(p, ec);
    ret(dictionary({
        {string("NSFileSize"), number(dir ? 0 : std::int64_t(fs::file_size(p, ec)))},
        {string("NSFileType"), string(dir ? "NSFileTypeDirectory" : "NSFileTypeRegular")},
        {string("NSFileCreationDate"), makeDate(now())},
        {string("NSFileModificationDate"), makeDate(now())},
        {string("NSFilePosixPermissions"), number(0644)},
    }));
}
OBJC_METHOD(NSFileManager, "attributesOfFileSystemForPath:error:", attributesOfFileSystem) {
    ret(dictionary({{string("NSFileSystemFreeSize"), number(8ll << 30)}, {string("NSFileSystemSize"), number(16ll << 30)}}));
}
OBJC_METHOD(NSFileManager, "setAttributes:ofItemAtPath:error:", setAttributes) { ret(1); }
OBJC_METHOD(NSFileManager, "URLsForDirectory:inDomains:", URLsForDirectory) {
    std::string path = marg(0) == 13 ? homePath() + "/Library/Caches" : marg(0) == 5 ? homePath() + "/Library" :
                       marg(0) == 14 ? homePath() + "/Library/Application Support" : homePath() + "/Documents";
    ret(array({objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(path)})}));
}
OBJC_METHOD(NSFileManager, "URLForDirectory:inDomain:appropriateForURL:create:error:", URLForDirectory) {
    std::string path = marg(0) == 13 ? homePath() + "/Library/Caches" : marg(0) == 14 ? homePath() + "/Library/Application Support" :
                       homePath() + "/Documents";
    ret(objc::send(objc::getClass("NSURL"), "fileURLWithPath:", {string(path)}));
}

// ======================================== NSUserDefaults ========================================
static id g_defaults = 0, g_defaultsStore = 0, g_registered = 0;
static std::recursive_mutex g_defaultsLock;
static std::string defaultsFile() { return files::hostPath(homePath() + "/Library/Preferences/com.supercell.scroll.plist"); }
static id defaultsStore() {
    if (!g_defaultsStore) {
        g_defaultsStore = plist::readFile(defaultsFile(), true);
        if (!g_defaultsStore || !objc::host<DictData>(g_defaultsStore)) g_defaultsStore = dictionary({}, true);
        g_registered = dictionary({}, true);
        objc::retain(g_defaultsStore);
        objc::retain(g_registered);
    }
    return g_defaultsStore;
}
static void saveDefaults() { plist::writeFile(defaultsStore(), defaultsFile()); }
static id defaultsGet(id key) {
    std::lock_guard lock(g_defaultsLock);
    if (const auto v = dictGet(defaultsStore(), key)) return v;
    return dictGet(g_registered, key);
}
static void defaultsSet(id key, id value) {
    std::lock_guard lock(g_defaultsLock);
    if (value) dictSet(defaultsStore(), key, value);
    else objc::send(defaultsStore(), "removeObjectForKey:", {key});
    saveDefaults();
}
OBJC_CLASS(NSUserDefaults, NSObject);
OBJC_CLASS_METHOD(NSUserDefaults, "standardUserDefaults", standardUserDefaults) { defaultsStore(); ret(singleton("NSUserDefaults", g_defaults)); }
OBJC_CLASS_METHOD(NSUserDefaults, "resetStandardUserDefaults", resetStandardUserDefaults) {}
OBJC_METHOD(NSUserDefaults, "initWithSuiteName:", initWithSuiteName) { defaultsStore(); ret(self()); }
OBJC_METHOD(NSUserDefaults, "objectForKey:", objectForKey) { ret(defaultsGet(marg(0))); }
OBJC_METHOD(NSUserDefaults, "stringForKey:", stringForKey) { const auto v = defaultsGet(marg(0)); ret(isString(v) ? v : v ? string(describe(v)) : 0); }
OBJC_METHOD(NSUserDefaults, "dataForKey:", dataForKey) { ret(defaultsGet(marg(0))); }
OBJC_METHOD(NSUserDefaults, "arrayForKey:", arrayForKey) { ret(defaultsGet(marg(0))); }
OBJC_METHOD(NSUserDefaults, "dictionaryForKey:", dictionaryForKey) { ret(defaultsGet(marg(0))); }
OBJC_METHOD(NSUserDefaults, "stringArrayForKey:", stringArrayForKey) { ret(defaultsGet(marg(0))); }
OBJC_METHOD(NSUserDefaults, "integerForKey:", integerForKey) { ret(std::uint64_t(intValue(defaultsGet(marg(0))))); }
OBJC_METHOD(NSUserDefaults, "boolForKey:", boolForKey) { ret(doubleValue(defaultsGet(marg(0))) != 0 || intValue(defaultsGet(marg(0))) != 0); }
OBJC_METHOD(NSUserDefaults, "floatForKey:", floatForKey) { hle::retFloat(float(doubleValue(defaultsGet(marg(0))))); }
OBJC_METHOD(NSUserDefaults, "doubleForKey:", doubleForKey) { hle::retDouble(doubleValue(defaultsGet(marg(0)))); }
OBJC_METHOD(NSUserDefaults, "setObject:forKey:", setObjectForKey) { defaultsSet(marg(1), marg(0)); }
OBJC_METHOD(NSUserDefaults, "setValue:forKey:", setValueForKey) { defaultsSet(marg(1), marg(0)); }
OBJC_METHOD(NSUserDefaults, "setInteger:forKey:", setInteger) { defaultsSet(marg(1), number(std::int64_t(marg(0)))); }
OBJC_METHOD(NSUserDefaults, "setBool:forKey:", setBool) { defaultsSet(marg(1), numberBool((marg(0) & 0xFF) != 0)); }
OBJC_METHOD(NSUserDefaults, "setFloat:forKey:", setFloat) { defaultsSet(marg(0), numberDouble(cpu::s(0), 'f')); }
OBJC_METHOD(NSUserDefaults, "setDouble:forKey:", setDouble) { defaultsSet(marg(0), numberDouble(cpu::d(0))); }
OBJC_METHOD(NSUserDefaults, "removeObjectForKey:", removeObjectForKey) { defaultsSet(marg(0), 0); }
OBJC_METHOD(NSUserDefaults, "synchronize", synchronize) { std::lock_guard lock(g_defaultsLock); saveDefaults(); ret(1); }
OBJC_METHOD(NSUserDefaults, "registerDefaults:", registerDefaults) {
    std::lock_guard lock(g_defaultsLock);
    defaultsStore();
    if (auto *d = objc::host<DictData>(marg(0))) for (auto &[k, v] : d->items) dictSet(g_registered, k, v);
}
OBJC_METHOD(NSUserDefaults, "dictionaryRepresentation", dictionaryRepresentation) { ret(objc::send(defaultsStore(), "copy")); }
// Persistent domains other than the app's own live in their own Library/Preferences/<name>.plist.
static std::string domainFile(id name) { return files::hostPath(homePath() + "/Library/Preferences/" + text(name) + ".plist"); }
OBJC_METHOD(NSUserDefaults, "persistentDomainForName:", persistentDomainForName) {
    std::lock_guard lock(g_defaultsLock);
    if (text(marg(0)) == "com.supercell.scroll") return ret(objc::send(defaultsStore(), "copy"));
    const auto d = plist::readFile(domainFile(marg(0)), false);
    ret(objc::host<DictData>(d) ? d : 0);
}
OBJC_METHOD(NSUserDefaults, "setPersistentDomain:forName:", setPersistentDomain) {
    std::lock_guard lock(g_defaultsLock);
    if (text(marg(1)) == "com.supercell.scroll") {
        objc::send(defaultsStore(), "removeAllObjects");
        if (auto *d = objc::host<DictData>(marg(0))) for (auto &[k, v] : d->items) dictSet(defaultsStore(), k, v);
        return saveDefaults();
    }
    plist::writeFile(marg(0) ? marg(0) : dictionary({}, false), domainFile(marg(1)));
}
OBJC_METHOD(NSUserDefaults, "removePersistentDomainForName:", removePersistentDomainForName) {
    std::lock_guard lock(g_defaultsLock);
    if (text(marg(0)) == "com.supercell.scroll") { objc::send(defaultsStore(), "removeAllObjects"); return saveDefaults(); }
    std::error_code ec;
    std::filesystem::remove(std::filesystem::u8path(domainFile(marg(0))), ec);
}

// ======================================== Keychain (SecItem) ========================================
// Generic passwords keyed by service + account, persisted in sandbox/keychain.plist.
static std::recursive_mutex g_keychainLock;
static id g_keychain = 0;
static std::string keychainFile() { return (fs::u8path(files::sandboxRoot()) / "keychain.plist").string(); }
static id keychain() {
    if (!g_keychain) {
        g_keychain = plist::readFile(keychainFile(), true);
        if (!g_keychain || !objc::host<DictData>(g_keychain)) g_keychain = dictionary({}, true);
        objc::retain(g_keychain);
    }
    return g_keychain;
}
static id keychainKey(id query) {
    const auto service = dictGet(query, string("kSecAttrService")), account = dictGet(query, string("kSecAttrAccount"));
    return string(describe(service) + "|" + describe(account));
}
HLE(SecItemAdd) {
    std::lock_guard lock(g_keychainLock);
    const auto key = keychainKey(arg(0));
    if (dictGet(keychain(), key)) return ret(std::uint64_t(-25299)); // errSecDuplicateItem
    auto value = dictGet(arg(0), string("kSecValueData"));
    dictSet(keychain(), key, value ? value : data("", 0));
    plist::writeFile(keychain(), keychainFile());
    hlog("keychain", "add %s", text(key).c_str());
    ret(0);
}
HLE(SecItemCopyMatching) {
    std::lock_guard lock(g_keychainLock);
    const auto key = keychainKey(arg(0));
    const auto value = dictGet(keychain(), key);
    hlog("keychain", "lookup %s%s", text(key).c_str(), value ? "" : " (not found)");
    if (!value) return ret(std::uint64_t(-25300)); // errSecItemNotFound
    if (arg(1)) {
        if (dictGet(arg(0), string("kSecReturnAttributes")))
            mem::w64(arg(1), dictionary({{string("kSecValueData"), value}, {string("kSecAttrAccount"), dictGet(arg(0), string("kSecAttrAccount"))},
                                         {string("kSecAttrService"), dictGet(arg(0), string("kSecAttrService"))}}));
        else mem::w64(arg(1), dataFromGuest(dataBytes(value), std::size_t(dataLength(value))));
    }
    ret(0);
}
HLE(SecItemUpdate) {
    std::lock_guard lock(g_keychainLock);
    const auto key = keychainKey(arg(0));
    if (!dictGet(keychain(), key)) return ret(std::uint64_t(-25300));
    if (const auto value = dictGet(arg(1), string("kSecValueData"))) dictSet(keychain(), key, value);
    plist::writeFile(keychain(), keychainFile());
    ret(0);
}
HLE(SecItemDelete) {
    std::lock_guard lock(g_keychainLock);
    const auto key = keychainKey(arg(0));
    const bool had = dictGet(keychain(), key) != 0;
    objc::send(keychain(), "removeObjectForKey:", {key});
    plist::writeFile(keychain(), keychainFile());
    ret(had ? 0 : std::uint64_t(-25300));
}

// ======================================== NSNotificationCenter ========================================
struct Observer {
    id observer = 0;
    objc::SEL selector = 0;
    std::string name;
    id object = 0, block = 0;
};
static std::mutex g_observersLock;
static std::vector<Observer> g_observers;
static id g_center = 0;
struct NotificationData : objc::HostObject { id name = 0, object = 0, userInfo = 0; };
static id makeNotification(id name, id object, id userInfo) {
    const auto n = objc::allocInstance(objc::getClass("NSNotification"));
    auto d = std::make_unique<NotificationData>();
    d->name = name;
    d->object = object;
    d->userInfo = userInfo;
    objc::retain(name);
    objc::retain(userInfo);
    objc::setHostObject(n, std::move(d));
    return n;
}
void ns::postNotification(id name, id object, id userInfo) {
    const auto notification = makeNotification(name, object, userInfo);
    const auto key = text(name);
    std::vector<Observer> targets;
    {
        std::lock_guard lock(g_observersLock);
        for (auto &o : g_observers)
            if ((o.name.empty() || o.name == key) && (!o.object || o.object == object)) targets.push_back(o);
    }
    hlog("notify", "%s (%zu observers)", key.c_str(), targets.size());
    for (auto &o : targets) {
        if (o.block) sys::callBlock(o.block, {notification});
        else objc::send(o.observer, objc::selName(o.selector), {notification});
    }
}
OBJC_CLASS(NSNotification, NSObject);
OBJC_CLASS_METHOD(NSNotification, "notificationWithName:object:", notificationWithName) { ret(makeNotification(marg(0), marg(1), 0)); }
OBJC_CLASS_METHOD(NSNotification, "notificationWithName:object:userInfo:", notificationWithNameUserInfo) {
    ret(makeNotification(marg(0), marg(1), marg(2)));
}
OBJC_METHOD(NSNotification, "name", name) { auto *d = objc::host<NotificationData>(self()); ret(d ? d->name : 0); }
OBJC_METHOD(NSNotification, "object", object) { auto *d = objc::host<NotificationData>(self()); ret(d ? d->object : 0); }
OBJC_METHOD(NSNotification, "userInfo", userInfo) { auto *d = objc::host<NotificationData>(self()); ret(d ? d->userInfo : 0); }
OBJC_CLASS(NSNotificationCenter, NSObject);
OBJC_CLASS_METHOD(NSNotificationCenter, "defaultCenter", defaultCenter) { ret(singleton("NSNotificationCenter", g_center)); }
OBJC_METHOD(NSNotificationCenter, "addObserver:selector:name:object:", addObserver) {
    std::lock_guard lock(g_observersLock);
    g_observers.push_back({marg(0), marg(1), marg(2) ? text(marg(2)) : "", marg(3), 0});
}
OBJC_METHOD(NSNotificationCenter, "addObserverForName:object:queue:usingBlock:", addObserverBlock) {
    const auto token = objc::allocInstance(objc::getClass("NSObject"));
    std::lock_guard lock(g_observersLock);
    g_observers.push_back({token, 0, marg(0) ? text(marg(0)) : "", marg(1), sys::blockCopy(marg(3))});
    ret(token);
}
OBJC_METHOD(NSNotificationCenter, "removeObserver:", removeObserver) {
    std::lock_guard lock(g_observersLock);
    g_observers.erase(std::remove_if(g_observers.begin(), g_observers.end(), [](const Observer &o) { return o.observer == marg(0); }), g_observers.end());
}
OBJC_METHOD(NSNotificationCenter, "removeObserver:name:object:", removeObserverName) {
    std::lock_guard lock(g_observersLock);
    const std::string name = marg(1) ? text(marg(1)) : "";
    g_observers.erase(std::remove_if(g_observers.begin(), g_observers.end(), [&](const Observer &o) {
        return o.observer == marg(0) && (name.empty() || o.name == name);
    }), g_observers.end());
}
OBJC_METHOD(NSNotificationCenter, "postNotificationName:object:", postName) { postNotification(marg(0), marg(1), 0); }
OBJC_METHOD(NSNotificationCenter, "postNotificationName:object:userInfo:", postNameUserInfo) { postNotification(marg(0), marg(1), marg(2)); }
OBJC_METHOD(NSNotificationCenter, "postNotification:", post) {
    auto *d = objc::host<NotificationData>(marg(0));
    if (d) postNotification(d->name, d->object, d->userInfo);
}

// ======================================== NSProcessInfo ========================================
static id g_processInfo = 0;
OBJC_CLASS(NSProcessInfo, NSObject);
OBJC_CLASS_METHOD(NSProcessInfo, "processInfo", processInfo) { ret(singleton("NSProcessInfo", g_processInfo)); }
OBJC_METHOD(NSProcessInfo, "processName", processName) { ret(string("Clash Royale")); }
OBJC_METHOD(NSProcessInfo, "processIdentifier", processIdentifier) { ret(4242); }
OBJC_METHOD(NSProcessInfo, "globallyUniqueString", globallyUniqueString) {
    static std::mt19937_64 rng(std::random_device{}());
    char b[64];
    std::snprintf(b, sizeof b, "%016llX-%016llX", (unsigned long long)rng(), (unsigned long long)rng());
    ret(string(b));
}
OBJC_METHOD(NSProcessInfo, "operatingSystemVersionString", operatingSystemVersionString) { ret(string("Version 9.3.5 (Build 13G36)")); }
// operatingSystemVersion returns a 24-byte struct through x8.
OBJC_METHOD(NSProcessInfo, "operatingSystemVersion", operatingSystemVersion) {
    const auto out = cpu::x(8);
    mem::w64(out, 9);
    mem::w64(out + 8, 3);
    mem::w64(out + 16, 5);
}
OBJC_METHOD(NSProcessInfo, "isOperatingSystemAtLeastVersion:", isAtLeast) {
    const auto v = marg(0);
    const std::uint64_t major = mem::r64(v), minor = mem::r64(v + 8);
    ret(major < 9 || (major == 9 && minor <= 3));
}
OBJC_METHOD(NSProcessInfo, "systemUptime", systemUptime) {
    hle::retDouble(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
}
OBJC_METHOD(NSProcessInfo, "environment", environment) { ret(dictionary({})); }
OBJC_METHOD(NSProcessInfo, "arguments", arguments) { ret(array({string("Clash Royale")})); }
OBJC_METHOD(NSProcessInfo, "physicalMemory", physicalMemory) { ret(1ull << 30); }
OBJC_METHOD(NSProcessInfo, "processorCount", processorCount) { ret(2); }
OBJC_METHOD(NSProcessInfo, "activeProcessorCount", activeProcessorCount) { ret(2); }
OBJC_METHOD(NSProcessInfo, "hostName", hostName) { ret(string("iPhone")); }

// ======================================== NSLocale / NSTimeZone ========================================
static id g_locale = 0, g_timeZone = 0;
OBJC_CLASS(NSLocale, NSObject);
OBJC_CLASS_METHOD(NSLocale, "currentLocale", currentLocale) { ret(singleton("NSLocale", g_locale)); }
OBJC_CLASS_METHOD(NSLocale, "systemLocale", systemLocale) { ret(singleton("NSLocale", g_locale)); }
OBJC_CLASS_METHOD(NSLocale, "autoupdatingCurrentLocale", autoupdatingCurrentLocale) { ret(singleton("NSLocale", g_locale)); }
OBJC_CLASS_METHOD(NSLocale, "preferredLanguages", preferredLanguages) { ret(array({string("en")})); }
OBJC_CLASS_METHOD(NSLocale, "localeWithLocaleIdentifier:", localeWithIdentifier) { ret(singleton("NSLocale", g_locale)); }
OBJC_CLASS_METHOD(NSLocale, "canonicalLanguageIdentifierFromString:", canonicalLanguage) { ret(marg(0)); }
OBJC_METHOD(NSLocale, "initWithLocaleIdentifier:", initWithLocaleIdentifier) { ret(self()); }
OBJC_METHOD(NSLocale, "localeIdentifier", localeIdentifier) { ret(string("en_US")); }
OBJC_METHOD(NSLocale, "objectForKey:", objectForKey) {
    const auto key = text(marg(0));
    if (key.find("CountryCode") != std::string::npos) return ret(string("US"));
    if (key.find("CurrencyCode") != std::string::npos) return ret(string("USD"));
    if (key.find("CurrencySymbol") != std::string::npos) return ret(string("$"));
    if (key.find("LanguageCode") != std::string::npos) return ret(string("en"));
    if (key.find("Identifier") != std::string::npos) return ret(string("en_US"));
    if (key.find("DecimalSeparator") != std::string::npos) return ret(string("."));
    if (key.find("GroupingSeparator") != std::string::npos) return ret(string(","));
    ret(0);
}
OBJC_METHOD(NSLocale, "displayNameForKey:value:", displayNameForKey) { ret(marg(1)); }
HLE(CFLocaleCopyCurrent) { ret(singleton("NSLocale", g_locale)); }
HLE(CFLocaleGetIdentifier) { ret(string("en_US")); }
HLE(CFLocaleGetValue) { ret(objc::send(arg(0), "objectForKey:", {arg(1)})); }
HLE(CFLocaleCopyPreferredLanguages) { ret(array({string("en")})); }
OBJC_CLASS(NSTimeZone, NSObject);
OBJC_CLASS_METHOD(NSTimeZone, "localTimeZone", localTimeZone) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_CLASS_METHOD(NSTimeZone, "systemTimeZone", systemTimeZone) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_CLASS_METHOD(NSTimeZone, "defaultTimeZone", defaultTimeZone) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_CLASS_METHOD(NSTimeZone, "timeZoneWithName:", timeZoneWithName) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_CLASS_METHOD(NSTimeZone, "timeZoneWithAbbreviation:", timeZoneWithAbbreviation) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_CLASS_METHOD(NSTimeZone, "timeZoneForSecondsFromGMT:", timeZoneForSeconds) { ret(singleton("NSTimeZone", g_timeZone)); }
OBJC_METHOD(NSTimeZone, "secondsFromGMT", secondsFromGMT) { ret(std::uint64_t(std::int64_t(platform::utcOffsetSeconds()))); }
OBJC_METHOD(NSTimeZone, "secondsFromGMTForDate:", secondsFromGMTForDate) { ret(std::uint64_t(std::int64_t(platform::utcOffsetSeconds()))); }
OBJC_METHOD(NSTimeZone, "name", name) { ret(string("Europe/Helsinki")); }
OBJC_METHOD(NSTimeZone, "abbreviation", abbreviation) { ret(string("GMT")); }

// ======================================== NSThread ========================================
static id g_mainThreadObject = 0;
OBJC_CLASS_METHOD(NSThread, "detachNewThreadSelector:toTarget:withObject:", detachNewThread) {
    const auto selector = objc::selName(marg(0));
    const auto target = marg(1), argument = marg(2);
    objc::retain(target);
    objc::retain(argument);
    sys::spawnGuestThread(selector.c_str(), [=] { objc::send(target, selector, {argument}); });
}
OBJC_CLASS_METHOD(NSThread, "isMainThread", classIsMainThread) { ret(sys::isMainThread()); }
OBJC_METHOD(NSThread, "isMainThread", isMainThread) { ret(self() == g_mainThreadObject); }
OBJC_CLASS_METHOD(NSThread, "mainThread", mainThread) { ret(singleton("NSThread", g_mainThreadObject)); }
OBJC_CLASS_METHOD(NSThread, "currentThread", currentThread) {
    if (sys::isMainThread()) return ret(singleton("NSThread", g_mainThreadObject));
    thread_local id t = 0;
    if (!t) { t = objc::allocInstance(objc::getClass("NSThread")); objc::retain(t); }
    ret(t);
}
OBJC_CLASS_METHOD(NSThread, "sleepForTimeInterval:", sleepForTimeInterval) {
    std::this_thread::sleep_for(std::chrono::duration<double>(cpu::d(0)));
}
OBJC_CLASS_METHOD(NSThread, "sleepUntilDate:", sleepUntilDate) {
    const double wait = dateValue(marg(0)) - now();
    if (wait > 0) std::this_thread::sleep_for(std::chrono::duration<double>(wait));
}
OBJC_CLASS_METHOD(NSThread, "setThreadPriority:", setThreadPriority) { ret(1); }
OBJC_CLASS_METHOD(NSThread, "callStackSymbols", callStackSymbols) { ret(array({})); }
OBJC_METHOD(NSThread, "setName:", setName) {}
OBJC_METHOD(NSThread, "name", name) { ret(string(sys::isMainThread() ? "main" : "")); }
OBJC_METHOD(NSThread, "threadDictionary", threadDictionary) {
    thread_local id d = 0;
    if (!d) { d = dictionary({}, true); objc::retain(d); }
    ret(d);
}
OBJC_METHOD(NSThread, "setThreadPriority:", setThreadPriorityInstance) {}
OBJC_METHOD(NSThread, "initWithTarget:selector:object:", initWithTarget) {
    // Stored as an associated triple in the object's words 1..3.
    const auto block = mem::calloc(24);
    mem::w64(block, marg(0));
    mem::w64(block + 8, marg(1));
    mem::w64(block + 16, marg(2));
    objc::setHostObject(self(), nullptr);
    mem::w64(self() + 8, block);
    ret(self());
}
OBJC_METHOD(NSThread, "start", start) {
    const auto block = mem::r64(self() + 8);
    if (!block) return;
    const auto target = mem::r64(block), argument = mem::r64(block + 16);
    const auto selector = objc::selName(mem::r64(block + 8));
    sys::spawnGuestThread(selector.c_str(), [=] { objc::send(target, selector, {argument}); });
}
OBJC_CLASS_SIZED(NSThread, NSObject, 16);
// performSelectorOnMainThread / inBackground (NSObject categories in Foundation).
OBJC_METHOD(NSObject, "performSelectorOnMainThread:withObject:waitUntilDone:", performOnMain) {
    const auto target = self(), argument = marg(1);
    const auto selector = objc::selName(marg(0));
    if (sys::isMainThread() && (marg(2) & 0xFF)) { objc::send(target, selector, {argument}); return; }
    objc::retain(target);
    runloop::post([=] { objc::send(target, selector, {argument}); }, (marg(2) & 0xFF) && !sys::isMainThread());
}
OBJC_METHOD(NSObject, "performSelectorInBackground:withObject:", performInBackground) {
    const auto target = self(), argument = marg(1);
    const auto selector = objc::selName(marg(0));
    objc::retain(target);
    sys::spawnGuestThread(selector.c_str(), [=] { objc::send(target, selector, {argument}); });
}
OBJC_METHOD(NSObject, "performSelector:withObject:afterDelay:", performAfterDelay) {
    const auto target = self(), argument = marg(1);
    const auto selector = objc::selName(marg(0));
    objc::retain(target);
    runloop::postAfter(cpu::d(0), [=] { objc::send(target, selector, {argument}); });
}
OBJC_CLASS_METHOD(NSObject, "cancelPreviousPerformRequestsWithTarget:", cancelPrevious) {}
OBJC_CLASS_METHOD(NSObject, "cancelPreviousPerformRequestsWithTarget:selector:object:", cancelPreviousSelector) {}

// ======================================== NSRunLoop / NSTimer ========================================
static id g_mainRunLoop = 0;
OBJC_CLASS(NSRunLoop, NSObject);
OBJC_CLASS_METHOD(NSRunLoop, "currentRunLoop", currentRunLoop) { ret(singleton("NSRunLoop", g_mainRunLoop)); }
OBJC_CLASS_METHOD(NSRunLoop, "mainRunLoop", mainRunLoop) { ret(singleton("NSRunLoop", g_mainRunLoop)); }
OBJC_METHOD(NSRunLoop, "addTimer:forMode:", addTimer) { runloop::addTimer(marg(0)); }
OBJC_METHOD(NSRunLoop, "run", run) {
    if (sys::isMainThread()) runloop::runForever();
    else for (;;) std::this_thread::sleep_for(std::chrono::seconds(1));
}
OBJC_METHOD(NSRunLoop, "runUntilDate:", runUntilDate) {
    const double wait = dateValue(marg(0)) - now();
    if (sys::isMainThread()) runloop::runFor(wait);
    else if (wait > 0) std::this_thread::sleep_for(std::chrono::duration<double>(std::min(wait, 0.05)));
}
OBJC_METHOD(NSRunLoop, "runMode:beforeDate:", runModeBeforeDate) {
    const double wait = dateValue(marg(1)) - now();
    if (sys::isMainThread()) runloop::runFor(std::min(wait, 0.016));
    else std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ret(1);
}
OBJC_METHOD(NSRunLoop, "getCFRunLoop", getCFRunLoop) { ret(self()); }
HLE(CFRunLoopGetCurrent) { ret(singleton("NSRunLoop", g_mainRunLoop)); }
HLE(CFRunLoopGetMain) { ret(singleton("NSRunLoop", g_mainRunLoop)); }
HLE(CFRunLoopRun) { runloop::runForever(); }
HLE(CFRunLoopWakeUp) {}
HLE(CFRunLoopStop) {}
HLE(CFRunLoopRunInMode) { runloop::runFor(cpu::d(0)); ret(3); }
OBJC_CLASS(NSTimer, NSObject);
static id makeTimer(double interval, id target, objc::SEL selector, id userInfo, bool repeats, id block) {
    const auto timer = objc::allocInstance(objc::getClass("NSTimer"));
    objc::retain(timer);
    objc::retain(target);
    objc::retain(userInfo);
    runloop::TimerInfo info;
    info.interval = std::max(interval, 0.0001);
    info.target = target;
    info.selector = selector;
    info.userInfo = userInfo;
    info.repeats = repeats;
    info.block = block ? sys::blockCopy(block) : 0;
    runloop::defineTimer(timer, info);
    return timer;
}
OBJC_CLASS_METHOD(NSTimer, "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", scheduled) {
    const auto t = makeTimer(cpu::d(0), marg(0), marg(1), marg(2), (marg(3) & 0xFF) != 0, 0);
    runloop::addTimer(t);
    ret(t);
}
OBJC_CLASS_METHOD(NSTimer, "timerWithTimeInterval:target:selector:userInfo:repeats:", timerWith) {
    ret(makeTimer(cpu::d(0), marg(0), marg(1), marg(2), (marg(3) & 0xFF) != 0, 0));
}
OBJC_CLASS_METHOD(NSTimer, "scheduledTimerWithTimeInterval:repeats:block:", scheduledBlock) {
    const auto t = makeTimer(cpu::d(0), 0, 0, 0, (marg(0) & 0xFF) != 0, marg(1));
    runloop::addTimer(t);
    ret(t);
}
OBJC_METHOD(NSTimer, "invalidate", invalidate) { runloop::invalidateTimer(self()); }
OBJC_METHOD(NSTimer, "isValid", isValid) { ret(runloop::timerValid(self())); }
OBJC_METHOD(NSTimer, "userInfo", userInfo) { ret(runloop::timerUserInfo(self())); }
OBJC_METHOD(NSTimer, "fire", fire) { runloop::fireTimer(self()); }
OBJC_METHOD(NSTimer, "timeInterval", timeInterval) { hle::retDouble(runloop::timerInterval(self())); }

// ======================================== NSURLRequest ========================================
// Holds the URL and method for code that builds requests (the content update downloader); nothing is sent - the
// emulator has no NSURLConnection (see patches/HttpDownload.cpp).
struct URLRequestData : objc::HostObject { id url = 0; std::string method = "GET"; };
static URLRequestData *requestOf(id o) {
    auto *d = objc::host<URLRequestData>(o);
    if (!d) { objc::setHostObject(o, std::make_unique<URLRequestData>()); d = objc::host<URLRequestData>(o); }
    return d;
}
static id retained(id object) {
    if (object) objc::retain(object);
    return object;
}
static id makeRequest(id cls, id url) {
    const auto o = objc::allocInstance(cls);
    auto d = std::make_unique<URLRequestData>();
    d->url = retained(url);
    objc::setHostObject(o, std::move(d));
    return o;
}
OBJC_CLASS(NSURLRequest, NSObject);
OBJC_CLASS(NSMutableURLRequest, NSURLRequest);
OBJC_CLASS_METHOD(NSURLRequest, "alloc", alloc) { const auto o = objc::allocInstance(self()); objc::setHostObject(o, std::make_unique<URLRequestData>()); ret(o); }
OBJC_CLASS_METHOD(NSURLRequest, "requestWithURL:", requestWithURL) { ret(makeRequest(self(), marg(0))); }
OBJC_CLASS_METHOD(NSURLRequest, "requestWithURL:cachePolicy:timeoutInterval:", requestWithURLPolicy) { ret(makeRequest(self(), marg(0))); }
OBJC_METHOD(NSURLRequest, "initWithURL:", initWithURL) { requestOf(self())->url = retained(marg(0)); ret(self()); }
OBJC_METHOD(NSURLRequest, "initWithURL:cachePolicy:timeoutInterval:", initWithURLPolicy) { requestOf(self())->url = retained(marg(0)); ret(self()); }
OBJC_METHOD(NSURLRequest, "URL", URL) { ret(requestOf(self())->url); }
OBJC_METHOD(NSURLRequest, "HTTPMethod", HTTPMethod) { ret(string(requestOf(self())->method)); }
OBJC_METHOD(NSMutableURLRequest, "setURL:", setURL) { requestOf(self())->url = retained(marg(0)); }
OBJC_METHOD(NSMutableURLRequest, "setHTTPMethod:", setHTTPMethod) { requestOf(self())->method = text(marg(0)); }
OBJC_METHOD(NSMutableURLRequest, "setHTTPBody:", setHTTPBody) {}
OBJC_METHOD(NSMutableURLRequest, "setValue:forHTTPHeaderField:", setValueForHeader) {}
OBJC_METHOD(NSMutableURLRequest, "addValue:forHTTPHeaderField:", addValueForHeader) {}
OBJC_METHOD(NSMutableURLRequest, "setTimeoutInterval:", setTimeoutInterval) {}
OBJC_METHOD(NSMutableURLRequest, "setCachePolicy:", setCachePolicy) {}

// ======================================== NSURL ========================================
struct URLData : objc::HostObject { std::string url; bool file = false; };
static id makeURL(const std::string &u, bool file) {
    const auto o = objc::allocInstance(objc::getClass("NSURL"));
    auto d = std::make_unique<URLData>();
    d->url = u;
    d->file = file;
    objc::setHostObject(o, std::move(d));
    return o;
}
static URLData *urlOf(id o) { return objc::host<URLData>(o); }
static std::string urlPath(const URLData *d) {
    if (!d) return {};
    if (d->file) return d->url;
    auto u = d->url;
    const auto scheme = u.find("://");
    if (scheme == std::string::npos) return u;
    const auto slash = u.find('/', scheme + 3);
    if (slash == std::string::npos) return "";
    auto path = u.substr(slash);
    const auto q = path.find('?');
    return q == std::string::npos ? path : path.substr(0, q);
}
OBJC_CLASS(NSURL, NSObject);
OBJC_CLASS_METHOD(NSURL, "URLWithString:", URLWithString) { ret(marg(0) ? makeURL(text(marg(0)), text(marg(0)).rfind("file://", 0) == 0) : 0); }
OBJC_CLASS_METHOD(NSURL, "URLWithString:relativeToURL:", URLWithStringRelative) { ret(marg(0) ? makeURL(text(marg(0)), false) : 0); }
OBJC_CLASS_METHOD(NSURL, "fileURLWithPath:", fileURLWithPath) { ret(makeURL(text(marg(0)), true)); }
OBJC_CLASS_METHOD(NSURL, "fileURLWithPath:isDirectory:", fileURLWithPathDir) { ret(makeURL(text(marg(0)), true)); }
OBJC_CLASS_METHOD(NSURL, "alloc", alloc) { const auto o = objc::allocInstance(self()); objc::setHostObject(o, std::make_unique<URLData>()); ret(o); }
OBJC_METHOD(NSURL, "initWithString:", initWithString) { if (!marg(0)) return ret(0); urlOf(self())->url = text(marg(0)); ret(self()); }
OBJC_METHOD(NSURL, "initFileURLWithPath:", initFileURLWithPath) { urlOf(self())->url = text(marg(0)); urlOf(self())->file = true; ret(self()); }
OBJC_METHOD(NSURL, "absoluteString", absoluteString) { auto *d = urlOf(self()); ret(string(d->file ? "file://" + d->url : d->url)); }
OBJC_METHOD(NSURL, "description", description) { auto *d = urlOf(self()); ret(string(d->file ? "file://" + d->url : d->url)); }
OBJC_METHOD(NSURL, "path", path) { ret(string(urlPath(urlOf(self())))); }
OBJC_METHOD(NSURL, "isFileURL", isFileURL) { ret(urlOf(self())->file); }
OBJC_METHOD(NSURL, "scheme", scheme) {
    auto *d = urlOf(self());
    if (d->file) return ret(string("file"));
    const auto p = d->url.find(':');
    ret(p == std::string::npos ? 0 : string(d->url.substr(0, p)));
}
OBJC_METHOD(NSURL, "host", host) {
    auto *d = urlOf(self());
    const auto s = d->url.find("://");
    if (s == std::string::npos) return ret(0);
    auto rest = d->url.substr(s + 3);
    rest = rest.substr(0, rest.find_first_of("/:?"));
    ret(string(rest));
}
OBJC_METHOD(NSURL, "query", query) {
    const auto &u = urlOf(self())->url;
    const auto q = u.find('?');
    ret(q == std::string::npos ? 0 : string(u.substr(q + 1)));
}
OBJC_METHOD(NSURL, "lastPathComponent", lastPathComponent) {
    const auto p = urlPath(urlOf(self()));
    ret(string(p.substr(p.rfind('/') == std::string::npos ? 0 : p.rfind('/') + 1)));
}
OBJC_METHOD(NSURL, "URLByAppendingPathComponent:", URLByAppendingPathComponent) {
    auto *d = urlOf(self());
    auto u = d->url;
    if (!u.empty() && u.back() != '/') u += '/';
    ret(makeURL(u + text(marg(0)), d->file));
}
OBJC_METHOD(NSURL, "setResourceValue:forKey:error:", setResourceValue) { ret(1); }
OBJC_METHOD(NSURL, "isEqual:", isEqual) { ret(urlOf(marg(0)) && urlOf(marg(0))->url == urlOf(self())->url); }
OBJC_METHOD(NSURL, "copyWithZone:", copyWithZone) { ret(self()); }
HLE(CFURLCreateFromFileSystemRepresentation) {
    ret(makeURL(std::string(reinterpret_cast<char *>(mem::base() + arg(1)), std::size_t(arg(2))), true));
}
HLE(CFURLGetFileSystemRepresentation) {
    const auto p = urlPath(urlOf(arg(0)));
    if (p.size() + 1 > arg(3)) return ret(0);
    std::memcpy(mem::base() + arg(2), p.c_str(), p.size() + 1);
    ret(1);
}
HLE(CFURLCopyFileSystemPath) { ret(string(urlPath(urlOf(arg(0))))); }
HLE(CFURLCreateStringByAddingPercentEscapes) { ret(objc::send(arg(1), "stringByAddingPercentEscapesUsingEncoding:", {4})); }

// ======================================== NSError ========================================
struct ErrorData : objc::HostObject { id domain = 0, userInfo = 0; std::int64_t code = 0; };
OBJC_CLASS(NSError, NSObject);
OBJC_CLASS_METHOD(NSError, "errorWithDomain:code:userInfo:", errorWithDomain) {
    const auto o = objc::allocInstance(self());
    auto d = std::make_unique<ErrorData>();
    d->domain = marg(0);
    d->code = std::int64_t(marg(1));
    d->userInfo = marg(2);
    objc::retain(marg(0));
    objc::retain(marg(2));
    objc::setHostObject(o, std::move(d));
    ret(o);
}
OBJC_METHOD(NSError, "initWithDomain:code:userInfo:", initWithDomain) {
    auto d = std::make_unique<ErrorData>();
    d->domain = marg(0);
    d->code = std::int64_t(marg(1));
    d->userInfo = marg(2);
    objc::setHostObject(self(), std::move(d));
    ret(self());
}
OBJC_METHOD(NSError, "domain", domain) { auto *d = objc::host<ErrorData>(self()); ret(d ? d->domain : 0); }
OBJC_METHOD(NSError, "code", code) { auto *d = objc::host<ErrorData>(self()); ret(d ? std::uint64_t(d->code) : 0); }
OBJC_METHOD(NSError, "userInfo", userInfo) { auto *d = objc::host<ErrorData>(self()); ret(d ? d->userInfo : 0); }
OBJC_METHOD(NSError, "localizedDescription", localizedDescription) {
    auto *d = objc::host<ErrorData>(self());
    ret(string(d ? describe(d->domain) + " error " + std::to_string(d->code) : "error"));
}
OBJC_METHOD(NSError, "description", description) {
    auto *d = objc::host<ErrorData>(self());
    ret(string(d ? "Error Domain=" + describe(d->domain) + " Code=" + std::to_string(d->code) : "error"));
}

// ======================================== NSUUID / CFUUID ========================================
struct UUIDData : objc::HostObject { std::uint8_t bytes[16]{}; };
static id makeUUID(const std::uint8_t *bytes) {
    const auto o = objc::allocInstance(objc::getClass("NSUUID"));
    auto d = std::make_unique<UUIDData>();
    if (bytes) std::memcpy(d->bytes, bytes, 16);
    else {
        static std::mt19937_64 rng(std::random_device{}());
        for (auto &b : d->bytes) b = std::uint8_t(rng());
        d->bytes[6] = (d->bytes[6] & 0x0F) | 0x40;
        d->bytes[8] = (d->bytes[8] & 0x3F) | 0x80;
    }
    objc::setHostObject(o, std::move(d));
    return o;
}
static std::string uuidText(id o) {
    auto *d = objc::host<UUIDData>(o);
    if (!d) return "00000000-0000-0000-0000-000000000000";
    char b[40];
    const auto *x = d->bytes;
    std::snprintf(b, sizeof b, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X", x[0], x[1], x[2], x[3], x[4], x[5],
                  x[6], x[7], x[8], x[9], x[10], x[11], x[12], x[13], x[14], x[15]);
    return b;
}
OBJC_CLASS(NSUUID, NSObject);
OBJC_CLASS_METHOD(NSUUID, "UUID", UUID) { ret(makeUUID(nullptr)); }
OBJC_CLASS_METHOD(NSUUID, "alloc", alloc) { ret(makeUUID(nullptr)); }
OBJC_METHOD(NSUUID, "init", init) { ret(self()); }
OBJC_METHOD(NSUUID, "initWithUUIDString:", initWithUUIDString) {
    const auto s = text(marg(0));
    auto *d = objc::host<UUIDData>(self());
    int k = 0;
    for (std::size_t i = 0; i + 1 < s.size() && k < 16;) {
        if (s[i] == '-') { ++i; continue; }
        d->bytes[k++] = std::uint8_t(std::stoi(s.substr(i, 2), nullptr, 16));
        i += 2;
    }
    ret(self());
}
OBJC_METHOD(NSUUID, "UUIDString", UUIDString) { ret(string(uuidText(self()))); }
OBJC_METHOD(NSUUID, "getUUIDBytes:", getUUIDBytes) { if (auto *d = objc::host<UUIDData>(self())) std::memcpy(mem::base() + marg(0), d->bytes, 16); }
HLE(CFUUIDCreate) { ret(makeUUID(nullptr)); }
HLE(CFUUIDCreateString) { ret(string(uuidText(arg(1)))); }
// CFUUIDGetUUIDBytes returns a 16-byte struct in x0/x1.
HLE(CFUUIDGetUUIDBytes) {
    auto *d = objc::host<UUIDData>(arg(0));
    std::uint64_t a = 0, b = 0;
    if (d) { std::memcpy(&a, d->bytes, 8); std::memcpy(&b, d->bytes + 8, 8); }
    cpu::setX(0, a);
    cpu::setX(1, b);
}
// CFUUIDCreateFromUUIDBytes(alloc, CFUUIDBytes by value): 16 bytes in x1/x2.
HLE(CFUUIDCreateFromUUIDBytes) {
    std::uint8_t bytes[16];
    const auto a = arg(1), b = arg(2);
    std::memcpy(bytes, &a, 8);
    std::memcpy(bytes + 8, &b, 8);
    ret(makeUUID(bytes));
}

// ======================================== Locks ========================================
struct LockData : objc::HostObject {
    std::recursive_mutex mutex;
    std::condition_variable_any cv;
};
static LockData *lockOf(id o) {
    auto *l = objc::host<LockData>(o);
    if (!l) {
        objc::setHostObject(o, std::make_unique<LockData>());
        l = objc::host<LockData>(o);
    }
    return l;
}
OBJC_CLASS(NSLock, NSObject);
OBJC_CLASS(NSRecursiveLock, NSObject);
OBJC_CLASS(NSCondition, NSObject);
OBJC_METHOD(NSLock, "lock", lock) { lockOf(self())->mutex.lock(); }
OBJC_METHOD(NSLock, "unlock", unlock) { lockOf(self())->mutex.unlock(); }
OBJC_METHOD(NSLock, "tryLock", tryLock) { ret(lockOf(self())->mutex.try_lock()); }
OBJC_METHOD(NSRecursiveLock, "lock", lock) { lockOf(self())->mutex.lock(); }
OBJC_METHOD(NSRecursiveLock, "unlock", unlock) { lockOf(self())->mutex.unlock(); }
OBJC_METHOD(NSRecursiveLock, "tryLock", tryLock) { ret(lockOf(self())->mutex.try_lock()); }
OBJC_METHOD(NSCondition, "lock", lock) { lockOf(self())->mutex.lock(); }
OBJC_METHOD(NSCondition, "unlock", unlock) { lockOf(self())->mutex.unlock(); }
OBJC_METHOD(NSCondition, "wait", wait) { auto *l = lockOf(self()); l->cv.wait(l->mutex); }
OBJC_METHOD(NSCondition, "waitUntilDate:", waitUntilDate) {
    auto *l = lockOf(self());
    const double wait = std::max(0.0, dateValue(marg(0)) - now());
    ret(l->cv.wait_for(l->mutex, std::chrono::duration<double>(wait)) == std::cv_status::no_timeout);
}
OBJC_METHOD(NSCondition, "signal", signal) { lockOf(self())->cv.notify_one(); }
OBJC_METHOD(NSCondition, "broadcast", broadcast) { lockOf(self())->cv.notify_all(); }

// ======================================== Misc ========================================
OBJC_CLASS(NSAssertionHandler, NSObject);
OBJC_CLASS_METHOD(NSAssertionHandler, "currentHandler", currentHandler) { ret(objc::allocInstance(self())); }
OBJC_METHOD(NSAssertionHandler, "handleFailureInMethod:object:file:lineNumber:description:", handleFailureInMethod) {
    hlog("assert", "assertion failed in %s (%s:%llu): %s", objc::selName(marg(0)).c_str(), text(marg(2)).c_str(),
         (unsigned long long)marg(3), text(marg(4)).c_str());
}
OBJC_METHOD(NSAssertionHandler, "handleFailureInFunction:file:lineNumber:description:", handleFailureInFunction) {
    hlog("assert", "assertion failed in %s (%s:%llu): %s", text(marg(0)).c_str(), text(marg(1)).c_str(), (unsigned long long)marg(2),
         text(marg(3)).c_str());
}
static std::uint64_t g_uncaughtHandler = 0;
HLE(NSSetUncaughtExceptionHandler) { g_uncaughtHandler = arg(0); }
HLE(NSGetUncaughtExceptionHandler) { ret(g_uncaughtHandler); }
static hle::DataRegistrar d_foundationVersion("_NSFoundationVersionNumber", [] {
    const auto p = mem::hleAlloc(8);
    mem::write<double>(p, 1241.14); // iOS 9.3
    return p;
});
// SCNetworkReachability: always reachable over Wi-Fi (kSCNetworkReachabilityFlagsReachable 2).
HLE(SCNetworkReachabilityCreateWithAddress) { ret(objc::allocInstance(objc::getClass("NSObject"))); }
HLE(SCNetworkReachabilityCreateWithName) { ret(objc::allocInstance(objc::getClass("NSObject"))); }
HLE(SCNetworkReachabilityGetFlags) { mem::w32(arg(1), 2); ret(1); }
HLE(SCNetworkReachabilitySetCallback) { ret(1); }
HLE(SCNetworkReachabilityScheduleWithRunLoop) { ret(1); }
HLE(SCNetworkReachabilityUnscheduleFromRunLoop) { ret(1); }
HLE(SCNetworkReachabilitySetDispatchQueue) { ret(1); }

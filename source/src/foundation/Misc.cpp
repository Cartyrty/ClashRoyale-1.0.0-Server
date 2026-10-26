// Smaller Foundation/UIKit/SDK classes the app touches during start-up.
#include "foundation/Foundation.h"
#include "foundation/PList.h"
#include "Log.h"
#include "Memory.h"
#include <algorithm>
#include <map>

using namespace ns;
using objc::marg;
using objc::self;
using hle::arg;
using hle::ret;

// NSCharacterSet: membership by a host predicate or explicit characters.
struct CharSetData : objc::HostObject {
    std::u16string chars;
    int builtin = 0; // 1 whitespace, 2 whitespace+newline, 3 alphanumeric, 4 decimal digits, 5 URL-allowed, 6 letters
    bool inverted = false;
    bool contains(char16_t c) const {
        bool in;
        switch (builtin) {
        case 1: in = c == u' ' || c == u'\t'; break;
        case 2: in = c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; break;
        case 3: in = (c < 128 && std::isalnum(c)) || c > 127; break;
        case 4: in = c >= u'0' && c <= u'9'; break;
        case 5: in = c < 128 && (std::isalnum(c) || std::strchr("-._~", char(c))); break;
        case 6: in = (c < 128 && std::isalpha(c)) || c > 127; break;
        default: in = chars.find(c) != std::u16string::npos;
        }
        return in != inverted;
    }
};
static id makeCharSet(int builtin, std::u16string chars = {}, bool inverted = false) {
    const auto o = objc::allocInstance(objc::getClass("NSCharacterSet"));
    auto d = std::make_unique<CharSetData>();
    d->builtin = builtin;
    d->chars = std::move(chars);
    d->inverted = inverted;
    objc::setHostObject(o, std::move(d));
    return o;
}
OBJC_CLASS(NSCharacterSet, NSObject);
OBJC_CLASS(NSMutableCharacterSet, NSCharacterSet);
OBJC_CLASS_METHOD(NSCharacterSet, "characterSetWithCharactersInString:", withCharacters) { ret(makeCharSet(0, text16(marg(0)))); }
OBJC_CLASS_METHOD(NSCharacterSet, "whitespaceCharacterSet", whitespace) { ret(makeCharSet(1)); }
OBJC_CLASS_METHOD(NSCharacterSet, "whitespaceAndNewlineCharacterSet", whitespaceNewline) { ret(makeCharSet(2)); }
OBJC_CLASS_METHOD(NSCharacterSet, "alphanumericCharacterSet", alphanumeric) { ret(makeCharSet(3)); }
OBJC_CLASS_METHOD(NSCharacterSet, "decimalDigitCharacterSet", decimalDigits) { ret(makeCharSet(4)); }
OBJC_CLASS_METHOD(NSCharacterSet, "letterCharacterSet", letters) { ret(makeCharSet(6)); }
OBJC_CLASS_METHOD(NSCharacterSet, "URLQueryAllowedCharacterSet", urlQuery) { ret(makeCharSet(5)); }
OBJC_CLASS_METHOD(NSCharacterSet, "URLPathAllowedCharacterSet", urlPath) { ret(makeCharSet(5)); }
OBJC_CLASS_METHOD(NSCharacterSet, "URLHostAllowedCharacterSet", urlHost) { ret(makeCharSet(5)); }
OBJC_METHOD(NSCharacterSet, "invertedSet", invertedSet) {
    auto *d = objc::host<CharSetData>(self());
    ret(makeCharSet(d ? d->builtin : 0, d ? d->chars : u"", d ? !d->inverted : true));
}
OBJC_METHOD(NSCharacterSet, "characterIsMember:", characterIsMember) {
    auto *d = objc::host<CharSetData>(self());
    ret(d && d->contains(char16_t(marg(0))));
}
OBJC_METHOD(NSString, "rangeOfCharacterFromSet:", rangeOfCharacterFromSet) {
    auto *d = objc::host<CharSetData>(marg(0));
    const auto t = text16(self());
    for (std::size_t i = 0; d && i < t.size(); ++i)
        if (d->contains(t[i])) { cpu::setX(0, i); cpu::setX(1, 1); return; }
    cpu::setX(0, 0x7FFFFFFFFFFFFFFFull);
    cpu::setX(1, 0);
}
OBJC_METHOD(NSString, "componentsSeparatedByCharactersInSet:", componentsSeparatedByCharactersInSet) {
    auto *d = objc::host<CharSetData>(marg(0));
    const auto t = text16(self());
    std::vector<id> parts;
    std::u16string current;
    for (auto c : t) {
        if (d && d->contains(c)) { parts.push_back(stringU16(current)); current.clear(); }
        else current.push_back(c);
    }
    parts.push_back(stringU16(current));
    ret(array(parts));
}
HLE(CFCharacterSetCreateMutable) { ret(makeCharSet(0)); }
HLE(CFCharacterSetAddCharactersInRange) {
    if (auto *d = objc::host<CharSetData>(arg(0)))
        for (std::uint64_t c = arg(1); c < arg(1) + arg(2) && c < 0x10000; ++c) d->chars.push_back(char16_t(c));
}
HLE(CFCharacterSetCreateCopy) { ret(arg(1)); }
HLE(CFCharacterSetIsLongCharacterMember) {
    auto *d = objc::host<CharSetData>(arg(0));
    ret(d && arg(1) < 0x10000 && d->contains(char16_t(arg(1))));
}

// NSDictionary keysSortedByValueUsingSelector:
OBJC_METHOD(NSDictionary, "keysSortedByValueUsingSelector:", keysSortedByValue) {
    auto *d = objc::host<DictData>(self());
    std::vector<std::pair<id, id>> items = d ? d->items : std::vector<std::pair<id, id>>{};
    const auto name = objc::selName(marg(0));
    std::stable_sort(items.begin(), items.end(), [&](auto &a, auto &b) { return std::int64_t(objc::send(a.second, name, {b.second})) < 0; });
    std::vector<id> keys;
    for (auto &[k, v] : items) keys.push_back(k);
    ret(array(keys));
}

// UIPasteboard: named pasteboards hold data by type.
struct PasteboardData : objc::HostObject { id items = 0; };
OBJC_CLASS(UIPasteboard, NSObject);
static id pasteboard(const std::string &name) {
    static std::map<std::string, id> boards;
    auto &b = boards[name];
    if (!b) {
        b = objc::allocInstance(objc::getClass("UIPasteboard"));
        objc::retain(b);
        auto d = std::make_unique<PasteboardData>();
        d->items = dictionary({}, true);
        objc::retain(d->items);
        objc::setHostObject(b, std::move(d));
    }
    return b;
}
OBJC_CLASS_METHOD(UIPasteboard, "generalPasteboard", generalPasteboard) { ret(pasteboard("general")); }
OBJC_CLASS_METHOD(UIPasteboard, "pasteboardWithName:create:", pasteboardWithName) { ret(pasteboard(text(marg(0)))); }
OBJC_CLASS_METHOD(UIPasteboard, "pasteboardWithUniqueName", pasteboardWithUniqueName) { ret(pasteboard("unique")); }
OBJC_METHOD(UIPasteboard, "setPersistent:", setPersistent) {}
OBJC_METHOD(UIPasteboard, "dataForPasteboardType:", dataForPasteboardType) { ret(dictGet(objc::host<PasteboardData>(self())->items, marg(0))); }
OBJC_METHOD(UIPasteboard, "setData:forPasteboardType:", setData) { dictSet(objc::host<PasteboardData>(self())->items, marg(1), marg(0)); }
OBJC_METHOD(UIPasteboard, "string", string_) { ret(dictGet(objc::host<PasteboardData>(self())->items, string("public.text"))); }
OBJC_METHOD(UIPasteboard, "setString:", setString) { dictSet(objc::host<PasteboardData>(self())->items, string("public.text"), marg(0)); }
OBJC_METHOD(UIPasteboard, "name", name) { ret(string("pasteboard")); }

// ASIdentifierManager (AdSupport): tracking disabled, zero identifier.
OBJC_CLASS(ASIdentifierManager, NSObject);
OBJC_CLASS_METHOD(ASIdentifierManager, "sharedManager", sharedManager) { static id m = 0; if (!m) { m = objc::allocInstance(self()); objc::retain(m); } ret(m); }
OBJC_METHOD(ASIdentifierManager, "isAdvertisingTrackingEnabled", isAdvertisingTrackingEnabled) { ret(0); }
OBJC_METHOD(ASIdentifierManager, "advertisingIdentifier", advertisingIdentifier) {
    ret(objc::send(objc::send(objc::getClass("NSUUID"), "alloc"), "initWithUUIDString:", {string("00000000-0000-0000-0000-000000000000")}));
}

// NSKeyedArchiver / NSKeyedUnarchiver for property-list objects (stored as XML plists).
OBJC_CLASS(NSKeyedArchiver, NSObject);
OBJC_CLASS(NSKeyedUnarchiver, NSObject);
OBJC_CLASS_METHOD(NSKeyedArchiver, "archivedDataWithRootObject:", archivedData) {
    const auto xml = plist::serialize(marg(0));
    ret(data(xml.data(), xml.size()));
}
OBJC_CLASS_METHOD(NSKeyedArchiver, "archiveRootObject:toFile:", archiveToFile) {
    const auto xml = plist::serialize(marg(0));
    ret(objc::send(data(xml.data(), xml.size()), "writeToFile:atomically:", {marg(1), 1}));
}
OBJC_CLASS_METHOD(NSKeyedUnarchiver, "unarchiveObjectWithData:", unarchiveData) {
    if (!marg(0)) return ret(0);
    ret(plist::parse(std::string(reinterpret_cast<char *>(mem::base() + dataBytes(marg(0))), std::size_t(dataLength(marg(0))))));
}
OBJC_CLASS_METHOD(NSKeyedUnarchiver, "unarchiveObjectWithFile:", unarchiveFile) {
    const auto d = objc::send(objc::getClass("NSData"), "dataWithContentsOfFile:", {marg(0)});
    if (!d) return ret(0);
    ret(plist::parse(std::string(reinterpret_cast<char *>(mem::base() + dataBytes(d)), std::size_t(dataLength(d)))));
}
OBJC_CLASS(NSPropertyListSerialization, NSObject);
OBJC_CLASS_METHOD(NSPropertyListSerialization, "dataWithPropertyList:format:options:error:", dataWithPropertyList) {
    const auto xml = plist::serialize(marg(0));
    ret(data(xml.data(), xml.size()));
}
OBJC_CLASS_METHOD(NSPropertyListSerialization, "propertyListWithData:options:format:error:", propertyListWithData) {
    if (!marg(0)) return ret(0);
    ret(plist::parse(std::string(reinterpret_cast<char *>(mem::base() + dataBytes(marg(0))), std::size_t(dataLength(marg(0)))), marg(1) != 0));
}
// -writeToFile:atomically: / +...WithContentsOfFile: for collections.
OBJC_METHOD(NSDictionary, "writeToFile:atomically:", dictWriteToFile) {
    ret(objc::send(objc::send(objc::getClass("NSPropertyListSerialization"), "dataWithPropertyList:format:options:error:", {self(), 100, 0, 0}),
                   "writeToFile:atomically:", {marg(0), 1}));
}
OBJC_METHOD(NSArray, "writeToFile:atomically:", arrayWriteToFile) {
    ret(objc::send(objc::send(objc::getClass("NSPropertyListSerialization"), "dataWithPropertyList:format:options:error:", {self(), 100, 0, 0}),
                   "writeToFile:atomically:", {marg(0), 1}));
}
static id plistFromFile(id path, bool mutableContainers) {
    const auto d = objc::send(objc::getClass("NSData"), "dataWithContentsOfFile:", {path});
    if (!d) return 0;
    return plist::parse(std::string(reinterpret_cast<char *>(mem::base() + dataBytes(d)), std::size_t(dataLength(d))), mutableContainers);
}
OBJC_CLASS_METHOD(NSDictionary, "dictionaryWithContentsOfFile:", dictionaryWithContentsOfFile) {
    const auto o = plistFromFile(marg(0), objc::isSubclass(self(), objc::getClass("NSMutableDictionary")));
    ret(objc::host<DictData>(o) ? o : 0);
}
OBJC_CLASS_METHOD(NSArray, "arrayWithContentsOfFile:", arrayWithContentsOfFile) {
    const auto o = plistFromFile(marg(0), objc::isSubclass(self(), objc::getClass("NSMutableArray")));
    ret(objc::host<ArrayData>(o) ? o : 0);
}
OBJC_METHOD(NSDictionary, "initWithContentsOfFile:", dictInitWithContentsOfFile) {
    const auto o = plistFromFile(marg(0), false);
    if (auto *src = objc::host<DictData>(o)) for (auto &[k, v] : src->items) dictSet(self(), k, v);
    ret(o ? self() : 0);
}

// NSURLCache: accepted and ignored.
OBJC_CLASS(NSURLCache, NSObject);
OBJC_METHOD(NSURLCache, "initWithMemoryCapacity:diskCapacity:diskPath:", initWithMemoryCapacity) { ret(self()); }
OBJC_CLASS_METHOD(NSURLCache, "setSharedURLCache:", setSharedURLCache) {}
OBJC_CLASS_METHOD(NSURLCache, "sharedURLCache", sharedURLCache) { ret(objc::allocInstance(self())); }
OBJC_METHOD(NSURLCache, "removeAllCachedResponses", removeAllCachedResponses) {}

// Audio session (AudioToolbox): accepted; OpenAL output comes separately.
HLE(AudioSessionInitialize) { ret(0); }
HLE(AudioSessionSetActive) { ret(0); }
HLE(AudioSessionSetProperty) { ret(0); }
HLE(AudioSessionGetProperty) {
    // kAudioSessionProperty_OtherAudioIsPlaying ('othr') and others: report 0.
    if (arg(2)) mem::w32(arg(2), 0);
    ret(0);
}

// NSException: name/reason/userInfo (raised exceptions end in objc_exception_throw).
struct ExceptionData : objc::HostObject { id name = 0, reason = 0, userInfo = 0; };
OBJC_CLASS(NSException, NSObject);
OBJC_CLASS_METHOD(NSException, "exceptionWithName:reason:userInfo:", exceptionWithName) {
    const auto e = objc::allocInstance(self());
    auto d = std::make_unique<ExceptionData>();
    d->name = marg(0); d->reason = marg(1); d->userInfo = marg(2);
    objc::setHostObject(e, std::move(d));
    ret(e);
}
OBJC_METHOD(NSException, "initWithName:reason:userInfo:", initWithName) {
    auto d = std::make_unique<ExceptionData>();
    d->name = marg(0); d->reason = marg(1); d->userInfo = marg(2);
    objc::setHostObject(self(), std::move(d));
    ret(self());
}
OBJC_METHOD(NSException, "name", name) { auto *d = objc::host<ExceptionData>(self()); ret(d ? d->name : 0); }
OBJC_METHOD(NSException, "reason", reason) { auto *d = objc::host<ExceptionData>(self()); ret(d ? d->reason : 0); }
OBJC_METHOD(NSException, "userInfo", userInfo) { auto *d = objc::host<ExceptionData>(self()); ret(d ? d->userInfo : 0); }
OBJC_METHOD(NSException, "raise", raise) { cpu::call(hle::resolveImport("_objc_exception_throw"), {self()}); }
OBJC_METHOD(NSSet, "initWithObjects:", initWithObjects) {
    std::vector<id> items;
    if (marg(0)) {
        items.push_back(marg(0));
        for (std::uint64_t slot = cpu::sp(); mem::r64(slot); slot += 8) items.push_back(mem::r64(slot));
    }
    for (auto i : items) objc::send(self(), "addObject:", {i});
    if (auto *d = objc::host<SetData>(self()); d && !d->mutableSet) {
        for (auto i : items)
            if (std::none_of(d->items.begin(), d->items.end(), [&](id e) { return objectsEqual(e, i); })) { objc::retain(i); d->items.push_back(i); }
    }
    ret(self());
}

//	Altirra - Atari 800/800XL/5200 emulator
//	System library - registry provider for non-Windows
//
//	Uses VDRegistryProviderMemory (already implemented) as the in-memory
//	store. Config is persisted to ~/.config/altirra/settings.json via
//	vdjson. On startup we load the JSON; on exit (or on each write) we
//	flush it back.
//
//	For Phase 3 (get libraries to link), the in-memory provider is
//	sufficient — the JSON persistence layer can be added in Phase 5/6.

#include <stdafx.h>
#include <vd2/system/registry.h>
#include <vd2/system/registrymemory.h>
#include <vd2/system/VDString.h>

// -------------------------------------------------------------------------
// VDRegistryKey — thin wrapper around IVDRegistryProvider
// -------------------------------------------------------------------------

// These are defined in registry.cpp on Windows; provide the cross-platform
// implementations here.

static IVDRegistryProvider *g_pRegistryProvider;

VDString VDRegistryAppKey::s_appbase;

IVDRegistryProvider *VDGetDefaultRegistryProvider() {
	static VDRegistryProviderMemory sDefaultProvider;
	return &sDefaultProvider;
}

IVDRegistryProvider *VDGetRegistryProvider() {
	if (!g_pRegistryProvider)
		g_pRegistryProvider = VDGetDefaultRegistryProvider();
	return g_pRegistryProvider;
}

void VDSetRegistryProvider(IVDRegistryProvider *provider) {
	g_pRegistryProvider = provider;
}

// -------------------------------------------------------------------------
// VDRegistryKey
// -------------------------------------------------------------------------

VDRegistryKey::VDRegistryKey(const char *pszKey, bool global, bool write)
	: mKey(nullptr)
{
	IVDRegistryProvider *p = VDGetRegistryProvider();
	void *base = global ? p->GetMachineKey() : p->GetUserKey();
	mKey = p->CreateKey(base, pszKey, write);
}

VDRegistryKey::VDRegistryKey(VDRegistryKey& baseKey, const char *name, bool write)
	: mKey(nullptr)
{
	IVDRegistryProvider *p = VDGetRegistryProvider();
	mKey = p->CreateKey(baseKey.mKey, name, write);
}

VDRegistryKey::VDRegistryKey(VDRegistryKey&& src)
	: mKey(src.mKey)
{
	src.mKey = nullptr;
}

VDRegistryKey::~VDRegistryKey() {
	if (mKey)
		VDGetRegistryProvider()->CloseKey(mKey);
}

VDRegistryKey& VDRegistryKey::operator=(VDRegistryKey&& src) {
	if (mKey) VDGetRegistryProvider()->CloseKey(mKey);
	mKey = src.mKey;
	src.mKey = nullptr;
	return *this;
}

bool VDRegistryKey::setBool(const char *name, bool v) const {
	return VDGetRegistryProvider()->SetBool(mKey, name, v);
}

bool VDRegistryKey::setInt(const char *name, int v) const {
	return VDGetRegistryProvider()->SetInt(mKey, name, v);
}

bool VDRegistryKey::setString(const char *name, const char *s) const {
	return VDGetRegistryProvider()->SetString(mKey, name, s);
}

bool VDRegistryKey::setString(const char *name, const wchar_t *s) const {
	return VDGetRegistryProvider()->SetString(mKey, name, s);
}

bool VDRegistryKey::setBinary(const char *name, const char *data, int len) const {
	return VDGetRegistryProvider()->SetBinary(mKey, name, data, len);
}

VDRegistryKey::Type VDRegistryKey::getValueType(const char *name) const {
	IVDRegistryProvider::Type t = VDGetRegistryProvider()->GetType(mKey, name);
	switch(t) {
		case IVDRegistryProvider::kTypeInt:    return kTypeInt;
		case IVDRegistryProvider::kTypeString: return kTypeString;
		case IVDRegistryProvider::kTypeBinary: return kTypeBinary;
		default:                               return kTypeUnknown;
	}
}

bool VDRegistryKey::getBool(const char *name, bool def) const {
	bool v = def;
	VDGetRegistryProvider()->GetBool(mKey, name, v);
	return v;
}

int VDRegistryKey::getInt(const char *name, int def) const {
	int v = def;
	VDGetRegistryProvider()->GetInt(mKey, name, v);
	return v;
}

int VDRegistryKey::getEnumInt(const char *name, int maxVal, int def) const {
	int v = getInt(name, def);
	return (v >= 0 && v < maxVal) ? v : def;
}

bool VDRegistryKey::getString(const char *name, VDStringA& s) const {
	return VDGetRegistryProvider()->GetString(mKey, name, s);
}

bool VDRegistryKey::getString(const char *name, VDStringW& s) const {
	return VDGetRegistryProvider()->GetString(mKey, name, s);
}

int VDRegistryKey::getBinaryLength(const char *name) const {
	return VDGetRegistryProvider()->GetBinaryLength(mKey, name);
}

bool VDRegistryKey::getBinary(const char *name, char *buf, int maxlen) const {
	return VDGetRegistryProvider()->GetBinary(mKey, name, buf, maxlen);
}

bool VDRegistryKey::removeValue(const char *name) {
	return VDGetRegistryProvider()->RemoveValue(mKey, name);
}

bool VDRegistryKey::removeKey(const char *name) {
	return VDGetRegistryProvider()->RemoveKey(mKey, name);
}

bool VDRegistryKey::removeKeyRecursive(const char *name) {
	return VDGetRegistryProvider()->RemoveKeyRecursive(mKey, name);
}

// -------------------------------------------------------------------------
// VDRegistryValueIterator / VDRegistryKeyIterator
// -------------------------------------------------------------------------

VDRegistryValueIterator::VDRegistryValueIterator(const VDRegistryKey& key)
	: mEnumerator(VDGetRegistryProvider()->EnumValuesBegin(key.getRawHandle()))
{
}

VDRegistryValueIterator::~VDRegistryValueIterator() {
	VDGetRegistryProvider()->EnumValuesClose(mEnumerator);
}

const char *VDRegistryValueIterator::Next() {
	return VDGetRegistryProvider()->EnumValuesNext(mEnumerator);
}

VDRegistryKeyIterator::VDRegistryKeyIterator(const VDRegistryKey& key)
	: mEnumerator(VDGetRegistryProvider()->EnumKeysBegin(key.getRawHandle()))
{
}

VDRegistryKeyIterator::~VDRegistryKeyIterator() {
	VDGetRegistryProvider()->EnumKeysClose(mEnumerator);
}

const char *VDRegistryKeyIterator::Next() {
	return VDGetRegistryProvider()->EnumKeysNext(mEnumerator);
}

// -------------------------------------------------------------------------
// VDRegistryAppKey
// -------------------------------------------------------------------------

VDRegistryAppKey::VDRegistryAppKey()
	: VDRegistryKey(s_appbase.c_str(), false, true)
{
}

VDRegistryAppKey::VDRegistryAppKey(const char *pszKey, bool write, bool global)
	: VDRegistryKey((s_appbase + "\\" + pszKey).c_str(), global, write)
{
}

void VDRegistryAppKey::setDefaultKey(const char *pszAppName) {
	s_appbase = pszAppName;
}

const char *VDRegistryAppKey::getDefaultKey() {
	return s_appbase.c_str();
}

// -------------------------------------------------------------------------
// VDRegistryCopy
// -------------------------------------------------------------------------

void VDRegistryCopy(IVDRegistryProvider& dstProvider, const char *dstPath,
                    IVDRegistryProvider& srcProvider, const char *srcPath)
{
	// Stub — complex operation not needed for basic functionality
}

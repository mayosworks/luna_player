//=============================================================================
// よみがな取得
//=============================================================================

#define INITGUID

#include <windows.h>
#include <cguid.h>
#include "msime.h"
#include "Phonetic.h"

Phonetic::Phonetic()
	: m_fe_lang(NULL)
{
}

Phonetic::~Phonetic()
{
	Finalize();
}

bool Phonetic::Initialize()
{
	HRESULT hr = S_OK;

	CLSID clsid = GUID_NULL;
	hr = CLSIDFromString(L"MSIME.Japan", &clsid);
	if (FAILED(hr)) {
		//OutputDebugString("MSIME.Japan get failed.");
		return false;
	}

	void** fe_lang = reinterpret_cast<void**>(&m_fe_lang);
	hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, IID_IFELanguage, fe_lang);
	if (FAILED(hr)) {
		//OutputDebugString("IFELanguage create failed.");
		return false;
	}

	return true;
}

void Phonetic::Finalize()
{
	if (m_fe_lang) {
		m_fe_lang->Release();
		m_fe_lang = NULL;
	}
}

bool Phonetic::Convert(const wchar_t* text, wchar_t* phonetic, UINT max_len)
{
	if (!m_fe_lang) {
		return false;
	}

	HRESULT hr = S_OK;
	hr = m_fe_lang->Open();
	if (FAILED(hr)) {
		//OutputDebugString("IFELanguage Open() failed.");
		return false;
	}

	UINT len = static_cast<UINT>(lstrlen(text));
	if (len == 0) {
		phonetic[0] = L'\0';
		return true;
	}

#if 1
	BSTR src = SysAllocStringLen(text, len);
	BSTR dest = NULL;

	hr = m_fe_lang->GetPhonetic(src, 1, len, &dest);
	m_fe_lang->Close();

	SysFreeString(src);

	if (FAILED(hr)) {
		SysFreeString(dest);
		//OutputDebugString("IFELanguage GetPhonetic() failed.");
		return false;
	}

	lstrcpyn(phonetic, dest, max_len);
	phonetic[max_len - 1] = L'\0';

	SysFreeString(dest);

#else
	DWORD fcInfo = FELANG_CLMN_FIXD;
	MORRSLT* result = NULL;
	hr = m_fe_lang->GetJMorphResult(FELANG_REQ_REV, FELANG_CMODE_HIRAGANAOUT, len,
		text, &fcInfo, &result);
	if (FAILED(hr)) {
		CoTaskMemFree(result);
		m_fe_lang->Close();
		return false;
	}

	if (result) {
		int copy = std::min<int>(result->cchOutput + 1, max_len - 1);
		lstrcpyn(ruby, result->pwchOutput, copy);
		phonetic[copy] = L'\0';

		CoTaskMemFree(result);
	}
#endif

	return true;
}

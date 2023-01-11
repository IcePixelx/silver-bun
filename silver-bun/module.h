#pragma once

#ifndef USE_PRECOMPILED_HEADERS
#include "memaddr.h"
#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <algorithm>
#elif
#pragma message("ADD PRECOMPILED HEADERS TO SILVER-BUN.")
#endif // !USE_PRECOMPILED_HEADERS

class CModule
{
public:
	struct ModuleSections_t
	{
		ModuleSections_t(void) = default;
		ModuleSections_t(const std::string& svSectionName, uintptr_t pSectionBase, size_t nSectionSize) :
			m_svSectionName(svSectionName), m_pSectionBase(pSectionBase), m_nSectionSize(nSectionSize) {}

		bool IsSectionValid(void) const
		{
			return m_nSectionSize != 0;
		}

		std::string m_svSectionName;         // Name of section.
		uintptr_t   m_pSectionBase;          // Start address of section.
		size_t      m_nSectionSize;          // Size of section.
	};

	CModule(void) = default;
	CModule(const std::string& moduleName);
	CModule(const uintptr_t nModuleBase);
	CMemory FindPatternSIMD(const std::string& svPattern, const ModuleSections_t* moduleSection = nullptr) const;
	CMemory FindString(const std::string& string, const ptrdiff_t occurrence = 1, bool nullTerminator = false) const;
	CMemory FindStringReadOnly(const std::string& svString, bool nullTerminator) const;

	CMemory          GetVirtualMethodTable(const std::string& svTableName, const uint32_t nRefIndex = 0);
	CMemory          GetExportedFunction(const std::string& svFunctionName) const;
	CMemory          GetImportedFunction(const std::string& svModuleName, const std::string& svFunctionName, const bool bGetFunctionReference = false) const;
	uintptr_t        GetModuleBase(void) const;
	DWORD            GetModuleSize(void) const;
	std::string      GetModuleName(void) const;
	uintptr_t        GetRVA(const uintptr_t nAddress) const;
	ModuleSections_t GetSectionByName(const std::string& svSectionName) const;
	std::vector<CModule::ModuleSections_t>& GetSections();
	
	void UnlinkFromPEB(void);
	CMemory FindFreeDataPage(const size_t nSize);

	IMAGE_NT_HEADERS64*      m_pNTHeaders;
	IMAGE_DOS_HEADER*        m_pDOSHeader;

	ModuleSections_t         m_ExecutableCode;
	ModuleSections_t         m_ExceptionTable;
	ModuleSections_t         m_RunTimeData;
	ModuleSections_t         m_ReadOnlyData;

private:
	CMemory FindPatternSIMD(const uint8_t* szPattern, const char* szMask, const ModuleSections_t* moduleSection = nullptr, const uint32_t nOccurrence = 0) const;

	std::string                   m_svModuleName;
	uintptr_t                     m_pModuleBase;
	DWORD                         m_nModuleSize;
	std::vector<ModuleSections_t> m_vModuleSections;
};
//===========================================================================//
//
// Purpose: Implementation of the CModule class.
//
//===========================================================================//
#include "module.h"
#include "utils.h"
#include "tebpeb64.h"

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : *svModuleName
//-----------------------------------------------------------------------------
CModule::CModule(const std::string& svModuleName) : m_svModuleName(svModuleName)
{
	m_pModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(svModuleName.c_str()));
	m_pDOSHeader  = reinterpret_cast<IMAGE_DOS_HEADER*>(m_pModuleBase);
	m_pNTHeaders  = reinterpret_cast<IMAGE_NT_HEADERS64*>(m_pModuleBase + m_pDOSHeader->e_lfanew);
	m_nModuleSize = static_cast<size_t>(m_pNTHeaders->OptionalHeader.SizeOfImage);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(m_pNTHeaders); // Get first image section.

	for (WORD i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; i++) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vModuleSections.push_back(ModuleSections_t(std::string(reinterpret_cast<const char*>(hCurrentSection.Name)),
			static_cast<uintptr_t>(m_pModuleBase + hCurrentSection.VirtualAddress), hCurrentSection.SizeOfRawData)); // Push back a struct with the section data.
	}

	m_ExecutableCode = GetSectionByName(".text");
	m_ExceptionTable = GetSectionByName(".pdata");
	m_RunTimeData    = GetSectionByName(".data");
	m_ReadOnlyData   = GetSectionByName(".rdata");
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : nModuleBase
//-----------------------------------------------------------------------------
CModule::CModule(uintptr_t nModuleBase) : m_pModuleBase(nModuleBase)
{
	m_pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_pModuleBase);
	m_pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(m_pModuleBase + m_pDOSHeader->e_lfanew);
	m_nModuleSize = static_cast<size_t>(m_pNTHeaders->OptionalHeader.SizeOfImage);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(m_pNTHeaders); // Get first image section.

	for (WORD i = 0; i < m_pNTHeaders->FileHeader.NumberOfSections; i++) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vModuleSections.push_back(ModuleSections_t(std::string(reinterpret_cast<const char*>(hCurrentSection.Name)),
			static_cast<uintptr_t>(m_pModuleBase + hCurrentSection.VirtualAddress), hCurrentSection.SizeOfRawData)); // Push back a struct with the section data.
	}

	m_ExecutableCode = GetSectionByName(".text");
	m_ExceptionTable = GetSectionByName(".pdata");
	m_RunTimeData = GetSectionByName(".data");
	m_ReadOnlyData = GetSectionByName(".rdata");
}

//-----------------------------------------------------------------------------
// Purpose: find array of bytes in process memory using SIMD instructions
// Input  : *szPattern - 
//          *szMask - 
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPatternSIMD(const uint8_t* szPattern, const char* szMask, const ModuleSections_t* moduleSection, const uint32_t nOccurrence) const
{
	if (!m_ExecutableCode.IsSectionValid())
		return CMemory();

	const bool bSectionValid = moduleSection ? moduleSection->IsSectionValid() : false;

	const uintptr_t nBase = bSectionValid ? moduleSection->m_pSectionBase : m_ExecutableCode.m_pSectionBase;
	const uintptr_t nSize = bSectionValid ? moduleSection->m_nSectionSize : m_ExecutableCode.m_nSectionSize;

	const size_t nMaskLen = strlen(szMask);
	const uint8_t* pData = reinterpret_cast<uint8_t*>(nBase);
	const uint8_t* pEnd = pData + nSize - nMaskLen;

	int nOccurrenceCount = 0;
	int nMasks[64]; // 64*16 = enough masks for 1024 bytes.
	const int iNumMasks = static_cast<int>(ceil(static_cast<float>(nMaskLen) / 16.f));

	memset(nMasks, '\0', iNumMasks * sizeof(int));
	for (intptr_t i = 0; i < iNumMasks; ++i)
	{
		for (intptr_t j = strnlen(szMask + i * 16, 16) - 1; j >= 0; --j)
		{
			if (szMask[i * 16 + j] == 'x')
			{
				_bittestandset(reinterpret_cast<LONG*>(&nMasks[i]), static_cast<LONG>(j));
			}
		}
	}
	const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(szPattern));
	__m128i xmm2, xmm3, msks;
	for (; pData != pEnd; _mm_prefetch(reinterpret_cast<const char*>(++pData + 64), _MM_HINT_NTA))
	{
		if (szPattern[0] == pData[0])
		{
			xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
			msks = _mm_cmpeq_epi8(xmm1, xmm2);
			if ((_mm_movemask_epi8(msks) & nMasks[0]) == nMasks[0])
			{
				for (uintptr_t i = 1; i < static_cast<uintptr_t>(iNumMasks); ++i)
				{
					xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pData + i * 16)));
					xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((szPattern + i * 16)));
					msks = _mm_cmpeq_epi8(xmm2, xmm3);
					if ((_mm_movemask_epi8(msks) & nMasks[i]) == nMasks[i])
					{
						if ((i + 1) == iNumMasks)
						{
							if (nOccurrenceCount == nOccurrence)
							{
								return static_cast<CMemory>(const_cast<uint8_t*>(pData));
							}
							nOccurrenceCount++;
						}
					}
					else
					{
						goto cont;
					}
				}
				if (nOccurrenceCount == nOccurrence)
				{
					return static_cast<CMemory>((&*(const_cast<uint8_t*>(pData))));
				}
				nOccurrenceCount++;
			}
		}cont:;
	}
	return CMemory();
}


//-----------------------------------------------------------------------------
// Purpose: find a string pattern in process memory using SIMD instructions
// Input  : &svPattern
//			&moduleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPatternSIMD(const std::string& svPattern, const ModuleSections_t* moduleSection) const
{
	const std::pair patternInfo = Utils::PatternToMaskedBytes(svPattern);
	return FindPatternSIMD(patternInfo.first.data(), patternInfo.second.c_str(), moduleSection);
}

//-----------------------------------------------------------------------------
// Purpose: find address of input string constant in read only memory
// Input  : *svString - 
//          bNullTerminator - 
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindStringReadOnly(const std::string& svString, bool bNullTerminator) const
{
	if (!m_ReadOnlyData.IsSectionValid())
		return CMemory();

	const std::vector<int> vBytes = Utils::StringToBytes(svString, bNullTerminator); // Convert our string to a byte array.
	const std::pair bytesInfo = std::make_pair(vBytes.size(), vBytes.data()); // Get the size and data of our bytes.

	const uint8_t* pBase = reinterpret_cast<uint8_t*>(m_ReadOnlyData.m_pSectionBase); // Get start of .rdata section.

	for (size_t i = 0ull; i < m_ReadOnlyData.m_nSectionSize - bytesInfo.first; i++)
	{
		bool bFound = true;

		// If either the current byte equals to the byte in our pattern or our current byte in the pattern is a wildcard
		// our if clause will be false.
		for (size_t j = 0ull; j < bytesInfo.first; j++)
		{
			if (pBase[i + j] != bytesInfo.second[j] && bytesInfo.second[j] != -1)
			{
				bFound = false;
				break;
			}
		}

		if (bFound)
		{
			return CMemory(&pBase[i]);
		}
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: find address of reference to string constant in executable memory
// Input  : *svString - 
//          bNullTerminator - 
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindString(const std::string& svString, const ptrdiff_t nOccurrence, bool bNullTerminator) const
{
	if (!m_ExecutableCode.IsSectionValid())
		return CMemory();

	const CMemory stringAddress = FindStringReadOnly(svString, bNullTerminator); // Get Address for the string in the .rdata section.

	if (!stringAddress)
		return CMemory();

	uint8_t* pLatestOccurrence = nullptr;
	uint8_t* pTextStart = reinterpret_cast<uint8_t*>(m_ExecutableCode.m_pSectionBase); // Get the start of the .text section.
	ptrdiff_t dOccurrencesFound = 0;
	CMemory resultAddress;

	for (size_t i = 0ull; i < m_ExecutableCode.m_nSectionSize - 0x5; i++)
	{
		byte byte = pTextStart[i];
		if (byte == 0x8D)
		{
			const CMemory skipOpCode = CMemory(reinterpret_cast<uintptr_t>(&pTextStart[i])).OffsetSelf(0x2); // Skip next 2 opcodes, those being the instruction and the register.
			const int32_t relativeAddress = skipOpCode.GetValue<int32_t>();                                  // Get 4-byte long string relative Address
			const uintptr_t nextInstruction = skipOpCode.Offset(0x4).GetPtr();                               // Get location of next instruction.
			const CMemory potentialLocation = CMemory(nextInstruction + relativeAddress);                    // Get potential string location.

			if (potentialLocation == stringAddress)
			{
				dOccurrencesFound++;
				if (nOccurrence == dOccurrencesFound)
				{
					return CMemory(&pTextStart[i]);
				}

				pLatestOccurrence = &pTextStart[i]; // Stash latest occurrence.
			}
		}
	}

	return CMemory(pLatestOccurrence);
}

//-----------------------------------------------------------------------------
// Purpose: get address of a virtual method table by rtti type descriptor name
// Input  : *svTableName - 
//			nRefIndex - 
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetVirtualMethodTable(const std::string& svTableName, const uint32_t nRefIndex)
{
	if (!m_ReadOnlyData.IsSectionValid()) // Process decided to rename the readonlydata section if this fails.
		return CMemory();

	ModuleSections_t moduleSection = { ".data", m_RunTimeData.m_pSectionBase, m_RunTimeData.m_nSectionSize };

	const auto tableNameInfo = Utils::StringToMaskedBytes(svTableName, false);
	CMemory rttiTypeDescriptor = FindPatternSIMD(tableNameInfo.first.data(), tableNameInfo.second.c_str(), &moduleSection).OffsetSelf(-0x10);
	if (!rttiTypeDescriptor)
		return CMemory();

	uintptr_t scanStart = m_ReadOnlyData.m_pSectionBase; // Get the start address of our scan.

	const uintptr_t scanEnd = (m_ReadOnlyData.m_pSectionBase + m_ReadOnlyData.m_nSectionSize) - 0x4; // Calculate the end of our scan.
	const uintptr_t rttiTDRva = rttiTypeDescriptor.GetPtr() - m_pModuleBase; // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.
	while (scanStart < scanEnd)
	{
		moduleSection = { ".rdata", scanStart, m_ReadOnlyData.m_nSectionSize };
		CMemory reference = FindPatternSIMD(reinterpret_cast<rsig_t>(&rttiTDRva), "xxxx", &moduleSection, nRefIndex);
		if (!reference)
			break;
		
		CMemory referenceOffset = reference.Offset(-0xC);
		if (referenceOffset.GetValue<int32_t>() != 1) // Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		{
			scanStart = reference.Offset(0x4).GetPtr(); // Set location to current reference + 0x4 so we avoid pushing it back again into the vector.
			continue;
		}

		moduleSection = { ".rdata", m_ReadOnlyData.m_pSectionBase, m_ReadOnlyData.m_nSectionSize };
		return FindPatternSIMD(reinterpret_cast<rsig_t>(&referenceOffset), "xxxxxxxx", &moduleSection).OffsetSelf(0x8);
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: get address of exported function in this module
// Input  : *svFunctionName - 
//          bNullTerminator - 
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetExportedFunction(const std::string& svFunctionName) const
{
	if (!m_pDOSHeader || m_pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE) // Is dosHeader valid?
		return CMemory();

	if (!m_pNTHeaders || m_pNTHeaders->Signature != IMAGE_NT_SIGNATURE) // Is ntHeader valid?
		return CMemory();

	// Get the location of IMAGE_EXPORT_DIRECTORY for this module by adding the IMAGE_DIRECTORY_ENTRY_EXPORT relative virtual address onto our module base address.
	const IMAGE_EXPORT_DIRECTORY* pImageExportDirectory = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(m_pModuleBase + m_pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
	if (!pImageExportDirectory)
		return CMemory();

	// Are there any exported functions?
	if (!pImageExportDirectory->NumberOfFunctions)
		return CMemory();

	// Get the location of the functions via adding the relative virtual address from the struct into our module base address.
	const DWORD* pAddressOfFunctions = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfFunctions);
	if (!pAddressOfFunctions)
		return CMemory();

	// Get the names of the functions via adding the relative virtual address from the struct into our module base Address.
	const DWORD* pAddressOfName = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfNames);
	if (!pAddressOfName)
		return CMemory();

	// Get the ordinals of the functions via adding the relative virtual Address from the struct into our module base address.
	DWORD* pAddressOfOrdinals = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfNameOrdinals);
	if (!pAddressOfOrdinals)
		return CMemory();

	for (DWORD i = 0; i < pImageExportDirectory->NumberOfFunctions; i++) // Iterate through all the functions.
	{
		// Get virtual relative Address of the function name. Then add module base Address to get the actual location.
		std::string ExportFunctionName = reinterpret_cast<char*>(reinterpret_cast<DWORD*>(m_pModuleBase + pAddressOfName[i]));

		if (ExportFunctionName.compare(svFunctionName) == 0) // Is this our wanted exported function?
		{
			// Get the function ordinal. Then grab the relative virtual address of our wanted function. Then add module base address so we get the actual location.
			return CMemory(m_pModuleBase + pAddressOfFunctions[reinterpret_cast<WORD*>(pAddressOfOrdinals)[i]]); // Return as CMemory class.
		}
	}
	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: find 'free' page in r/w/x sections
// Input  : nSize -
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindFreeDataPage(const size_t nSize)
{
	auto checkDataSection = [](const void* address, const std::size_t size)
	{
		MEMORY_BASIC_INFORMATION membInfo = { 0 };

		VirtualQuery(address, &membInfo, sizeof(membInfo));

		if (membInfo.AllocationBase && membInfo.BaseAddress && membInfo.State == MEM_COMMIT && !(membInfo.Protect & PAGE_GUARD) && membInfo.Protect != PAGE_NOACCESS)
		{
			if ((membInfo.Protect & (PAGE_EXECUTE_READWRITE | PAGE_READWRITE)) && membInfo.RegionSize >= size)
			{
				return ((membInfo.Protect & (PAGE_EXECUTE_READWRITE | PAGE_READWRITE)) && membInfo.RegionSize >= size) ? true : false;
			}
		}
		return false;
	};

	// This is very unstable, this doesn't check for the actual 'page' sizes.
	// Also can be optimized to search per 'section'.
	const uintptr_t endOfModule = m_pModuleBase + m_pNTHeaders->OptionalHeader.SizeOfImage - sizeof(uintptr_t);
	for (uintptr_t currAddr = endOfModule; m_pModuleBase < currAddr; currAddr -= sizeof(uintptr_t))
	{
		if (*reinterpret_cast<uintptr_t*>(currAddr) == 0 && checkDataSection(reinterpret_cast<void*>(currAddr), nSize))
		{
			bool bIsGoodPage = true;
			uint32_t nPageCount = 0;

			for (; nPageCount < nSize && bIsGoodPage; nPageCount += sizeof(uintptr_t))
			{
				const uintptr_t pageData = *reinterpret_cast<std::uintptr_t*>(currAddr + nPageCount);
				if (pageData != 0)
					bIsGoodPage = false;
			}

			if (bIsGoodPage && nPageCount >= nSize)
				return currAddr;
		}
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: unlink module from peb
//-----------------------------------------------------------------------------
void CModule::UnlinkFromPEB() // Disclaimer: This does not bypass GetMappedFileName. That function calls NtQueryVirtualMemory which does a syscall to ntoskrnl for getting info on a section.
{
#define UNLINK_FROM_PEB(entry)    \
	(entry).Flink->Blink = (entry).Blink; \
	(entry).Blink->Flink = (entry).Flink;

	const PEB64* processEnvBlock = reinterpret_cast<PEB64*>(__readgsqword(0x60)); // https://en.wikipedia.org/wiki/Win32_Thread_Information_Block
	const LIST_ENTRY* inLoadOrderList = &processEnvBlock->Ldr->InLoadOrderModuleList;

	for (LIST_ENTRY* entry = inLoadOrderList->Flink; entry != inLoadOrderList; entry = entry->Flink)
	{
		const PLDR_DATA_TABLE_ENTRY pldrEntry = reinterpret_cast<PLDR_DATA_TABLE_ENTRY>(entry->Flink);
		const std::uintptr_t baseAddr = reinterpret_cast<std::uintptr_t>(pldrEntry->DllBase);

		if (baseAddr != m_pModuleBase)
			continue;

		UNLINK_FROM_PEB(pldrEntry->InInitializationOrderLinks);
		UNLINK_FROM_PEB(pldrEntry->InMemoryOrderLinks);
		UNLINK_FROM_PEB(pldrEntry->InLoadOrderLinks);
		break;
	}
#undef UNLINK_FROM_PEB
}

//-----------------------------------------------------------------------------
// Purpose: get the module section by name (example: '.rdata', '.text')
// Input  : *svModuleName - 
// Output : ModuleSections_t
//-----------------------------------------------------------------------------
CModule::ModuleSections_t CModule::GetSectionByName(const std::string& svSectionName) const
{
	for (const ModuleSections_t& section : m_vModuleSections)
	{
		if (section.m_svSectionName == svSectionName)
			return section;
	}

	return ModuleSections_t();
}

//-----------------------------------------------------------------------------
// Purpose: get all module sections.
//-----------------------------------------------------------------------------
std::vector<CModule::ModuleSections_t>& CModule::GetSections()
{
	return m_vModuleSections;
}

//-----------------------------------------------------------------------------
// Purpose: returns the module base
//-----------------------------------------------------------------------------
uintptr_t CModule::GetModuleBase(void) const
{
	return m_pModuleBase;
}

//-----------------------------------------------------------------------------
// Purpose: returns the module size
//-----------------------------------------------------------------------------
DWORD CModule::GetModuleSize(void) const
{
	return m_nModuleSize;
}

//-----------------------------------------------------------------------------
// Purpose: returns the module name
//-----------------------------------------------------------------------------
std::string CModule::GetModuleName(void) const
{
	return m_svModuleName;
}

//-----------------------------------------------------------------------------
// Purpose: returns the RVA of given address
//-----------------------------------------------------------------------------
uintptr_t CModule::GetRVA(const uintptr_t nAddress) const
{
	return (nAddress - GetModuleBase());
}

#pragma once

#ifndef USE_OUTSIDE_HEADERS
#include <iostream>
#include <vector>
#include <windows.h>
#include <intrin.h>
#include "tebpeb64.h"
#endif // !USE_OUTSIDE_HEADERS

typedef const unsigned char* rsig_t;

class CMemory
{
public:
	enum class Direction : int
	{
		DOWN = 0,
		UP,
	};

	CMemory() : ptr(0) {}
	CMemory(const uintptr_t ptr) : ptr(ptr) {}
	CMemory(const void* const ptr) : ptr(reinterpret_cast<uintptr_t>(ptr)) {}

	inline operator uintptr_t(void) const
	{
		return ptr;
	}

	inline operator void* (void) const
	{
		return reinterpret_cast<void*>(ptr);
	}

	inline operator bool(void) const
	{
		return ptr != 0;
	}

	inline bool operator!= (const CMemory& addr) const
	{
		return ptr != addr.ptr;
	}

	inline bool operator== (const CMemory& addr) const
	{
		return ptr == addr.ptr;
	}

	inline bool operator== (const uintptr_t addr) const
	{
		return ptr == addr;
	}

	inline uintptr_t GetPtr(void) const
	{
		return ptr;
	}

	template<class T> inline T GetValue(void) const
	{
		return *reinterpret_cast<T*>(ptr);
	}

	template<class T> inline T GetVirtualFunctionIndex(void) const
	{
		return *reinterpret_cast<T*>(ptr) / sizeof(uintptr_t);
	}

	template<typename T> inline T RCast(void) const
	{
		return reinterpret_cast<T>(ptr);
	}

	inline CMemory Offset(const ptrdiff_t offset) const
	{
		return CMemory(ptr + offset);
	}

	inline CMemory OffsetSelf(const ptrdiff_t offset)
	{
		ptr += offset;
		return *this;
	}

	inline CMemory Deref(int deref = 1) const
	{
		uintptr_t reference = ptr;

		while (deref--)
		{
			if (reference)
				reference = *reinterpret_cast<uintptr_t*>(reference);
		}

		return CMemory(reference);
	}

	inline CMemory DerefSelf(int deref = 1)
	{
		while (deref--)
		{
			if (ptr)
				ptr = *reinterpret_cast<uintptr_t*>(ptr);
		}

		return *this;
	}

	inline CMemory WalkVTable(const ptrdiff_t vfuncIdx)
	{
		const uintptr_t reference = ptr + (sizeof(uintptr_t) * vfuncIdx);
		return CMemory(reference);
	}

	inline CMemory WalkVTableSelf(const ptrdiff_t vfuncIdx)
	{
		ptr += (sizeof(uintptr_t) * vfuncIdx);
		return *this;
	}

	bool CheckOpCodes(const std::vector<uint8_t>& vOpcodeArray) const
	{
		uintptr_t ref = ptr;

		// Loop forward in the ptr class member.
		for (auto [byteAtCurrentAddress, i] = std::tuple<uint8_t, size_t>{ uint8_t(), (size_t)0 }; i < vOpcodeArray.size(); i++, ref++)
		{
			byteAtCurrentAddress = *reinterpret_cast<uint8_t*>(ref);

			// If byte at ptr doesn't equal in the byte array return false.
			if (byteAtCurrentAddress != vOpcodeArray[i])
				return false;
		}

		return true;
	}

	void Patch(const std::vector<uint8_t>& vOpcodeArray) const
	{
		DWORD oldProt = NULL;

		SIZE_T dwSize = vOpcodeArray.size();
		VirtualProtect(reinterpret_cast<void*>(ptr), dwSize, PAGE_EXECUTE_READWRITE, &oldProt); // Patch page to be able to read and write to it.

		for (size_t i = 0; i < vOpcodeArray.size(); i++)
		{
			*reinterpret_cast<uint8_t*>(ptr + i) = vOpcodeArray[i]; // Write opcodes to Address.
		}

		dwSize = vOpcodeArray.size();
		VirtualProtect(reinterpret_cast<void*>(ptr), dwSize, oldProt, &oldProt); // Restore protection.
	}

	void PatchString(const char* szString) const
	{
		DWORD oldProt = NULL;
		SIZE_T dwSize = strlen(szString);

		VirtualProtect(reinterpret_cast<void*>(ptr), dwSize, PAGE_EXECUTE_READWRITE, &oldProt); // Patch page to be able to read and write to it.

		for (SIZE_T i = 0; i < dwSize; i++)
		{
			*reinterpret_cast<uint8_t*>(ptr + i) = szString[i]; // Write string to Address.
		}

		VirtualProtect(reinterpret_cast<void*>(ptr), dwSize, oldProt, &oldProt); // Restore protection.
	}

	CMemory FindPattern(const char* szPattern, const Direction searchDirect = Direction::DOWN, const int opCodesToScan = 512, const ptrdiff_t nOccurences = 1) const
	{
		uint8_t* pScanBytes = reinterpret_cast<uint8_t*>(ptr); // Get the base of the module.

		const std::vector<int> PatternBytes = PatternToBytes(szPattern); // Convert our pattern to a byte array.
		const std::pair<size_t, const int*> bytesInfo = std::make_pair<size_t, const int*>(PatternBytes.size(), PatternBytes.data()); // Get the size and data of our bytes.
		ptrdiff_t occurrences = 0;

		for (long i = 01; i < opCodesToScan + bytesInfo.first; i++)
		{
			bool bFound = true;
			int nMemOffset = searchDirect == Direction::DOWN ? i : -i;

			for (DWORD j = 0ul; j < bytesInfo.first; j++)
			{
				// If either the current byte equals to the byte in our pattern or our current byte in the pattern is a wildcard
				// our if clause will be false.
				uint8_t currentByte = *(pScanBytes + nMemOffset + j);
				_mm_prefetch(reinterpret_cast<const CHAR*>(static_cast<int64_t>(currentByte + nMemOffset + 64)), _MM_HINT_T0); // precache some data in L1.
				if (currentByte != bytesInfo.second[j] && bytesInfo.second[j] != -1)
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
			{
				occurrences++;
				if (nOccurences == occurrences)
				{
					return CMemory(&*(pScanBytes + nMemOffset));
				}
			}
		}

		return CMemory();
	}

	CMemory FindPatternSelf(const char* szPattern, const Direction searchDirect = Direction::DOWN, const int opCodesToScan = 512, const ptrdiff_t occurrence = 1)
	{
		uint8_t* pScanBytes = reinterpret_cast<uint8_t*>(ptr); // Get the base of the module.

		const std::vector<int> PatternBytes = PatternToBytes(szPattern); // Convert our pattern to a byte array.
		const std::pair<size_t, const int*> bytesInfo = std::make_pair<size_t, const int*>(PatternBytes.size(), PatternBytes.data()); // Get the size and data of our bytes.
		ptrdiff_t occurrences = 0;

		for (long i = 01; i < opCodesToScan + bytesInfo.first; i++)
		{
			bool bFound = true;
			int nMemOffset = searchDirect == Direction::DOWN ? i : -i;

			for (DWORD j = 0ul; j < bytesInfo.first; j++)
			{
				// If either the current byte equals to the byte in our pattern or our current byte in the pattern is a wildcard
				// our if clause will be false.
				uint8_t currentByte = *(pScanBytes + nMemOffset + j);
				_mm_prefetch(reinterpret_cast<const CHAR*>(static_cast<int64_t>(currentByte + nMemOffset + 64)), _MM_HINT_T0); // precache some data in L1.
				if (currentByte != bytesInfo.second[j] && bytesInfo.second[j] != -1)
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
			{
				occurrences++;
				if (occurrence == occurrences)
				{
					ptr = uintptr_t(&*(pScanBytes + nMemOffset));
					return *this;
				}
			}
		}

		ptr = uintptr_t();
		return *this;
	}

	std::vector<CMemory> FindAllCallReferences(const uintptr_t sectionBase, const size_t sectionSize) const
	{
		std::vector<CMemory> referencesInfo;

		uint8_t* const pTextStart = reinterpret_cast<uint8_t*>(sectionBase);
		for (size_t i = 0ull; i < sectionSize - 0x5; i++, _mm_prefetch(reinterpret_cast<const char*>(pTextStart + 64), _MM_HINT_NTA))
		{
			if (pTextStart[i] == 0xE8) // Call instruction = 0xE8
			{
				CMemory memAddr = CMemory(&pTextStart[i]);
				if (!memAddr.Offset(0x1).CheckOpCodes({ 0x00, 0x00, 0x00, 0x00 })) // Check if its not a dynamic resolved call.
				{
					if (memAddr.FollowNearCall() == *this)
						referencesInfo.push_back(std::move(memAddr));
				}
			}
		}

		return referencesInfo;
	}

	inline CMemory FollowNearCall(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5) const
	{
		return ResolveRelativeAddress(opcodeOffset, nextInstructionOffset);
	}

	inline CMemory FollowNearCallSelf(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5)
	{
		return ResolveRelativeAddressSelf(opcodeOffset, nextInstructionOffset);
	}

	inline CMemory ResolveRelativeAddress(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4) const
	{
		const uintptr_t skipRegister = ptr + registerOffset;
		const int32_t relativeAddress = *reinterpret_cast<int32_t*>(skipRegister);

		const uintptr_t nextInstruction = ptr + nextInstructionOffset;
		return CMemory(nextInstruction + relativeAddress);
	}

	inline CMemory ResolveRelativeAddressSelf(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4)
	{
		const uintptr_t skipRegister = ptr + registerOffset;
		const int32_t relativeAddress = *reinterpret_cast<int32_t*>(skipRegister);

		const uintptr_t nextInstruction = ptr + nextInstructionOffset;
		ptr = nextInstruction + relativeAddress;
		return *this;
	}

	static void HookVirtualMethod(const uintptr_t virtualTable, const void* const pHookMethod, const ptrdiff_t methodIndex, void** const ppOriginalMethod)
	{
		DWORD oldProt = 0u;

		// Calculate delta to next virtual method.
		const uintptr_t virtualMethod = virtualTable + (methodIndex * sizeof(ptrdiff_t));

		const uintptr_t originalFunction = *reinterpret_cast<uintptr_t*>(virtualMethod);

		// Set page for current virtual method to execute n read n write so we can actually hook it.
		VirtualProtect(reinterpret_cast<void*>(virtualMethod), sizeof(virtualMethod), PAGE_EXECUTE_READWRITE, &oldProt);

		// Patch virtual method to our hook.
		*reinterpret_cast<uintptr_t*>(virtualMethod) = reinterpret_cast<uintptr_t>(pHookMethod);

		VirtualProtect(reinterpret_cast<void*>(virtualMethod), sizeof(virtualMethod), oldProt, &oldProt);

		*ppOriginalMethod = reinterpret_cast<void*>(originalFunction);
	}

	static void HookImportedFunction(const uintptr_t pImportedMethod, const void* const pHookMethod, void** const ppOriginalMethod)
	{
		DWORD oldProt = 0u;

		const uintptr_t originalFunction = *reinterpret_cast<uintptr_t*>(pImportedMethod);

		// Set page for current iat entry to execute n read n write so we can actually hook it.
		VirtualProtect(reinterpret_cast<void*>(pImportedMethod), sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProt);

		// Patch method to our hook.
		*reinterpret_cast<uintptr_t*>(pImportedMethod) = reinterpret_cast<uintptr_t>(pHookMethod);

		VirtualProtect(reinterpret_cast<void*>(pImportedMethod), sizeof(void*), oldProt, &oldProt);

		*ppOriginalMethod = reinterpret_cast<void*>(originalFunction);
	}

	static std::vector<int> PatternToBytes(const char* const szInput)
	{
		char* const pszPatternStart = const_cast<char*>(szInput);
		const char* const pszPatternEnd = pszPatternStart + strlen(szInput);
		std::vector<int> vBytes;

		for (char* pszCurrentByte = pszPatternStart; pszCurrentByte < pszPatternEnd; ++pszCurrentByte)
		{
			if (*pszCurrentByte == '?')
			{
				++pszCurrentByte;
				if (*pszCurrentByte == '?')
					++pszCurrentByte; // Skip double wildcard.

				vBytes.push_back(-1); // Push the byte back as invalid.
			}
			else
			{
				vBytes.push_back(strtoul(pszCurrentByte, &pszCurrentByte, 16));
			}
		}

		return vBytes;
	};

	static std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(const char* const szInput)
	{
		const char* pszPatternStart = const_cast<char*>(szInput);
		const char* pszPatternEnd = pszPatternStart + strlen(szInput);

		std::vector<uint8_t> vBytes;
		std::string svMask;

		for (const char* pszCurrentByte = pszPatternStart; pszCurrentByte < pszPatternEnd; ++pszCurrentByte)
		{
			if (*pszCurrentByte == '?')
			{
				++pszCurrentByte;
				if (*pszCurrentByte == '?')
				{
					++pszCurrentByte; // Skip double wildcard.
				}

				// Technically skipped but we need to push any value here.
				vBytes.push_back(0);
				svMask += '?';
			}
			else
			{
				vBytes.push_back(uint8_t(strtoul(pszCurrentByte, const_cast<char**>(&pszCurrentByte), 16)));
				svMask += 'x';
			}
		}
		return make_pair(vBytes, svMask);
	};

	static std::vector<int> StringToBytes(const char* const szInput, bool bNullTerminator)
	{
		const char* pszStringStart = const_cast<char*>(szInput);
		const char* pszStringEnd = pszStringStart + strlen(szInput);
		std::vector<int> vBytes;

		for (const char* pszCurrentByte = pszStringStart; pszCurrentByte < pszStringEnd; ++pszCurrentByte)
		{
			// Dereference character and push back the byte.
			vBytes.push_back(*pszCurrentByte);
		}

		if (bNullTerminator)
		{
			vBytes.push_back('\0');
		}
		return vBytes;
	};

	static std::pair<std::vector<uint8_t>, std::string> StringToMaskedBytes(const char* szInput, bool bNullTerminator)
	{
		const char* pszStringStart = const_cast<char*>(szInput);
		const char* pszStringEnd = pszStringStart + strlen(szInput);
		std::vector<uint8_t> vBytes;
		std::string svMask;

		for (const char* pszCurrentByte = pszStringStart; pszCurrentByte < pszStringEnd; ++pszCurrentByte)
		{
			// Dereference character and push back the byte.
			vBytes.push_back(*pszCurrentByte);
			svMask += 'x';
		}

		if (bNullTerminator)
		{
			vBytes.push_back(0x0);
			svMask += 'x';
		}
		return make_pair(vBytes, svMask);
	};

private:
	uintptr_t ptr;
};

class CModule
{
public:
	struct ModuleSections_t
	{
		ModuleSections_t(void) = default;
		ModuleSections_t(const char* sectionName, uintptr_t pSectionBase, size_t nSectionSize) : m_SectionName(sectionName), m_pSectionBase(pSectionBase), m_nSectionSize(nSectionSize) {}
		inline bool IsSectionValid(void) const { return m_nSectionSize != 0; }

		const char* m_SectionName;
		uintptr_t   m_pSectionBase;
		size_t      m_nSectionSize;
	};

	CModule(void) = default;
	CModule(const char* const szModuleName) : m_ModuleName(szModuleName)
	{
		m_pModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(szModuleName));

		Init();
		LoadSections(".text", ".pdata", ".data", ".rdata");
	}

	CModule(const char* const szModuleName, const uintptr_t nModuleBase) : m_ModuleName(szModuleName), m_pModuleBase(nModuleBase)
	{
		Init();
		LoadSections(".text", ".pdata", ".data", ".rdata");
	}

	~CModule()
	{
		if (m_ModuleSections)
		{
			delete[] m_ModuleSections;
			m_ModuleSections = nullptr;
		}
	}

	void Init()
	{
		m_pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_pModuleBase);
		m_pNTHeaders = reinterpret_cast<decltype(m_pNTHeaders)>(m_pModuleBase + m_pDOSHeader->e_lfanew);
		m_nModuleSize = m_pNTHeaders->OptionalHeader.SizeOfImage;
		
		// Populate sections now from NT Header.
		const IMAGE_SECTION_HEADER* const pFirstSection = IMAGE_FIRST_SECTION(m_pNTHeaders);
		m_nNumSections = m_pNTHeaders->FileHeader.NumberOfSections;

		if (m_nNumSections != 0)
		{
			m_ModuleSections = new ModuleSections_t[m_nNumSections];
			for (WORD i = 0; i < m_nNumSections; i++)
			{
				const IMAGE_SECTION_HEADER* const pCurrentSection = &pFirstSection[i];

				ModuleSections_t* const pSection = &m_ModuleSections[i];
				pSection->m_SectionName = reinterpret_cast<const char* const>(pCurrentSection->Name);
				pSection->m_pSectionBase = static_cast<uintptr_t>(m_pModuleBase + pCurrentSection->VirtualAddress);
				pSection->m_nSectionSize = pCurrentSection->Misc.VirtualSize;
			}
		}
	}

	void LoadSections(const char* const szExecuteable, const char* const szExeception, const char* const szRunTime, const char* const szReadOnly)
	{
		m_ExecutableCode = GetSectionByName(szExecuteable);
		m_ExceptionTable = GetSectionByName(szExeception);
		m_RunTimeData	 = GetSectionByName(szRunTime);
		m_ReadOnlyData	 = GetSectionByName(szReadOnly);
	}

	CMemory FindPatternSIMD(const uint8_t* const pPattern, const char* const szMask, const ModuleSections_t* const moduleSection = nullptr, const size_t nOccurrence = 0u) const
	{
		if (!m_ExecutableCode->IsSectionValid())
			return CMemory();

		const bool bSectionValid = moduleSection ? moduleSection->IsSectionValid() : false;

		const uintptr_t nBase = bSectionValid ? moduleSection->m_pSectionBase : m_ExecutableCode->m_pSectionBase;
		const uintptr_t nSize = bSectionValid ? moduleSection->m_nSectionSize : m_ExecutableCode->m_nSectionSize;

		const size_t nMaskLen = strlen(szMask);
		uint8_t* pData = reinterpret_cast<uint8_t*>(nBase);
		const uint8_t* const pEnd = pData + nSize - nMaskLen;

		size_t nOccurrenceCount = 0u;
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
		const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern));
		__m128i xmm2, xmm3, msks;
		for (; pData != pEnd; _mm_prefetch(reinterpret_cast<const char*>(++pData + 64), _MM_HINT_NTA))
		{
			if (pPattern[0] == pData[0])
			{
				xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
				msks = _mm_cmpeq_epi8(xmm1, xmm2);
				if ((_mm_movemask_epi8(msks) & nMasks[0]) == nMasks[0])
				{
					for (uintptr_t i = 1; i < static_cast<uintptr_t>(iNumMasks); ++i)
					{
						xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pData + i * 16)));
						xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pPattern + i * 16)));
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
						return static_cast<CMemory>((&*(pData)));
					}
					nOccurrenceCount++;
				}
			}cont:;
		}
		return CMemory();
	}

	CMemory FindPatternSIMD(const char* szPattern, const ModuleSections_t* moduleSection = nullptr, const size_t nOccurrence = 0u) const
	{
		const std::pair<std::vector<uint8_t>, std::string> patternInfo = CMemory::PatternToMaskedBytes(szPattern);
		return FindPatternSIMD(patternInfo.first.data(), patternInfo.second.c_str(), moduleSection, nOccurrence);
	}

	CMemory FindString(const char* const szString, const ptrdiff_t nOccurrence = 1, bool bNullTerminator = false) const
	{
		if (!m_ExecutableCode->IsSectionValid())
			return CMemory();

		// Get address for the string in the .rdata section.
		const CMemory stringAddress = FindStringReadOnly(szString, bNullTerminator);
		if (!stringAddress)
			return CMemory();

		uint8_t* pLatestOccurrence = nullptr;
		uint8_t* const pTextStart = reinterpret_cast<uint8_t*>(m_ExecutableCode->m_pSectionBase);
		ptrdiff_t dOccurrencesFound = 0;

		for (size_t i = 0ull; i < m_ExecutableCode->m_nSectionSize - 0x5; i++)
		{
			byte byte = pTextStart[i];
			if (byte == 0x8D) // 0x8D = LEA
			{
				const CMemory skipOpCode = CMemory(reinterpret_cast<uintptr_t>(&pTextStart[i])).OffsetSelf(0x2); // Skip next 2 opcodes, those being the instruction and the register.
				const int32_t relativeAddress = skipOpCode.GetValue<int32_t>();
				const uintptr_t nextInstruction = skipOpCode.Offset(0x4).GetPtr();
				const CMemory potentialLocation = CMemory(nextInstruction + relativeAddress);

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

	CMemory FindStringReadOnly(const char* const szString, const bool bNullTerminator) const
	{
		if (!m_ReadOnlyData->IsSectionValid())
			return CMemory();

		const std::vector<int> vBytes = CMemory::StringToBytes(szString, bNullTerminator);
		const std::pair<size_t, const int*> bytesInfo = std::make_pair<size_t, const int*>(vBytes.size(), vBytes.data());

		const uint8_t* const pBase = reinterpret_cast<uint8_t*>(m_ReadOnlyData->m_pSectionBase);

		for (size_t i = 0ull; i < m_ReadOnlyData->m_nSectionSize - bytesInfo.first; i++)
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

	CMemory FindFreeDataPage(const size_t nSize) const
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

	CMemory GetVirtualMethodTable(const char* const szTableName, const size_t nRefIndex = 0)
	{
		if (!m_ReadOnlyData->IsSectionValid())
			return CMemory();

		ModuleSections_t moduleSection(".data", m_RunTimeData->m_pSectionBase, m_RunTimeData->m_nSectionSize);

		const auto tableNameInfo = CMemory::StringToMaskedBytes(szTableName, false);
		CMemory rttiTypeDescriptor = FindPatternSIMD(tableNameInfo.first.data(), tableNameInfo.second.c_str(), &moduleSection).OffsetSelf(-0x10);
		if (!rttiTypeDescriptor)
			return CMemory();

		uintptr_t scanStart = m_ReadOnlyData->m_pSectionBase; // Get the start address of our scan.

		const uintptr_t scanEnd = (m_ReadOnlyData->m_pSectionBase + m_ReadOnlyData->m_nSectionSize) - 0x4; // Calculate the end of our scan.
		const uintptr_t rttiTDRva = rttiTypeDescriptor.GetPtr() - m_pModuleBase; // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.
		while (scanStart < scanEnd)
		{
			moduleSection = { ".rdata", scanStart, m_ReadOnlyData->m_nSectionSize };
			CMemory reference = FindPatternSIMD(reinterpret_cast<rsig_t>(&rttiTDRva), "xxxx", &moduleSection, nRefIndex);
			if (!reference)
				break;

			CMemory referenceOffset = reference.Offset(-0xC);
			if (referenceOffset.GetValue<int32_t>() != 1) // Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
			{
				scanStart = reference.Offset(0x4).GetPtr(); // Set location to current reference + 0x4 so we avoid pushing it back again into the vector.
				continue;
			}

			moduleSection = { ".rdata", m_ReadOnlyData->m_pSectionBase, m_ReadOnlyData->m_nSectionSize };
			return FindPatternSIMD(reinterpret_cast<rsig_t>(&referenceOffset), "xxxxxxxx", &moduleSection).OffsetSelf(0x8);
		}

		return CMemory();
	}

	CMemory GetImportedFunction(const char* const szModuleName, const char* const szFunctionName, const bool bGetFunctionReference) const
	{
		// Get the location of IMAGE_IMPORT_DESCRIPTOR for this module by adding the IMAGE_DIRECTORY_ENTRY_IMPORT relative virtual address onto our module base address.
		IMAGE_IMPORT_DESCRIPTOR* const pImageImportDescriptors = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR* const>(m_pModuleBase + m_pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
		if (!pImageImportDescriptors)
			return CMemory();

		for (IMAGE_IMPORT_DESCRIPTOR* pIID = pImageImportDescriptors; pIID->Name != 0; pIID++)
		{
			// Get virtual relative Address of the imported module name. Then add module base Address to get the actual location.
			const char* const szImportedModuleName = reinterpret_cast<char*>(reinterpret_cast<DWORD*>(m_pModuleBase + pIID->Name));

			if (_stricmp(szImportedModuleName, szModuleName) == 0)
			{
				// Original first thunk to get function name.
				IMAGE_THUNK_DATA* pOgFirstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(m_pModuleBase + pIID->OriginalFirstThunk);

				// To get actual function address.
				IMAGE_THUNK_DATA* pFirstThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(m_pModuleBase + pIID->FirstThunk);
				for (; pOgFirstThunk->u1.AddressOfData; ++pOgFirstThunk, ++pFirstThunk)
				{
					// Get image import by name.
					const IMAGE_IMPORT_BY_NAME* pImageImportByName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(m_pModuleBase + pOgFirstThunk->u1.AddressOfData);

					if (_stricmp(pImageImportByName->Name, szFunctionName) == 0)
					{
						// Grab function address from firstThunk.
					#if _WIN64
						uintptr_t* pFunctionAddress = &pFirstThunk->u1.Function;
					#else
						uintptr_t* pFunctionAddress = reinterpret_cast<uintptr_t*>(&pFirstThunk->u1.Function);
					#endif // #if _WIN64

						// Reference or address?
						return bGetFunctionReference ? CMemory(pFunctionAddress) : CMemory(*pFunctionAddress);
					}
				}

			}
		}
		return CMemory();
	}

	CMemory GetExportedFunction(const char* szFunctionName) const
	{
		const IMAGE_EXPORT_DIRECTORY* pImageExportDirectory = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(m_pModuleBase + m_pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
		if (!pImageExportDirectory)
			return CMemory();

		if (!pImageExportDirectory->NumberOfFunctions)
			return CMemory();

		const DWORD* const pAddressOfFunctions = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfFunctions);
		if (!pAddressOfFunctions)
			return CMemory();

		const DWORD* const pAddressOfName = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfNames);
		if (!pAddressOfName)
			return CMemory();

		DWORD* const pAddressOfOrdinals = reinterpret_cast<DWORD*>(m_pModuleBase + pImageExportDirectory->AddressOfNameOrdinals);
		if (!pAddressOfOrdinals)
			return CMemory();

		for (DWORD i = 0; i < pImageExportDirectory->NumberOfNames; i++)
		{
			const char* const szExportFunctionName = reinterpret_cast<char*>(reinterpret_cast<DWORD*>(m_pModuleBase + pAddressOfName[i]));
			if (_stricmp(szExportFunctionName, szFunctionName) == 0)
			{
				return CMemory(m_pModuleBase + pAddressOfFunctions[reinterpret_cast<WORD*>(pAddressOfOrdinals)[i]]);
			}
		}

		return CMemory();
	}

	ModuleSections_t* const GetSectionByName(const char* const szSectionName) const
	{
		for (uint16_t i = 0; i < m_nNumSections; i++)
		{
			ModuleSections_t* const pSection = &m_ModuleSections[i];
			if (strcmp(szSectionName, pSection->m_SectionName) == 0)
				return pSection;
		}

		return nullptr;
	}

	inline uintptr_t GetModuleBase(void) const { return m_pModuleBase; }
	inline DWORD GetModuleSize(void) const { return m_nModuleSize; }
	inline const std::string& GetModuleName(void) const { return m_ModuleName; }

	inline ModuleSections_t* const GetSections() const { return m_ModuleSections; }
	inline uintptr_t GetRVA(const uintptr_t nAddress) const { return (nAddress - GetModuleBase()); }

#if _WIN64 
	void UnlinkFromPEB(void) const
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

#endif // #if _WIN64 

#if _WIN64 
	IMAGE_NT_HEADERS64* m_pNTHeaders;
#else
	IMAGE_NT_HEADERS32* m_pNTHeaders;
#endif // #if _WIN64 
	IMAGE_DOS_HEADER* m_pDOSHeader;

	ModuleSections_t* m_ExecutableCode;
	ModuleSections_t* m_ExceptionTable;
	ModuleSections_t* m_RunTimeData;
	ModuleSections_t* m_ReadOnlyData;

private:

	std::string         m_ModuleName;
	uintptr_t           m_pModuleBase;
	DWORD				m_nModuleSize;
	WORD				m_nNumSections;
	ModuleSections_t* m_ModuleSections;
};
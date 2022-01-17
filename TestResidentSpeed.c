#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dosextens.h>

#include <proto/exec.h>
#include <proto/dos.h>

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#include "CiaTimer.h"
#include "TimingFunctions.h"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

const char Version[] = "$VER: TestResidentSpeed 0.61 (17.1.2022) by Patrik Axelsson";

enum ComponentType {
	ComponentType_None,
	ComponentType_Memory,
	ComponentType_Resident,
	ComponentType_LibBase,
	ComponentType_EndMarker
};

struct Component {
	enum ComponentType type;
	void *address;
	unsigned version;
	unsigned revision;
	uint8_t *startAddress;
	size_t size;
	char name[32];
};

static struct Component *AddMemoriesToComponents(struct ExecBase *SysBase, struct List *memList, struct Component *nextComponent);
static struct Component *AddResidentsAndAssociatedLibBasesToComponents(struct ExecBase *SysBase, struct Resident **residentPtrList, struct Component *nextComponent);
static void TestComponentsSpeed(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, struct Component *startComponent, void *bestMem, bool showAll, bool verbose);

LONG TestResidentSpeed(void) {
	struct ExecBase *SysBase = *(struct ExecBase **) 4;
	// Variables needing cleanup at end
	struct DosLibrary *DOSBase = NULL;
	struct RDArgs *args = NULL;
	struct CiaTimer *ciaTimer = NULL;
	struct Component *components = NULL;
	
	LONG retval = RETURN_ERROR;

	DOSBase = (void *) OpenLibrary("dos.library", 36);
	if(NULL == DOSBase) {
		goto cleanup;
	}
	
	void *argsValues[2] = {0};
	args = ReadArgs("SHOWALL/S,VERBOSE/S", (void *) argsValues, NULL);
	if (NULL == args) {
		PrintFault(IoErr(), NULL);
		goto cleanup;
	}

	const bool showAll = NULL != argsValues[0];
	const bool verbose = NULL != argsValues[1];
	
	struct CiaTimer ciaTimerStore;
	ciaTimer = AllocCiaTimer(SysBase, &ciaTimerStore);
	if (NULL == ciaTimer) {
		Printf("Failed allocating a timer!\n");
		goto cleanup;
	}

	const size_t componentsSize = 256;
	components = AllocVec(sizeof(struct Component) * componentsSize, MEMF_ANY | MEMF_CLEAR);
	if (NULL == components) {
		Printf("Could not allocate buffer");
	}
	components[componentsSize - 1].type = ComponentType_EndMarker;

	StartCiaTimer(ciaTimer);

	struct Component *nextComponent = components;
	const struct Component *firstMemoryComponent = nextComponent;
	// The lists we are iterating can change during iteration if
	// task-switching is not forbidden.
	Forbid();
	nextComponent = AddMemoriesToComponents(SysBase, &SysBase->MemList, nextComponent);
	nextComponent = AddResidentsAndAssociatedLibBasesToComponents(SysBase, SysBase->ResModules, nextComponent);
	Permit();

	if (ComponentType_Memory != firstMemoryComponent->type) {
		PutStr("Found no memory!\n");
		goto cleanup;
	}
	void *bestMem = firstMemoryComponent->address;

	TestComponentsSpeed(SysBase, DOSBase, ciaTimer, components, bestMem, showAll, verbose); 

	retval = RETURN_OK;
cleanup:
	FreeVec(components);
	FreeCiaTimer(ciaTimer);
	if (NULL != args) {
		FreeArgs(args);
	}
	CloseLibrary((void *) DOSBase);
	
	return retval;
}

struct InitTable {
	ULONG it_LibBaseSize;
	void **it_FuncTable;
	ULONG *it_DataTable;
	APTR it_InitRoutine;
};

struct HumanSize {
	char string[8];
};

static void SPrintF(struct ExecBase *SysBase, char *buffer, const char *format, ...) {
	static const ULONG putCharProc = 0x16c04e75; // move.b d0,(a3)+; rts
	va_list varArgs;
	va_start(varArgs, format);
	RawDoFmt(format, varArgs, (void *) &putCharProc, buffer);
	va_end(varArgs);
}

struct HumanSize CalcHumanSize(struct ExecBase *SysBase, size_t size) {
	struct HumanSize humanSize;
	humanSize.string[0] = '\0';
	char units[] = {'k', 'M', 'G', 'T'};
	char unit = '\0';
	const char *format = "%lu";
	if (size < 999) {
		SPrintF(SysBase, humanSize.string, "%4lu", size);
	} 
	else {
		for (size_t i = 0; i < sizeof(units); i++) {
			if (size / 1024 < 999) {
				unit = units[i];
				if (size / 1024 < 10) {
					SPrintF(SysBase, humanSize.string, "%lu.%01lu%lc", size / 1024, (((size % 1024) * 999) / 1024) / 100, unit);
				}
				else {
					SPrintF(SysBase, humanSize.string, "%3lu%lc", size / 1024, unit);
				}
				break;
			}

			size /= 1024;
		}
	}

	return humanSize;
}

//TODO: Return calculated start and end of resident
static size_t CalcSizeFromResidentContents(struct Resident *resident) {
	void *minAddress = (void *) resident;
	void *maxAddress = (void *) (resident + 1);

	const struct InitTable *initTable = (void *) resident->rt_Init;
	const bool isAutoInit = NULL != initTable && RTF_AUTOINIT & resident->rt_Flags;

	const void * const addressList[] = {
		resident->rt_Name,
		resident->rt_IdString,
		resident->rt_Init,
		isAutoInit ? initTable->it_FuncTable : NULL,
		isAutoInit ? initTable->it_DataTable : NULL,
		isAutoInit ? initTable->it_InitRoutine : NULL
	};

	for (size_t i = 0; i < ARRAY_SIZE(addressList); i++) {
		const void *address = addressList[i];

		if (address != NULL) {
			if (address < minAddress) {
				minAddress = address;
			} 
			if (address > maxAddress) {
				maxAddress = address;
			}
		}
	}

	if (isAutoInit) {
		if (-1 == *(WORD *) initTable->it_FuncTable) {
			WORD minFuncOffset = 0;
			WORD maxFuncOffset = 0;
			WORD funcOffset;
			WORD *funcOffsetTable = ((WORD *) initTable->it_FuncTable) + 1;
			while (-1 != (funcOffset = *funcOffsetTable++)) {
				if (funcOffset < minFuncOffset) {
					minFuncOffset = funcOffset;
				}
				if (funcOffset > maxFuncOffset) {
					maxFuncOffset = funcOffset;
				}
			}

			if ((void *) funcOffsetTable > maxAddress) {
				maxAddress = funcOffsetTable;
			}
			
			void *minFunctionAddress = (uint8_t *) initTable->it_FuncTable + minFuncOffset;
			if (minFunctionAddress < minAddress) {
				minAddress = minFunctionAddress;
			}
			void *maxFunctionAddress = (uint8_t *) initTable->it_FuncTable + maxFuncOffset;
			if (maxFunctionAddress > maxAddress) {
				maxAddress = maxFunctionAddress;
			}
		}
		else {
			void *function;
			void **functionTable = initTable->it_FuncTable;;
			while (-1 != (ULONG) (function = *functionTable++)) {
				if (function < minAddress) {
					minAddress = function;
				}
				if (function > maxAddress) {
					maxAddress = function;
				}
			}
			if ((void *) functionTable > maxAddress) {
				maxAddress = functionTable;
			}
		}
	}

	return (uint8_t *) maxAddress - (uint8_t *) minAddress;
}

struct FullTimingResult {
	uint32_t eClocks;
	size_t actualLength;
	uint32_t speed;
};

static struct FullTimingResult TimeReads(struct ExecBase *SysBase, struct DosLibrary *DOSBase, struct CiaTimer *ciaTimer, void *addr, size_t length) {

	struct TimingResult medianResult;
	size_t proposedTestLength = 0;
	do {
		proposedTestLength += 256;
		const size_t testLength = proposedTestLength < length ? proposedTestLength : length;
		// Make several tests and get the median to filter out oddly slow results (memory refresh?)
		struct TimingResult results[5];
		for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
			Disable();
			CacheClearU();
			// Dry run to populate the instruction cache
			uint8_t testBuffer[384];
			TimeConsequtiveReads(ciaTimer->registers.lowByte, testBuffer, length < sizeof(testBuffer) ? length : sizeof(testBuffer)); 
			results[i] = TimeConsequtiveReads(ciaTimer->registers.lowByte, addr, testLength);
			Enable();
		}

		for (size_t i = 0; i < ARRAY_SIZE(results) - 1; i++) {
			uint32_t currentMinEClocks = results[i].eClocks;
			size_t minIndex = i;
			for (size_t j = i; j < ARRAY_SIZE(results); j++) {
				if (results[j].eClocks < currentMinEClocks) {
					currentMinEClocks = results[j].eClocks;
					minIndex = j;
				}
			}
			struct TimingResult tmpResult = results[i];
			results[i] = results[minIndex];
			results[minIndex] = tmpResult;
		}

		/*for (size_t i = 0; i < ARRAY_SIZE(results); i++) {
			Printf("%lu: %lu %lu %lu\n", i, testLength, results[i].eClocks, results[i].actualLength);
		}*/

		medianResult = results[ARRAY_SIZE(results) >> 1];

		if (medianResult.eClocks > 128) {
			break;
		}
	} while (proposedTestLength < length);


	return (struct FullTimingResult) {
		.eClocks = medianResult.eClocks,
		.actualLength = medianResult.actualLength,
		.speed = medianResult.eClocks != 0 ?
				(uint32_t) ((((uint64_t) medianResult.actualLength) * SysBase->ex_EClockFrequency) / medianResult.eClocks) :
				-1
	};
}


static bool IsPrintableIso8859(uint8_t c) {
	return !(c < 0x20 || (c > 0x7e && c < 0xa0));
}

static const char *GetLocation(struct ExecBase *SysBase, void *addr) {
	const ULONG memType = TypeOfMem(addr);
	const size_t addrNum = (size_t) addr; 
	
	return memType & MEMF_CHIP ?
			"Chip RAM" :
			memType & MEMF_FAST ?
					"Fast RAM" :
					(addrNum >= 0xe00000 && addrNum <= 0xefffff) ?
							"Ext  ROM" :
							(addrNum >= 0xF00000 && addrNum <= 0xf7ffff) ?
									"Diag ROM" :
									(addrNum >= 0xf80000 && addrNum <= 0xffffff) ?
											"Kick ROM" :
											"Unknown";
}

static struct Component *AddToComponents(
		struct Component *nextComponent,
		enum ComponentType type,
		void *address,
		unsigned version,
		unsigned revision,
		void *startAddress,
		size_t size,
		const char *name
) {
	if (nextComponent->type != ComponentType_None) {
		return nextComponent;
	}
	nextComponent->type = type;
	nextComponent->address = address;
	nextComponent->version = version;
	nextComponent->revision = revision;
	nextComponent->startAddress = startAddress;
	nextComponent->size = size;
	// Components array is initialized to zero, so name is safe to use as is.
	if (NULL != name) {
		// strncpy() is forever broken and generates unterminated strings
		// when source is longer than the destination, but we avoid that
		// here as components array is initialized to zero and we don't
		// allow it to overwrite the last position of name.
		strncpy(nextComponent->name, name, sizeof(nextComponent->name) - 1);
	}

	return nextComponent + 1;
}

static struct Component *FindComponentByAddress(struct Component *startComponent, const void *address) {
	struct Component *component = startComponent;

	while (component->type != ComponentType_None && component->type != ComponentType_EndMarker) {
		if (component->address == address) {
			return component;
		}
		component++;
	}
	return NULL;
}

static const char *ComponentType2String(const enum ComponentType type) {
	const char *strings[] = {
		"None",
		"Memory",
		"Resident",
		"LibBase",
	};

	if (ComponentType_EndMarker <= type) {
		return "Unknown";
	}

	return strings[type];
}

static struct Component *AddMemoryToComponents(
		struct Component *nextComponent,
		struct MemHeader *memHeader
) {
	if (NULL == memHeader) {
		return nextComponent;
	}
	return AddToComponents(
			nextComponent,
			ComponentType_Memory,
			memHeader,
			0,
			0,
			memHeader->mh_Lower,
			(uint8_t *) memHeader->mh_Upper - (uint8_t *) memHeader,
			memHeader->mh_Node.ln_Name
	);
}

static struct Component *AddResidentToComponents(
		struct Component *nextComponent,
		struct Resident *resident
) {
	if (NULL == resident) {
		return nextComponent;
	}
	const size_t endskipSize = (resident->rt_EndSkip >= (void *) resident) ?
			((uint8_t *) resident->rt_EndSkip - (uint8_t *) resident) :
			0;
	const size_t calcSize = CalcSizeFromResidentContents(resident);
	const size_t size = calcSize > endskipSize ? calcSize : endskipSize;
	return AddToComponents(
			nextComponent,
			ComponentType_Resident,
			resident,
			resident->rt_Version,
			0,
			resident,
			size,
			resident->rt_Name
	);
}

static struct Component *AddLibBaseToComponents(
		struct Component *nextComponent,
		struct Library *libBase
) {
	if (NULL == libBase) {
		return nextComponent;
	}
	if (NULL != libBase->lib_Node.ln_Name && 0 == strcmp("FileSystem.resource", libBase->lib_Node.ln_Name)) {
		return nextComponent;
	}
	return AddToComponents(
			nextComponent,
			ComponentType_LibBase,
			libBase,
			libBase->lib_Version,
			libBase->lib_Revision,
			(uint8_t *) libBase - libBase->lib_NegSize,
			(size_t) libBase->lib_PosSize + libBase->lib_NegSize,
			libBase->lib_Node.ln_Name
	);
}

static struct Component *AddMemoriesToComponents(struct ExecBase *SysBase, struct List *memList, struct Component *nextComponent) {
	struct MemHeader *memHeader;
	for (memHeader = (void *) memList->lh_Head; NULL != memHeader->mh_Node.ln_Succ; memHeader = (void *) memHeader->mh_Node.ln_Succ) {
		nextComponent = AddMemoryToComponents(nextComponent, memHeader);
	}
	return nextComponent;
}

static struct Component *AddLibBasesAssociatedWithResidentToComponents(
		struct ExecBase *SysBase,
		struct Resident *resident,
		struct Component *nextComponent
) {
	nextComponent = AddLibBaseToComponents(nextComponent, (void *) FindName(&SysBase->LibList, resident->rt_Name));
	nextComponent = AddLibBaseToComponents(nextComponent, (void *) FindName(&SysBase->DeviceList, resident->rt_Name));
	nextComponent = AddLibBaseToComponents(nextComponent, (void *) FindName(&SysBase->ResourceList, resident->rt_Name));
	return nextComponent;
}

static bool IsLink(struct Resident *resident) {
	// If most significant bit is set, it is a link.
	return (int32_t) resident < 0;
}

static struct Resident **GetLinkAddr(struct Resident *resident) {
	return (void *) (((uint32_t) resident << 1) >> 1);
}

static struct Component *RecursiveAddResidentsAndAssociatedLibBasesToComponents(
		struct ExecBase *SysBase,
		struct Resident **residentPtrList,
		struct Component *startComponent,
		struct Component *nextComponent
) {
	for (struct Resident **residentPtr = residentPtrList; *residentPtr != NULL; residentPtr++) {
		struct Resident *resident = *residentPtr;
		if (IsLink(resident)) {
			const struct Resident **residentLinkPtr = GetLinkAddr(resident);
			nextComponent = RecursiveAddResidentsAndAssociatedLibBasesToComponents(SysBase, residentLinkPtr, startComponent, nextComponent);
		}
		else if (resident->rt_MatchWord == RTC_MATCHWORD) {
			// Don't add already added residents again.
			if (NULL != FindComponentByAddress(startComponent, resident)) {
				continue;
			}
			// Only add the version of a resident in use in the system.
			if (resident != FindResident(resident->rt_Name)) {
				continue;
			}

			nextComponent = AddResidentToComponents(nextComponent, resident);
			nextComponent = AddLibBasesAssociatedWithResidentToComponents(SysBase, resident, nextComponent);
		}
	}
	return nextComponent;
}

static struct Component *AddResidentsAndAssociatedLibBasesToComponents(struct ExecBase *SysBase, struct Resident **residentPtrList, struct Component *nextComponent) {
	return RecursiveAddResidentsAndAssociatedLibBasesToComponents(SysBase, residentPtrList, nextComponent, nextComponent);
}

static char *SanitizeForPrinting(const char *src, char *dest, const size_t destSize) {
	size_t i = 0;
	size_t j = 0;
	if (NULL != src) {
		while (j < destSize - 1) {
			const uint8_t c = src[i++];
			if (c == '\0')
				break;
			if(IsPrintableIso8859(c)) {
				dest[j++] = c;
			}
		}
		dest[j] = '\0';
	}

	return dest;
}

static void PrintComponents(
		struct ExecBase *SysBase,
		struct DosLibrary *DOSBase,
		struct CiaTimer *ciaTimer,
		struct Component *startComponent,
		void *bestMem,
		bool showAll,
		bool verbose
) {
	struct Component *component = startComponent;

	if (verbose) {
		Printf("%-8s %-23s %-7s %-4s %-8s %-8s %-6s %-4s %-8s %-6s %-5s %-8s %-7s\n", "Type", "Name", "Version", "Size", "Address", "Location", "TSize", "EClk", "Speed", "ITSize", "IEClk", "ISpeed", "Speed %");
	}
	else {
		Printf("%-8s %-23s %-7s %-4s %-8s %-8s %-7s\n", "Type", "Name", "Version", "Size", "Location", "Speed", "Speed %");
	}

	while (component->type != ComponentType_None && component->type != ComponentType_EndMarker) {
		char sanitizedName[24];
		SanitizeForPrinting(component->name, sanitizedName, sizeof(sanitizedName));
		const char *componentTypeString = ComponentType2String(component->type);
		const char *location = GetLocation(SysBase, component->startAddress);

		const struct FullTimingResult result = TimeReads(SysBase, DOSBase, ciaTimer, component->startAddress, component->size);

		const struct FullTimingResult resultBestMem = TimeReads(SysBase, DOSBase, ciaTimer, (uint8_t *) bestMem + (((size_t) component->startAddress) & 0xff), component->size);

		const unsigned percentage = 0 != resultBestMem.speed ?
				((uint64_t) result.speed * 100 * 10) / resultBestMem.speed :
				0;
				const bool sameSize = result.actualLength == resultBestMem.actualLength;
				const bool oneEclockMore = result.eClocks == resultBestMem.eClocks + 1;
		if (showAll || ComponentType_Memory != component->type && percentage < 98 * 10 && !(sameSize && oneEclockMore)) {
			const struct HumanSize humanSize = CalcHumanSize(SysBase, component->size); 
			if (verbose) {
				Printf(
						"%-8s %-23s %3lu.%-3lu %s %08lx %-8s %6lu %4lu %4lu.%03lu %6lu %5lu %4lu.%03lu %5lu.%01lu\n",
						componentTypeString,
						sanitizedName,
						component->version,
						component->revision,
						humanSize.string,
						component->address,
						location,
						result.actualLength,
						result.eClocks,
						result.speed / (1024 * 1024),
						((result.speed % (1024 * 1024)) * 999) / (1024 * 1024),
						resultBestMem.actualLength,
						resultBestMem.eClocks,
						resultBestMem.speed / (1024 * 1024),
						((resultBestMem.speed % (1024 * 1024)) * 999) / (1024 * 1024),
						percentage / 10,
						percentage % 10

				);
			}
			else {
				Printf(
						"%-8s %-23s %3lu.%-3lu %s %-8s %4lu.%03lu %5lu.%01lu\n",
						componentTypeString,
						sanitizedName,
						component->version,
						component->revision,
						humanSize.string,
						location,
						result.speed / (1024 * 1024),
						((result.speed % (1024 * 1024)) * 999) / (1024 * 1024),
						percentage / 10,
						percentage % 10

				);
			}
		}

		component++;
	}
}

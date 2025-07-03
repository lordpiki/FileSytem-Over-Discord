#include <fltKernel.h>
#include <dontuse.h>
#include <suppress.h>

PFLT_FILTER FilterHandle = NULL;

// Deare the function prototypes
NTSTATUS MiniUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
FLT_PREOP_CALLBACK_STATUS MiniPreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID *CompletionContext
);
FLT_POSTOP_CALLBACK_STATUS MiniPostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID* CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
);
FLT_PREOP_CALLBACK_STATUS MiniPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID* CompletionContext
);


const FLT_OPERATION_REGISTRATION Callbacks[] = {
	// This is a pass-through filter, so we don't register any callbacks
	{ IRP_MJ_CREATE, 0, MiniPreCreate, MiniPostCreate},
	{ IRP_MJ_READ, 0, NULL, NULL }, // TODO
	{ IRP_MJ_WRITE, 0, MiniPreWrite, NULL}, // TODO
	{ IRP_MJ_CLOSE, 0, NULL, NULL }, // TODO
	{ IRP_MJ_CLEANUP, 0, NULL, NULL }, // TODO
	{ IRP_MJ_OPERATION_END }
};

const FLT_REGISTRATION FilterRegistration = {
	sizeof(FLT_REGISTRATION), // Size of the structure
	FLT_REGISTRATION_VERSION, // Version of the structure
	0, // Flags
	NULL, // Context registration
	Callbacks, // Operation callbacks
	MiniUnload, // FilterUnloadCallback
	NULL, // InstanceSetupCallback
	NULL, // InstanceQueryTeardownCallback
	NULL, // InstanceTeardownStartCallback
	NULL, // InstanceTeardownCompleteCallback
	NULL, // GenerateFileNameCallback
	NULL, // GenerateDestinationFileNameCallback
	NULL  // NormalizeNameComponentCallback
};

NTSTATUS MiniUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
	KdPrint(("Driver Unload \n"));
	FltUnregisterFilter(FilterHandle);

	return STATUS_SUCCESS;
}

FLT_POSTOP_CALLBACK_STATUS MiniPostCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID* CompletionContext,
	_In_ FLT_POST_OPERATION_FLAGS Flags
)
{
	KdPrint(("Post Create Operation\n"));
	return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS MiniPreWrite(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID* CompletionContext
)
{
	PFLT_FILE_NAME_INFORMATION FileNameInfo;
	NTSTATUS status;
	WCHAR Name[260] = { 0 };
	const WCHAR TargetFolder[] = L"\\DOCUMENTS\\TEST\\"; // Relative path, uppercase for comparison

	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &FileNameInfo);

	if (NT_SUCCESS(status))
	{
		status = FltParseFileNameInformation(FileNameInfo);

		if (NT_SUCCESS(status))
		{
			if (FileNameInfo->Name.Length < sizeof(Name))
			{
				RtlCopyMemory(Name, FileNameInfo->Name.Buffer, FileNameInfo->Name.Length);
				Name[FileNameInfo->Name.Length / sizeof(WCHAR)] = L'\0'; // Null-terminate

				_wcsupr(Name); // Convert to uppercase for easier comparison

				// Look for the relative path "\DOCUMENTS\TEST\" anywhere in the file name
				if (wcsstr(Name, TargetFolder) != NULL)
				{
					KdPrint(("Write file in target folder: %ws blocked\n", Name));
					Data->IoStatus.Status = STATUS_SUCCESS;
					Data->IoStatus.Information = 0;
					FltReleaseFileNameInformation(FileNameInfo);
					return FLT_PREOP_COMPLETE;
				}

				KdPrint(("File Name: %ws\n", Name));
			}
		}

		FltReleaseFileNameInformation(FileNameInfo);
	}

	return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS MiniPreCreate(
	_Inout_ PFLT_CALLBACK_DATA Data,
	_In_ PCFLT_RELATED_OBJECTS FltObjects,
	_Out_ PVOID *CompletionContext
)
{
	PFLT_FILE_NAME_INFORMATION FileNameInfo;
	NTSTATUS status;
	WCHAR Name[260] = { 0 };

	status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &FileNameInfo);

	if (NT_SUCCESS(status))
	{
		status = FltParseFileNameInformation(FileNameInfo);

		if (NT_SUCCESS(status))
		{
			if (FileNameInfo->Name.Length < sizeof(Name))
			{
				RtlCopyMemory(Name, FileNameInfo->Name.Buffer, FileNameInfo->Name.Length);
				KdPrint(("File Name: %ws\n", Name));
			}
		}
		
		FltReleaseFileNameInformation(FileNameInfo);
	}

	return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS status;

	// Try to register the filter
	status = FltRegisterFilter(DriverObject, &FilterRegistration, &FilterHandle);

	// If we succeeded, continue, else, return the status
	if (NT_SUCCESS(status))
	{
		// Start the filtering
		status = FltStartFiltering(FilterHandle);

		// If the filtering is UNSECCESSFUL, unregister it
		if (!NT_SUCCESS(status))
		{
			FltUnregisterFilter(FilterHandle);
		}
	}

	return status;
}
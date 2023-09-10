#include <windows.h>
#include <stdio.h>
#include <locale.h>

#include "../core/core.h"

FILE_NOTIFY_INFORMATION fileChangeBuffer[64];
typedef struct {
	char filename[64];
	unsigned long long lastWriteTime;
} file_info;
file_info files[256];
int filesCount = 0;

HANDLE directoryHandles[64];
HANDLE ioHandles[64];
OVERLAPPED directoryOverlapped[64];

file_info* getFile(char* filename) {
	for(int i=0; i<filesCount; ++i) {
		if(!strcmp(files[i].filename, filename)) {
			return files + i;
		}
	}
	return NULL;
}

void addFile(char* filename, unsigned long long lastWriteTime) {
	if(filesCount < 256) {
		file_info* file = &files[filesCount++];
		strcpy(file->filename, filename);
		file->lastWriteTime = lastWriteTime;
	}
}

void clear() {
	system("cls");
}

int build(char* filename) {
	clear();
	printf("file changed %s \n", filename);

	int result = system("sh ./build.sh");
	// printf("result %i \n", result);

	if(!result) {
		printf("\033[92mbuild successful\033[0m \n");
	} else {
		printf("\033[91mbuild failed\033[0m \n");
	}
	return result;
}

void handleChange(int dirIndex) {
	FILE_NOTIFY_INFORMATION *fileChange = fileChangeBuffer;
	while(fileChange) {
		char filename[64] = {0};
		for(int i=0; i<fileChange->FileNameLength/sizeof(*fileChange->FileName); ++i) {
			filename[i] = fileChange->FileName[i];
		}

		if( sfind(filename, ".c") ||
			sfind(filename, ".h") ||
			sfind(filename, ".txt") ||
			sfind(filename, ".sh")) {
			file_info* file = getFile(filename);
			HANDLE fileHandle = CreateFileA(
				filename,
				GENERIC_READ,
				FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			FILETIME lastWriteTime;
			GetFileTime(fileHandle, NULL, NULL, &lastWriteTime);

			// the laste write time is the number of 100-nanosecond intervals
			unsigned long long writeTime = (long long )lastWriteTime.dwHighDateTime<<32 | lastWriteTime.dwLowDateTime;
			writeTime /= 10 * 1000; // milliseconds

			if(file) {
				// If more than a second has passed
				if(writeTime-file->lastWriteTime > 1000) {
					// printf("file changed %s time difference %li \n", filename, writeTime-file->lastWriteTime);
					file->lastWriteTime = writeTime;
					build(filename);
				}
			} else {
				addFile(filename, writeTime);
				build(filename);
			}
			CloseHandle(fileHandle);
		}

		if(fileChange->NextEntryOffset) {
			fileChange = (char*)fileChange + fileChange->NextEntryOffset;
		} else {
			fileChange = 0;
		}
	}
}

void completionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
	// printf("LpoverlappedCompletionRoutine %i \n", dwErrorCode);
	// printf("dir %i \n", lpOverlapped->hEvent);
	int dirIndex = lpOverlapped->hEvent;

	handleChange(dirIndex);

	ReadDirectoryChangesW(
		directoryHandles[dirIndex],
		fileChangeBuffer,
		sizeof(fileChangeBuffer),
		TRUE,
		FILE_NOTIFY_CHANGE_LAST_WRITE,
		NULL,
		&directoryOverlapped[dirIndex],
		&completionRoutine);
}

int main(int argc, char **argv) {
	if(argc < 2) {
		printf("Arg 1 should be a directory to watch \n");
		exit(1);
	}

	char* dirPath = argv[1];
	SetCurrentDirectoryA(dirPath);

	// HANDLE notificationHandle = FindFirstChangeNotificationA(
	// 	"./*.c",
	// 	TRUE,
	// 	FILE_NOTIFY_CHANGE_LAST_WRITE);
	// if(notificationHandle == INVALID_HANDLE_VALUE) {
	// 	printf("FindFirstChangeNotificationA \n");
	// 	printf("Failed with error code %d \n", GetLastError());
	// }

	int dirCount = argc-1;
	if(dirCount > 64) {
		printf("Too many directories \n");
		exit(1);
	}
	
	for(int i=0; i<dirCount; ++i) {
		directoryHandles[i] = CreateFileA(
			argv[i+1],
			FILE_LIST_DIRECTORY,
			FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,
			NULL);

		ioHandles[i] = CreateEventA(NULL, FALSE, FALSE, argv[i+1]);
		directoryOverlapped[i] = (OVERLAPPED){0};
		directoryOverlapped[i].hEvent = i;//ioHandles[i];
	}

	printf("\033[93mlistening on:\033[0m \n");
	for(int i=0; i<dirCount; ++i) printf("\033[93m%s\033[0m \n", argv[i+1]);

	for(int i=0; i<dirCount; ++i) {
		long bytes;
		int read = ReadDirectoryChangesW(
			directoryHandles[i],
			fileChangeBuffer,// + (i*8),
			sizeof(fileChangeBuffer),///8,
			TRUE,
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			&bytes,
			&directoryOverlapped[i],
			&completionRoutine);
	}

	for(;;) {
		DWORD waitResult = WaitForMultipleObjectsEx(dirCount, directoryHandles, FALSE, INFINITE, TRUE);
		// TODO what is waitResult now?
		// printf("waitResult %i \n", waitResult);

		// if(waitResult >= WAIT_OBJECT_0+argc-1) {
			// printf("\033[91mwait error %i\033[0m \n", waitResult);
			// exit(1);
		// }

		// int dirIndex = waitResult-WAIT_OBJECT_0;

		// long bytesTransfered;
		// GetOverlappedResult(directoryHandles[dirIndex], &directoryOverlapped[dirIndex], &bytesTransfered, FALSE);
	}
}
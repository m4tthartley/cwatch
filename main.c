#include <windows.h>
#include <stdio.h>
#include <locale.h>

#include <core/core.h>

#define VERSION "0.3.0.dev"
#define SIMULATE_MULTIPLE_DIRECTORY 0

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
string paths[64];

file_info* getFile(char* filename) {
	for(int i=0; i<filesCount; ++i) {
		if(!strcmp(files[i].filename, filename)) {
			return files + i;
		}
	}
	return NULL;
}

void addFile(char* filename, unsigned long long lastWriteTime) {
	printf("adding file %s \n", filename);
	if(filesCount < 256) {
		file_info* file = &files[filesCount++];
		strncpy(file->filename, filename, 63);
		file->lastWriteTime = lastWriteTime;
	}
}

void clear() {
	// system("cls");
}

int build(char* filename) {
	char cwd[MAX_PATH] = {0};
	GetCurrentDirectoryA(MAX_PATH, cwd);
	SetCurrentDirectoryA(paths[0]);

	clear();
	printf("file changed %s \n", filename);

	int result = system("sh ./build.sh");
	// printf("result %i \n", result);

	if(!result) {
		printf("\033[92mbuild successful\033[0m \n");
	} else {
		printf("\033[91mbuild failed\033[0m \n");
	}

	SetCurrentDirectoryA(cwd);
	return result;
}

void handleChange(int dirIndex) {
	printf("Handling file changes... \n");
	FILE_NOTIFY_INFORMATION *fileChange = fileChangeBuffer;
	int loop_count = 0;
	while(fileChange) {
		printf("\nFILE_NOTIFY_INFORMATION \n");
		printf("offset is %i \n", fileChange->NextEntryOffset);
		char rawFileName[MAX_PATH+1] = {0};
		for(int i=0; i<fileChange->FileNameLength/sizeof(*fileChange->FileName); ++i) {
			rawFileName[i] = fileChange->FileName[i];
		}
		string filename = s_create(rawFileName);
		s_prepend(&filename, "/");
		s_prepend(&filename, paths[dirIndex]);
		printf("file change %s \n", filename);
		s_free(filename);

		printf("after filename conversion %i \n", fileChange->NextEntryOffset);

		if( s_find(filename, ".c", 0) ||
			s_find(filename, ".h", 0) ||
			s_find(filename, ".txt", 0) ||
			s_find(filename, ".sh", 0)) {
			file_info* file = getFile(filename);
			HANDLE fileHandle = CreateFileA(
				filename,
				GENERIC_READ,
				FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				NULL);
			// if(fileHandle == INVALID_HANDLE_VALUE) {
			// 	printf("CreateFileA failed with error code %d \n", GetLastError());
			// 	return;
			// }
			FILETIME fileTime = {0};
			GetFileTime(fileHandle, NULL, NULL, &fileTime);

			// the laste write time is the number of 100-nanosecond intervals
			unsigned long long writeTime = ((long long )fileTime.dwHighDateTime)<<32 | fileTime.dwLowDateTime;
			writeTime /= 10 * 1000; // milliseconds

			if(file) {
				// printf("file change %s, diff %li \n", filename, writeTime-file->lastWriteTime);
				// printf("%li, %li (%i, %i) \n", writeTime, file->lastWriteTime, fileTime.dwHighDateTime, fileTime.dwLowDateTime);
				// If more than a second has passed
				if(writeTime-file->lastWriteTime > 1000) {
					// printf("file changed %s time difference %li \n", filename, writeTime-file->lastWriteTime);
					file->lastWriteTime = writeTime;
					
					// build(filename);
					{
						int next_offset1 = fileChange->NextEntryOffset;
						printf("before build %i \n", fileChange->NextEntryOffset);

						char cwd[MAX_PATH] = {0};
						GetCurrentDirectoryA(MAX_PATH, cwd);
						SetCurrentDirectoryA(paths[0]);

						printf("during build 1 %i \n", fileChange->NextEntryOffset);

						clear();
						printf("file changed %s \n", filename);

						printf("during build 2 %i \n", fileChange->NextEntryOffset);

						int result = system("sh ./build.sh");
						// printf("result %i \n", result);

						printf("during build 3 %i \n", fileChange->NextEntryOffset);

						if(!result) {
							printf("\033[92mbuild successful\033[0m \n");
						} else {
							printf("\033[91mbuild failed\033[0m \n");
						}

						SetCurrentDirectoryA(cwd);

						printf("after build %i \n", fileChange->NextEntryOffset);
						if (fileChange->NextEntryOffset != next_offset1) {
							int x = 0;
						}
					}
				} else {
					printf("didn't change file %s, diff %li \n", filename, writeTime-file->lastWriteTime);
				}
			} else {
				// printf("file change %s, new \n", filename);
				printf("before add file %i \n", fileChange->NextEntryOffset);
				addFile(filename, writeTime);
				printf("before build %i \n", fileChange->NextEntryOffset);
				build(filename);
			}
			CloseHandle(fileHandle);
		}

		printf("after if %i \n", fileChange->NextEntryOffset);

		if(fileChange->NextEntryOffset) {
			printf("offsetting by %i \n", fileChange->NextEntryOffset);
			if (fileChange->NextEntryOffset > 64) {
				int x = 0;
			}
			fileChange = (char*)fileChange + fileChange->NextEntryOffset;
			++loop_count;
		} else {
			fileChange = 0;
		}
	}
}

void completionRoutine(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
	printf("completion routine \n");
	
	int dirIndex = *((int*)&lpOverlapped->hEvent);

	handleChange(dirIndex);

	memset(fileChangeBuffer, 0, sizeof(fileChangeBuffer));

	BOOL readResult = ReadDirectoryChangesW(
		directoryHandles[dirIndex],
		fileChangeBuffer,
		sizeof(fileChangeBuffer),
		TRUE,
		FILE_NOTIFY_CHANGE_LAST_WRITE,
		NULL,
		&directoryOverlapped[dirIndex],
		&completionRoutine);
	if(!readResult) {
		printf("\033[91mReadDirectoryChangesW failed\033[0m \n");
	}
}

int main(int _argc, char **_argv) {
	if(_argc < 2) {
		printf("Arg 1 should be a directory to watch \n");
		exit(1);
	}

	u8 strBuffer[PAGE_SIZE];
	string_pool spool;
	s_create_pool(&spool, strBuffer, sizeof(strBuffer));
	s_pool(&spool);

	// HANDLE notificationHandle = FindFirstChangeNotificationA(
	// 	"./*.c",
	// 	TRUE,
	// 	FILE_NOTIFY_CHANGE_LAST_WRITE);
	// if(notificationHandle == INVALID_HANDLE_VALUE) {
	// 	printf("FindFirstChangeNotificationA \n");
	// 	printf("Failed with error code %d \n", GetLastError());
	// }

	int dirCount = _argc-1;
	if(dirCount > 64) {
		printf("Too many directories \n");
		exit(1);
	}

	for(int i=0; i<dirCount; ++i) {
		paths[i] = s_create(_argv[i+1]);
	}

	// char* cwd = paths[0];
	// SetCurrentDirectoryA(cwd);

	if(dirCount > 1 && SIMULATE_MULTIPLE_DIRECTORY) {
		dirCount = 1;
		s_append(paths, "/..");
	}
	
	for(int i=0; i<dirCount; ++i) {
		directoryHandles[i] = CreateFileA(
			paths[i],
			FILE_LIST_DIRECTORY,
			FILE_SHARE_DELETE|FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OVERLAPPED,
			NULL);
		if(directoryHandles[i] == INVALID_HANDLE_VALUE) {
			printf("\033[91mCreateFileA failed\033[0m \n");
		}

		ioHandles[i] = CreateEventA(NULL, FALSE, FALSE, paths[i]);
		directoryOverlapped[i] = (OVERLAPPED){0};
		directoryOverlapped[i].hEvent = (void*)(u64)i;//ioHandles[i];
	}

	printf("\033[93mcwatch version %s\033[0m \n", VERSION);
	printf("\033[93mlistening on:\033[0m \n");
	for(int i=0; i<dirCount; ++i) printf("\033[93m%s\033[0m \n", paths[i]);

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
		if(!read) {
			printf("\033[91mReadDirectoryChangesW failed\033[0m \n");
		}
	}

	for(;;) {
		DWORD waitResult = WaitForMultipleObjectsEx(dirCount, directoryHandles, FALSE, INFINITE, TRUE);
		if(waitResult != WAIT_IO_COMPLETION) {
			printf("\033[91mWaitForMultipleObjectsEx returned something other than WAIT_IO_COMPLETION: %i\033[0m \n", waitResult);
		}
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
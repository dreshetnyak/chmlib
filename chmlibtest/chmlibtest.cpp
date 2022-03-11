// chmlibtest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>

#include "chm_lib.h"

typedef struct IStreamReaderCtx { IStream* Stream; } IStreamReaderCtx;
int64_t IStreamReader(void* ctxPtr, void* buffer, int64_t offset, int64_t size);

int main()
{
    chm_file chmFile;
    IStreamReaderCtx ctx{ nullptr };
    bool parsingResult = chm_parse(&chmFile, IStreamReader, &ctx);
    if (!parsingResult)
		return 1;
    
    printf("%s:\n", path);
    parsingResult = extract(&chmFile, base_path);
    chm_close(&chmFile);
    fd_reader_close(&ctx);
    
    std::cout << "Hello World!\n";
}



int64_t IStreamReader(void* ctxPtr, void* buffer, const int64_t offset, const int64_t size)
{
    const auto ctx = static_cast<IStreamReaderCtx*>(ctxPtr);
    if (ctx == nullptr)
	    return -1;
    const auto stream = ctx->Stream;
    if (stream == nullptr)
        return -1;

    // Obtain the initial position
    constexpr LARGE_INTEGER fileBeginPosition{ { 0, 0 } };
    ULARGE_INTEGER initialPosition{};
    if (FAILED(stream->Seek(fileBeginPosition, STREAM_SEEK_CUR, &initialPosition)))
        return -1;

    // Set the position to the read offset
    LARGE_INTEGER position;
    position.QuadPart = offset;
    if (FAILED(stream->Seek(position, STREAM_SEEK_SET, nullptr)))
        return -1;

    // Read the data from the position
    ULONG bytesRead{};
    if (const auto result = stream->Read(buffer, static_cast<ULONG>(size), &bytesRead); FAILED(result))
        return result;
    
    // Restore the original position
    position.QuadPart = static_cast<LONGLONG>(initialPosition.QuadPart);
    if (FAILED(stream->Seek(position, STREAM_SEEK_SET, nullptr)))
        return -1;

    return bytesRead;
}

HRESULT ReadFile(void* data, const int64_t dataSize)
{
    mem_reader_ctx ctx{};
    mem_reader_init(&ctx, data, dataSize);

    chm_file chmFile;
    if (!chm_parse(&chmFile, fd_reader, &ctx))
        return E_FAIL;

    printf("%s:\n", path);
    std::codecvt_base::ok = extract(&chmFile, base_path);

	chm_close(&chmFile);
    return S_OK;
}



static bool extract_entry(chm_file* chmFile, chm_entry* chmEntry, const char* basePath)
{
    int64_t pathLen;
    char buffer[32768];
    char* i;

    if (chmEntry->path[0] != '/')
        return true;

    /* quick hack for security hole mentioned by Sven Tantau */
    if (strstr(chmEntry->path, "/../") != NULL) {
        /* fprintf(stderr, "Not extracting %s (dangerous path)\n", ui->path); */
        return true;
    }

    if (snprintf(buffer, sizeof(buffer), "%s%s", basePath, chmEntry->path) > 1024) {
        return false;
    }

    /* Get the length of the path */
    pathLen = strlen(chmEntry->path) - 1;

    if (chmEntry->path[pathLen] == '/') {
        /* this is directory */
        return rmkdir(buffer) != -1;
    }

    /* this is file */
    FILE* fout;

    printf("--> %s\n", chmEntry->path);
    if ((fout = fopen(buffer, "wb")) == NULL) {
        /* make sure that it isn't just a missing directory before we abort */
        char newbuf[32768];
        strcpy(newbuf, buffer);
        i = strrchr(newbuf, '/');
        *i = '\0';
        rmkdir(newbuf);
        if ((fout = fopen(buffer, "wb")) == NULL)
            return false;
    }

    auto remain = chmEntry->length;
    int64_t offset = 0;
	while (remain != 0) 
    {
        const auto len = chm_retrieve_entry(chmFile, chmEntry, reinterpret_cast<uint8_t*>(buffer), offset, sizeof buffer);
        if (len <= 0)
            return false; //fprintf(stderr, "incomplete file: %s\n", chmEntry->path);
                
        fwrite(buffer, 1, static_cast<size_t>(len), fout);
        offset += len;
        remain -= len;        
    }

    fclose(fout);
    return true;
}

static bool extract(chm_file* chmFile, const char* basePath)
{
    /* extract as many entries as possible */
    for (int entryIndex = 0; entryIndex < chmFile->n_entries; entryIndex++) 
    {
        if (!extract_entry(chmFile, chmFile->entries[entryIndex], basePath))
	        return false;
    }

    return !chmFile->parse_entries_failed;
}


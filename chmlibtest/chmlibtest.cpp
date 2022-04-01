// chmlibtest.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <filesystem>
#include <functional>
#include <iostream>
#include <thumbcache.h>
#include <windows.h>

#include "chm_lib.h"
#include "Gfx.h"
#include "shlwapi.h"
#include "StrLib.h"
#include "XmlDocument.h"

using namespace std;

typedef struct IStreamReaderCtx { IStream* Stream; } IStreamReaderCtx;
int64_t IStreamReader(void* ctxPtr, void* buffer, int64_t offset, int64_t size);
bool TryGetCoverFromXsFile(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetCoverBitmap(IStream* stream, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryReadFile(chm_file& chmFile, chm_entry& fileEntry, vector<char>& outFileContent);
bool TryLoadBitmap(const vector<char>& coverImage, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetCoverFromToc(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetFileContent(chm_file& chmFile, int& fileIndex, vector<char>& outFileContent, const function<bool(const string&)>& pathMatch);
bool TryGetCoverBitmap(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType, const function<bool(const string&)>& pathMatch);
bool TryGetCoverBitmap(IStream* stream, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetCoverFromImageTag(chm_file& chmFile, const Xml::Document& xml, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetCoverByHtmlPath(chm_file& chmFile, const string& htmlFilePath, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);

bool TryGetPathFromHhcObjects(chm_file& chmFile, const Xml::Document& hhcXml, const function<bool(const string&)>& isParamTagIndicatesCoverObject, const function<bool(const string&)>& isParamTagWithCoverPath, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetPathFromHhcObject(chm_file& chmFile, const Xml::Document& hhcObjectXml, const function<bool(const string&)>& isParamTagIndicatesCoverObject, const function<bool(const string&)>& isParamTagWithCoverPath, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
bool TryGetCoverFromHhc(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType);
void PreparePath(string& path);

vector<string> ImageFileExtensions
{
    ".bmp",
    ".ico",
    ".gif",
    ".jpg",
    ".jpe",
    ".jfif",
    ".jpeg",
    ".png",
    ".tif",
    ".tiff"
};

vector<string> HtmlExtensions
{
    ".html",
    ".htm"
};

vector<string> TocFileNames
{
    "toc.html",
    "toc.htm"
};

vector<string> OtherTocFileNames
{
    "cover.html",
    "cover.htm",
    "index.html",
    "index.htm",
};

int main()
{
    static_cast<void>(CoInitialize(nullptr));
    
    HBITMAP bitmap{ nullptr };
    WTS_ALPHATYPE alphaType{};

    string coverFileName{};
    vector<char> coverFileContent{};
    //const auto searchPath{ "C:\\TEMP\\CHM1" };
    //const auto imagePath{ L"C:\\TEMP\\CHM1\\IMG" };
    const auto searchPath{ "R:\\TEMP\\CHM" };
    const auto imagePath{ L"R:\\TEMP\\IMG" };
    for (const auto& entry : filesystem::directory_iterator(searchPath))
    {
        if (entry.is_directory())
            continue;

        const filesystem::path::value_type* filePath = entry.path().c_str();
    	if (!StrLib::EndsWith(wstring{ filePath }, wstring{L".chm"}))
            continue;
        
        IStream* fileStream{ nullptr };
        if (const auto result = SHCreateStreamOnFileW(filePath, STGM_READ | STGM_SHARE_DENY_WRITE, &fileStream); FAILED(result))
        {
            std::wcout << L"ST: " << filePath << endl;
            continue;
        }

        if (!TryGetCoverBitmap(fileStream, bitmap, alphaType))
        {
            std::wcout << L"NK: " << filePath << endl;
            //std::wcout << L"Failed get the cover for: " << filePath << endl;
            continue;
        }

        std::wstring imageFileName{ imagePath };
        imageFileName.append(L"\\");
        imageFileName.append(entry.path().filename().c_str());        
        imageFileName.append(L".png");
        if (!SaveImage(bitmap, imageFileName, Gfx::ImageFileType::Png))
            std::wcout << L"Failed to save: " << imageFileName << endl;        
        
        DeleteObject(bitmap);

        std::wcout << L"OK: " << filePath << endl;
        //std::wcout << L"Cover obtained for: " << filePath << endl;
        //std::wcout << filePath << endl;

        //fileStream->Release();
    }

    //parsingResult = extract(&chmFile, base_path);
    //chm_close(&chmFile);
    //fd_reader_close(&ctx);
    //std::cout << "Path: '" << entry->path << "';" << " Length: " << entry->length << "; Space: " << entry->space << "; Flags: " << entry->flags << std::endl;

    
    
    CoUninitialize();
    
    //return !chmFile->parse_entries_failed;

    //printf("%s:\n", path);
    //parsingResult = extract(&chmFile, base_path);
    //chm_close(&chmFile);
    //fd_reader_close(&ctx);
    
    std::cout << "Done.\n";
    return 0;
}

bool TryGetCoverBitmap(IStream *stream, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    chm_file chmFile;
    IStreamReaderCtx ctx{ stream };
    if (!chm_parse(&chmFile, IStreamReader, &ctx))
        return false;

	// Find an image file with the name that ends with _xs, those files are usually the cover images
    if (TryGetCoverFromXsFile(chmFile, outBitmap, outAlphaType))
        return true;

    // Locate toc.html/toc.htm file get the first image that looks like a cover
    if (TryGetCoverFromToc(chmFile, outBitmap, outAlphaType))
        return true;

    // Read HHC objects and check if there is an object with a cover, load referenced covet file
    if (TryGetCoverFromHhc(chmFile, outBitmap, outAlphaType))
        return true;

    return false;
}

bool TryGetCoverFromXsFile(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    return TryGetCoverBitmap(chmFile, outBitmap, outAlphaType, [&](const string& path) -> bool
    {
        if (!StrLib::EndsWith(path, ImageFileExtensions))
            return false;
        const auto extensionOffset = path.find_last_of('.');
        const auto pathWithoutExtension = extensionOffset != string::npos ? path.substr(0, extensionOffset) : path;
        return StrLib::EndsWith(pathWithoutExtension, std::string{ "_xs" });
    });
}

bool TryGetCoverFromToc(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    int fileIndex{ 0 };
    vector<char> tocFileContent{};

    if (!TryGetFileContent(chmFile, fileIndex, tocFileContent, [&](const string& path) -> bool { return StrLib::EndsWith(path, TocFileNames); }) &&
        !TryGetFileContent(chmFile, fileIndex, tocFileContent, [&](const string& path) -> bool { return StrLib::EndsWith(path, OtherTocFileNames); }))
        return false;

    const Xml::Document tocXml{ string {tocFileContent} };
    tocFileContent.clear();

    return TryGetCoverFromImageTag(chmFile, tocXml, outBitmap, outAlphaType);
}

bool TryGetCoverFromHhc(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
	const string hhcExtension{ ".hhc" };

    auto isParamTagIndicatesNameObject = [](const string& paramTag) -> bool
    {
        string attributeValue{};
        return Xml::Document::GetTagAttribute(paramTag, "name", attributeValue) &&
            StrLib::EqualsCi(attributeValue, string{ "Name" });
    };

    auto isParamTagWithCoverPath = [](const string& paramTag) -> bool
    {
        string attributeValue{};
        return Xml::Document::GetTagAttribute(paramTag, "name", attributeValue) &&
            StrLib::EqualsCi(attributeValue, string{ "Local" });
    };

    vector<char> hhcFileContent{};
    for (int fileIndex{ 0 }; TryGetFileContent(chmFile, fileIndex, hhcFileContent, [&](const string& path) -> bool { return StrLib::EndsWith(path, hhcExtension); }); ++fileIndex)
    {
        const Xml::Document hhcXml{ string {hhcFileContent} };
        hhcFileContent.clear();
        if (TryGetPathFromHhcObjects(chmFile, hhcXml, isParamTagIndicatesNameObject, isParamTagWithCoverPath, outBitmap, outAlphaType))
			return true;        
    }

    return false;
}

bool TryGetPathFromHhcObjects(chm_file& chmFile, const Xml::Document& hhcXml,
    const function<bool(const string&)>& isParamTagIndicatesCoverObject,
    const function<bool(const string&)>& isParamTagWithCoverPath,
    HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    string elementContent;
    const string objectElementName{ "object" };
    for (size_t elementSearchOffset = 0, contentOffset = 0; hhcXml.GetElementContent("object", elementSearchOffset, elementContent, &contentOffset); elementSearchOffset += elementContent.size())
    {
        elementSearchOffset = contentOffset;
        const Xml::Document hhcObjectXml{ string {elementContent} }; //CAUTION! Do not inline! Possible bug in VS2022.
        if (TryGetPathFromHhcObject(chmFile, hhcObjectXml, isParamTagIndicatesCoverObject, isParamTagWithCoverPath, outBitmap, outAlphaType))
            return true;
    }

    return false;
}

bool TryGetPathFromHhcObject(chm_file& chmFile, const Xml::Document& hhcObjectXml, 
    const function<bool(const string&)>& isParamTagIndicatesCoverObject,
    const function<bool(const string&)>& isParamTagWithCoverPath,
    HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    string path, tag;
    vector<char> coverFileContent{};

    if (!hhcObjectXml.ContainsTag("param", isParamTagIndicatesCoverObject) ||
        !hhcObjectXml.GetTag("param", tag, isParamTagWithCoverPath) ||
        !Xml::Document::GetTagAttribute(tag, "value", path))
        return false;

    PreparePath(path);

    if (StrLib::EndsWith(path, HtmlExtensions))
        return TryGetCoverByHtmlPath(chmFile, path, outBitmap, outAlphaType);

	if (StrLib::EndsWith(path, ImageFileExtensions))
		return TryGetCoverBitmap(chmFile, outBitmap, outAlphaType, [&](const string& filePath) -> bool { return StrLib::EndsWith(filePath, path); });

    return false;
}

bool TryGetCoverByHtmlPath(chm_file& chmFile, const string& htmlFilePath, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    int fileIndex{ 0 };
    vector<char> xmlFileContent{};
    if (!TryGetFileContent(chmFile, fileIndex, xmlFileContent, [&](const string& htmlPath) -> bool { return StrLib::EndsWith(htmlPath, htmlFilePath); }))
        return false;

    const Xml::Document xml{ string {xmlFileContent} };
    xmlFileContent.clear();

    return TryGetCoverFromImageTag(chmFile, xml, outBitmap, outAlphaType);
}

bool TryGetCoverFromImageTag(chm_file& chmFile, const Xml::Document& xml, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    string tag, srcValue;
    vector<char> coverFileContent{};
    for (size_t offset = 0, tagOffset = 0; xml.GetTag("img", offset, tagOffset, tag); offset += tag.size())
    {
        offset = tagOffset;
        if (!Xml::Document::GetTagAttribute(tag, "src", srcValue))
            continue;

        PreparePath(srcValue);
        if (StrLib::EndsWith(srcValue, ImageFileExtensions) &&
            TryGetCoverBitmap(chmFile, outBitmap, outAlphaType, [&](const string& path) -> bool { return StrLib::EndsWith(path, srcValue); }))
            return true;
    }

    return false;
}

void PreparePath(string& path)
{
    StrLib::UnEscapeXml(path);
    StrLib::TrimStartCi(path, '.');
    StrLib::ReplaceAll(path, '\\', '/');
    StrLib::TrimStartCi(path, string{ "/.." });

    const auto anchorOffset = path.find_last_of('#');
    if (anchorOffset == string::npos)
        return;
    path.resize(anchorOffset);        
}

bool TryGetCoverBitmap(chm_file& chmFile, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType, const function<bool(const string&)>& pathMatch)
{
    vector<char> coverImage{};
    for (int fileIndex{ 0 }; TryGetFileContent(chmFile, fileIndex, coverImage, pathMatch); fileIndex++)
    {
        if (TryLoadBitmap(coverImage, outBitmap, outAlphaType))
            return true;
    }

    return false;
}

bool TryGetFileContent(chm_file& chmFile, int& fileIndex, vector<char>& outFileContent, const function<bool(const string&)>& pathMatch)
{
    for (int entryIndex = fileIndex; entryIndex < chmFile.n_entries; entryIndex++)
    {
        if (const auto entry = chmFile.entries[entryIndex];
            !pathMatch(string{ entry->path }) || 
            !TryReadFile(chmFile, *entry, outFileContent))
	        continue;
        fileIndex = entryIndex;
        return true;
    }

    return false;
}

bool TryLoadBitmap(const vector<char>& coverImage, HBITMAP& outBitmap, WTS_ALPHATYPE& outAlphaType)
{
    SIZE imageSize{};
    if (FAILED(Gfx::LoadImageToHBitmap(coverImage.data(), coverImage.size(), outBitmap, outAlphaType, imageSize)))
        return false;

    if (Gfx::ImageSizeSatisfiesCoverConstraints(imageSize))
        return true;

    DeleteObject(outBitmap);
    return false;
}

bool TryReadFile(chm_file& chmFile, chm_entry& fileEntry, vector<char>& outFileContent)
{
    const auto fileLength = fileEntry.length;

    try { outFileContent.resize(fileLength); }
    catch (...) { return false; }

    const auto readBytes = chm_retrieve_entry(&chmFile, &fileEntry, reinterpret_cast<uint8_t*>(outFileContent.data()), 0, fileLength);
    return readBytes == fileLength;
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


////////////////////////////////////////////////////////
/// UTILITY
///




/*

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

static bool extract_entry(chm_file* chmFile, chm_entry* chmEntry, const char* basePath);

static HRESULT FindEntry(chm_file* chmFile, const std::string& fileName)
{
    for (int entryIndex = 0; entryIndex < chmFile->n_entries; entryIndex++)
    {
        if (!extract_entry(chmFile, chmFile->entries[entryIndex], basePath))
            return false;
    }

    return !chmFile->parse_entries_failed;
}

static bool extract(chm_file* chmFile, const char* basePath)
{
    // extract as many entries as possible
    for (int entryIndex = 0; entryIndex < chmFile->n_entries; entryIndex++)
    {
        if (!extract_entry(chmFile, chmFile->entries[entryIndex], basePath))
            return false;
    }

    return !chmFile->parse_entries_failed;
}

static bool extract_entry(chm_file* chmFile, chm_entry* chmEntry, const char* basePath)
{
    int64_t pathLen;
    char buffer[32768];
    char* i;

    if (chmEntry->path[0] != '/')
        return true;

    // quick hack for security hole mentioned by Sven Tantau
    if (strstr(chmEntry->path, "/../") != NULL) {
        // fprintf(stderr, "Not extracting %s (dangerous path)\n", ui->path);
        return true;
    }

    if (snprintf(buffer, sizeof(buffer), "%s%s", basePath, chmEntry->path) > 1024) {
        return false;
    }

    // Get the length of the path
    pathLen = strlen(chmEntry->path) - 1;

    if (chmEntry->path[pathLen] == '/') {
        // this is directory
        return rmkdir(buffer) != -1;
    }

    // this is file
    FILE* fout;

    printf("--> %s\n", chmEntry->path);
    if ((fout = fopen(buffer, "wb")) == NULL) {
        // make sure that it isn't just a missing directory before we abort
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


*/

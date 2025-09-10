#pragma once

#include <unordered_map>
#include <fcntl.h>       
#include <unistd.h>      
#include <sys/stat.h>    
#include <sys/mman.h> 
#include <assert.h>

#include "../reactor/Buffer.h"
#include"../logger/log_fac.h"
class HttpResponse {
public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& srcDir, std::string& path, bool isKeepAlive = false, int code = -1);
    void MakeResponse(BufferBlock& buff);
    void UnmapFile();
    char* File();
    size_t FileLen() const;
    void ErrorContent(BufferBlock& buff, std::string message);
    int Code() const { return code_; }

private:
    void AddStateLine_(BufferBlock &buff);
    void AddHeader_(BufferBlock &buff);
    void AddContent_(BufferBlock &buff);

    void ErrorHtml_();
    std::string GetFileType_();

    int code_;
    bool isKeepAlive_;

    std::string path_;
    std::string srcDir_;
    
    char* mmFile_; 
    struct stat mmFileStat_;

    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    static const std::unordered_map<int, std::string> CODE_STATUS;
    static const std::unordered_map<int, std::string> CODE_PATH;
};

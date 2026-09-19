#include "ModelConfig.h"
#include <fstream>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <cctype>
namespace detectcore {
static std::string trim(std::string s){
    auto valid=[](unsigned char c){return !std::isspace(c);};
    auto b=std::find_if(s.begin(),s.end(),valid), e=std::find_if(s.rbegin(),s.rend(),valid).base();
    return b<e?std::string(b,e):std::string{};
}
ModelInfo readModelInfo(const std::filesystem::path& directory){
    if(!std::filesystem::is_directory(directory)) throw std::runtime_error("Không tìm thấy thư mục model");
    for(const auto& f:{"model.ncnn.param","model.ncnn.bin","detectcore.cfg","classes.txt"})
        if(!std::filesystem::is_regular_file(directory/f)) throw std::runtime_error("Thiếu file model: "+std::string(f));
    ModelInfo info; info.directory=directory;
    std::ifstream cfg(directory/"detectcore.cfg"); std::string line; std::map<std::string,std::string> props;
    while(std::getline(cfg,line)){
        if(line.empty()||line[0]=='#') continue;
        auto eq=line.find('='); if(eq==std::string::npos) throw std::runtime_error("detectcore.cfg có dòng không hợp lệ");
        std::string k=trim(line.substr(0,eq));
        if(!props.emplace(k,trim(line.substr(eq+1))).second) throw std::runtime_error("Trùng khóa cấu hình: "+k);
    }
    if(props.at("layout")!="raw_xywh") throw std::runtime_error("Chỉ hỗ trợ YOLO26 detection RAW xywh; không nhận NMS/end-to-end khác");
    info.inputName=props.at("input"); info.outputName=props.at("output");
    info.width=std::stoi(props.at("width")); info.height=std::stoi(props.at("height"));
    if(info.width<=0||info.height<=0||info.width>4096||info.height>4096 ||
       info.inputName.empty()||info.outputName.empty()) throw std::runtime_error("Model config không hợp lệ");
    std::ifstream names(directory/"classes.txt");
    while(std::getline(names,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()) throw std::runtime_error("classes.txt có lớp trống");
        info.names.push_back(line);
    }
    if(info.names.empty()||info.names.size()>10000) throw std::runtime_error("classes.txt không có lớp hợp lệ");
    return info;
}
}

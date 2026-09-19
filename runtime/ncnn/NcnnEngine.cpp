#include <detectcore/IInferenceEngine.h>
#include <net.h>
#include <fstream>
#include <cstdio>
#include <memory>
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace detectcore {
class NcnnEngine final : public IInferenceEngine {
    ncnn::Net net_;
    ModelInfo info_;
public:
    void load(const ModelInfo& info, int threads) override {
        net_.opt.use_vulkan_compute=false;
        net_.opt.num_threads=threads;
        // NCNN takes narrow filenames: utf8_to_local is not guaranteed on Windows.
        // Use NCNN's memory-based loading from a binary file opened via std::filesystem.
        std::ifstream pf(info.directory/"model.ncnn.param",std::ios::binary);
        if(!pf)throw std::runtime_error("Không mở được .param");
        std::string param((std::istreambuf_iterator<char>(pf)),{});
        if(param.empty())throw std::runtime_error("File NCNN param rỗng");
        const int rc=net_.load_param_mem(param.c_str());
        if(rc!=0)throw std::runtime_error("NCNN load_param_mem thất bại: "+std::to_string(rc));
        // FILE* avoids loading the whole model into RAM and supports native Unicode paths on Windows.
        const auto weights=info.directory/"model.ncnn.bin";
        std::FILE* fp=nullptr;
#ifdef _WIN32
        _wfopen_s(&fp,weights.c_str(),L"rb");
#else
        fp=std::fopen(weights.c_str(),"rb");
#endif
        if(!fp)throw std::runtime_error("Không mở được .bin");
        const auto closeFile=[](std::FILE* f){if(f)std::fclose(f);};
        std::unique_ptr<std::FILE,decltype(closeFile)> file(fp,closeFile);
        const int modelCode=net_.load_model(file.get());
        if(modelCode!=0)throw std::runtime_error("NCNN load_model thất bại: "+std::to_string(modelCode));
        auto inputs=net_.input_names(),outputs=net_.output_names();
        if(std::find(inputs.begin(),inputs.end(),info.inputName)==inputs.end() ||
           std::find(outputs.begin(),outputs.end(),info.outputName)==outputs.end())
            throw std::runtime_error("Tensor names khác model param, hãy tạo lại detectcore.cfg");
        if(outputs.size()!=1) throw std::runtime_error("MVP chỉ hỗ trợ model một output (detection only)");
        info_=info;
    }
    Tensor infer(const cv::Mat& rgb) override {
        if(rgb.empty()||rgb.type()!=CV_8UC3||rgb.cols!=info_.width||rgb.rows!=info_.height)
            throw std::invalid_argument("NCNN input không đúng RGB 8-bit/kích thước");
        ncnn::Mat input=ncnn::Mat::from_pixels(rgb.data,ncnn::Mat::PIXEL_RGB,rgb.cols,rgb.rows);
        if(input.empty())throw std::runtime_error("Không cấp phát được NCNN input");
        const float norm[3]={1.f/255.f,1.f/255.f,1.f/255.f};
        input.substract_mean_normalize(nullptr,norm);
        auto ex=net_.create_extractor();
        int rc=ex.input(info_.inputName.c_str(),input);
        if(rc!=0)throw std::runtime_error("NCNN input thất bại: "+std::to_string(rc));
        ncnn::Mat output;
        rc=ex.extract(info_.outputName.c_str(),output);
        if(rc!=0||output.empty()) throw std::runtime_error("NCNN output thất bại: "+std::to_string(rc));
        // NCNN stores w as innermost axis, h as next, c as outermost.
        // Ultralytics raw output is logical [4+nc, candidate_count] after batch removal.
        if(output.elemsize!=sizeof(float) || (output.dims!=2 && !(output.dims==3 && output.c==1)))
            throw std::runtime_error("Output tensor không hỗ trợ: dims="+std::to_string(output.dims)+
               " c="+std::to_string(output.c)+" elemsize="+std::to_string(output.elemsize));
        const int rows=output.h, cols=output.w;
        if(rows!=static_cast<int>(info_.names.size())+4 || cols<1 || cols>300000)
            throw std::runtime_error("Output shape không đúng RAW [4+nc,N]: h="+std::to_string(rows)+
                " w="+std::to_string(cols)+" nc="+std::to_string(info_.names.size()));
        Tensor raw;raw.rows=rows;raw.cols=cols;raw.values.resize(static_cast<size_t>(rows)*cols);
        ncnn::Mat mat=(output.dims==3?output.channel(0):output);
        for(int r=0;r<rows;++r){
            const float* src=mat.row(r);
            std::copy_n(src,cols,raw.values.data()+static_cast<size_t>(r)*cols);
        }
        return raw;
    }
};
std::unique_ptr<IInferenceEngine> createNcnnEngine(){return std::make_unique<NcnnEngine>();}
}

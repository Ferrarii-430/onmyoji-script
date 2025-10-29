#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <QString>

struct Detection {
    cv::Rect bbox;
    float confidence;
    int class_id;
    QString className;
};

class YOLODetector {
public:
    // 删除拷贝构造函数和赋值操作符
    YOLODetector(const YOLODetector&) = delete;
    YOLODetector& operator=(const YOLODetector&) = delete;

    // 获取单例实例
    static YOLODetector& getInstance() {
        static YOLODetector instance;
        return instance;
    }

    // 初始化模型（只需调用一次）
    bool initialize(const std::wstring& model_path);

    // 检测方法
    std::vector<Detection> detect(const cv::Mat& image, double nms_threshold = 0.55f);
    std::vector<Detection> nonMaximumSuppression(const std::vector<Detection>& detections, double iou_threshold);

private:
    // 私有构造函数
    YOLODetector() = default;

    std::unique_ptr<Ort::Env> env;
    std::unique_ptr<Ort::Session> session;
    std::string input_name;
    std::string output_name;
    std::vector<int64_t> input_shape;
    std::vector<int64_t> output_shape;
    bool is_initialized = false;
    std::mutex detect_mutex;  // 线程安全
};

#endif // YOLODETECTOR_H

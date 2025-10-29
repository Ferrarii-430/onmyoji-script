#include "src/widget/main/mainwindow.h"
#include "src/widget/main/ui_mainwindow.h"
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/core/utils/logger.hpp>
#include <src/include/onnxruntime_cxx_api.h>

#include "ConfigManager.h"
#include "src/utils/ClassNameCache.h"

struct Detection {
    cv::Rect bbox;
    float confidence;
    int class_id;
};

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    switch (type) {
    case QtDebugMsg:
        std::cout << msg.toUtf8().constData() << std::endl;
        break;
    case QtWarningMsg:
        std::cout << "[Warning] " << msg.toUtf8().constData() << std::endl;
        break;
    case QtCriticalMsg:
        std::cerr << "[Critical] " << msg.toUtf8().constData() << std::endl;
        break;
    case QtFatalMsg:
        std::cerr << "[Fatal] " << msg.toUtf8().constData() << std::endl;
        abort();
    }
}

// 非极大值抑制 (NMS)
std::vector<Detection> nonMaximumSuppression(const std::vector<Detection>& detections, float iou_threshold) {
    std::vector<Detection> result;
    std::vector<bool> suppressed(detections.size(), false);

    // 按置信度降序排序
    std::vector<size_t> indices(detections.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
        return detections[i].confidence > detections[j].confidence;
    });

    for (size_t i = 0; i < indices.size(); ++i) {
        if (suppressed[indices[i]]) continue;

        result.push_back(detections[indices[i]]);

        for (size_t j = i + 1; j < indices.size(); ++j) {
            if (suppressed[indices[j]]) continue;

            const Detection& det1 = detections[indices[i]];
            const Detection& det2 = detections[indices[j]];

            // 计算IoU
            cv::Rect intersection = det1.bbox & det2.bbox;
            float intersection_area = intersection.area();
            float union_area = det1.bbox.area() + det2.bbox.area() - intersection_area;
            float iou = intersection_area / union_area;

            if (iou > iou_threshold) {
                suppressed[indices[j]] = true;
            }
        }
    }

    return result;
}

void detectOnmyoji() {
    try {
        // 初始化ONNX Runtime环境
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "OnmyojiDetector");
        Ort::SessionOptions session_options;

        // 设置线程数（可选）
        session_options.SetIntraOpNumThreads(1);
        session_options.SetInterOpNumThreads(1);

        // 加载ONNX模型
        std::wstring model_path = L"D:\\CppProduct\\qtTest\\cmake-build-gt-cmake-692\\src\\resource\\onmyoji-yolo-v5.onnx";
        Ort::Session session(env, model_path.c_str(), session_options);

        std::cout << "ONNX Runtime initialized successfully!" << std::endl;
        std::cout << "ONNX Runtime version: " << OrtGetApiBase()->GetVersionString() << std::endl;

        // 获取模型信息
        Ort::AllocatorWithDefaultOptions allocator;

        // 输入信息
        auto input_name = session.GetInputNameAllocated(0, allocator);
        std::string input_name_str = input_name.get();
        auto input_type_info = session.GetInputTypeInfo(0);
        auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
        auto input_shape = input_tensor_info.GetShape();

        std::cout << "Input name: " << input_name_str << std::endl;
        std::cout << "Input shape: ";
        for (auto dim : input_shape) {
            std::cout << dim << " ";
        }
        std::cout << std::endl;

        // 输出信息
        auto output_name = session.GetOutputNameAllocated(0, allocator);
        std::string output_name_str = output_name.get();
        auto output_type_info = session.GetOutputTypeInfo(0);
        auto output_tensor_info = output_type_info.GetTensorTypeAndShapeInfo();
        auto output_shape = output_tensor_info.GetShape();

        std::cout << "Output name: " << output_name_str << std::endl;
        std::cout << "Output shape: ";
        for (auto dim : output_shape) {
            std::cout << dim << " ";
        }
        std::cout << std::endl;

        // 读取输入图像
        cv::Mat image = cv::imread("D:\\CppProduct\\qtTest\\cmake-build-gt-cmake-692\\src\\resource\\thumbnail\\debug_capture_result.png");
        if (image.empty()) {
            std::cout << "Failed to load image!" << std::endl;
            return;
        }

        cv::Mat original_image = image.clone();

        // 图像预处理 - 与YOLOv5训练时保持一致
        cv::Mat resized_image, float_image;
        int input_width = 640;  // YOLOv5标准输入尺寸
        int input_height = 640;

        // 保持宽高比的resize
        float scale = std::min(static_cast<float>(input_width) / image.cols,
                              static_cast<float>(input_height) / image.rows);
        int new_width = static_cast<int>(image.cols * scale);
        int new_height = static_cast<int>(image.rows * scale);

        cv::resize(image, resized_image, cv::Size(new_width, new_height));

        // 创建填充后的图像
        cv::Mat padded_image = cv::Mat::zeros(input_height, input_width, CV_8UC3);
        cv::Scalar padding_color(114, 114, 114);  // YOLOv5使用的灰色填充

        int dx = (input_width - new_width) / 2;
        int dy = (input_height - new_height) / 2;
        resized_image.copyTo(padded_image(cv::Rect(dx, dy, new_width, new_height)));

        // 转换为float并归一化
        padded_image.convertTo(float_image, CV_32F, 1.0 / 255.0);

        // 转换颜色通道 BGR -> RGB
        cv::cvtColor(float_image, float_image, cv::COLOR_BGR2RGB);

        // 将图像数据转换为模型输入格式 (HWC -> CHW)
        std::vector<float> input_tensor_values(input_width * input_height * 3);
        std::vector<cv::Mat> channels(3);
        cv::split(float_image, channels);

        size_t channel_size = input_width * input_height;
        for (int i = 0; i < 3; ++i) {
            memcpy(input_tensor_values.data() + i * channel_size,
                   channels[i].data, channel_size * sizeof(float));
        }

        // 准备输入张量
        std::vector<int64_t> input_shape_vec = {1, 3, input_height, input_width};
        auto memory_info = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            memory_info,
            input_tensor_values.data(),
            input_tensor_values.size(),
            input_shape_vec.data(),
            input_shape_vec.size()
        );

        // 准备输入输出名称
        std::vector<const char*> input_names = {input_name_str.c_str()};
        std::vector<const char*> output_names = {output_name_str.c_str()};

        // 运行推理
        auto output_tensors = session.Run(
            Ort::RunOptions{nullptr},
            input_names.data(),
            &input_tensor,
            1,
            output_names.data(),
            1
        );

        // 获取输出数据
        float* output_data = output_tensors[0].GetTensorMutableData<float>();
        auto output_shape_info = output_tensors[0].GetTensorTypeAndShapeInfo();
        auto output_shape_vec = output_shape_info.GetShape();

        // YOLOv5输出格式通常是 [1, 25200, 85]
        // 85 = 4(bbox) + 1(objectness) + 80(class probabilities)
        std::vector<Detection> raw_detections;

        if (output_shape_vec.size() == 3 && output_shape_vec[1] == 25200) {
            int num_detections = output_shape_vec[1];
            int num_features = output_shape_vec[2];  // 应该是74
            int num_classes = num_features - 5;      // 74 - 5 = 69个类别

            std::cout << "Processing " << num_detections << " detections with "
                      << num_classes << " custom classes" << std::endl;

            for (int i = 0; i < num_detections; ++i) {
                float* detection = output_data + i * num_features;
                float objectness = detection[4];  // 对象置信度

                // 使用示例中的置信度阈值 0.4
                if (objectness > 0.4f) {
                    // 找到最大类别概率
                    int class_id = std::max_element(detection + 5, detection + 5 + num_classes) - (detection + 5);
                    float class_confidence = detection[5 + class_id];
                    float total_confidence = objectness * class_confidence;

                    // 再次使用置信度阈值
                    if (total_confidence > 0.4f) {
                        // 解析边界框坐标 (相对于640x640)
                        float center_x = detection[0];
                        float center_y = detection[1];
                        float width = detection[2];
                        float height = detection[3];

                        // 转换为原始图像坐标
                        float x1 = (center_x - width / 2 - dx) / scale;
                        float y1 = (center_y - height / 2 - dy) / scale;
                        float x2 = (center_x + width / 2 - dx) / scale;
                        float y2 = (center_y + height / 2 - dy) / scale;

                        // 确保坐标在图像范围内
                        x1 = std::max(0.0f, std::min(x1, static_cast<float>(original_image.cols)));
                        y1 = std::max(0.0f, std::min(y1, static_cast<float>(original_image.rows)));
                        x2 = std::max(0.0f, std::min(x2, static_cast<float>(original_image.cols)));
                        y2 = std::max(0.0f, std::min(y2, static_cast<float>(original_image.rows)));

                        Detection det;
                        det.bbox = cv::Rect(
                            static_cast<int>(x1),
                            static_cast<int>(y1),
                            static_cast<int>(x2 - x1),
                            static_cast<int>(y2 - y1)
                        );
                        det.confidence = total_confidence;
                        det.class_id = class_id;

                        raw_detections.push_back(det);
                    }
                }
            }
        }

        std::cout << "Raw detections before NMS: " << raw_detections.size() << std::endl;

        // 应用非极大值抑制 (使用示例中的IoU阈值 0.55)
        std::vector<Detection> final_detections = nonMaximumSuppression(raw_detections, 0.55f);

        std::cout << "Final detections after NMS: " << final_detections.size() << std::endl;

        // 在图像上绘制检测结果
        for (const auto& det : final_detections) {
            QString labelName = ClassNameCache::getClassName(det.class_id);
            cv::rectangle(original_image, det.bbox, cv::Scalar(0, 255, 0), 2);
            std::string label = labelName.toStdString();
            // std::string label = "Class " + labelName.toStdString() +
                               // " Conf: " + std::to_string(det.confidence);
            cv::putText(original_image, label, cv::Point(det.bbox.x, det.bbox.y - 10),
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
        }

        // 保存结果
        cv::imwrite("onmyoji_detection_result.png", original_image);
        std::cout << "Detection completed! Results saved to onmyoji_detection_result.png" << std::endl;

    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Standard exception: " << e.what() << std::endl;
    }
}

int main(int argc, char* argv[])
{

    // 禁用OpenCV的优化（包括SIMD指令）
    cv::setUseOptimized(false);

    // 设置OpenCV日志级别（可选）
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    qInstallMessageHandler(myMessageHandler); // 重定向所有 qDebug/qWarning

    QApplication a(argc, argv);
    mainwindow w;
    w.show();

    // QString path = ConfigManager::instance().classesNamePath();
    // ClassNameCache::initialize(path);
    // detectOnmyoji();

    return QApplication::exec();
}